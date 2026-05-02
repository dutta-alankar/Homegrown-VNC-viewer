#include "VncClientSession.h"

#include <QImage>

#include <algorithm>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <rfb/keysym.h>
#include <rfb/rfbclient.h>
}

namespace {
int kClientTag = 0;

static inline uint32_t readPixelValue(const uint8_t* p, int bytesPerPixel, bool bigEndian) {
    switch (bytesPerPixel) {
    case 4:
        if (bigEndian) {
            return (static_cast<uint32_t>(p[0]) << 24) |
                   (static_cast<uint32_t>(p[1]) << 16) |
                   (static_cast<uint32_t>(p[2]) << 8) |
                   static_cast<uint32_t>(p[3]);
        }
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    case 3:
        if (bigEndian) {
            return (static_cast<uint32_t>(p[0]) << 16) |
                   (static_cast<uint32_t>(p[1]) << 8) |
                   static_cast<uint32_t>(p[2]);
        }
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16);
    case 2:
        if (bigEndian) {
            return (static_cast<uint32_t>(p[0]) << 8) |
                   static_cast<uint32_t>(p[1]);
        }
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8);
    case 1:
        return static_cast<uint32_t>(p[0]);
    default:
        return 0;
    }
}

static inline int scaleChannel(uint32_t v, uint32_t max) {
    if (max == 0) {
        return 0;
    }
    return static_cast<int>((v * 255u + (max / 2u)) / max);
}
} // namespace

VncClientSession::VncClientSession(QObject* parent)
    : QObject(parent) {}

VncClientSession::~VncClientSession() {
    stop();
}

void VncClientSession::start(const QString& host, int port, const QString& password) {
    stop();

    m_running = true;
    m_thread = std::make_unique<std::thread>([this, host, port, password]() {
        runWorker(host, port, password);
    });
}

void VncClientSession::stop() {
    m_running = false;

    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
    m_thread.reset();
    m_connected = false;
}

void VncClientSession::sendPointerEvent(int x, int y, int buttonMask) {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (m_connected && m_client) {
        SendPointerEvent(m_client, x, y, buttonMask);
    }
}

void VncClientSession::sendKeyEvent(quint32 keySym, bool down) {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (m_connected && m_client) {
        SendKeyEvent(m_client, keySym, down ? 1 : 0);
    }
}

void VncClientSession::sendClipboardText(const QString& text) {
    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (m_connected && m_client) {
        QByteArray utf8 = text.toUtf8();
        SendClientCutText(m_client, utf8.data(), utf8.size());
    }
}

rfbBool VncClientSession::onMallocFrameBuffer(rfbClient* client) {
    auto* self = static_cast<VncClientSession*>(rfbClientGetClientData(client, &kClientTag));
    if (!self) {
        return FALSE;
    }

    const int bytesPerPixel = client->format.bitsPerPixel / 8;
    const size_t size = static_cast<size_t>(client->width) * static_cast<size_t>(client->height) * static_cast<size_t>(bytesPerPixel);
    if (size == 0) {
        return FALSE;
    }

    uint8_t* newBuffer = static_cast<uint8_t*>(std::malloc(size));
    if (!newBuffer) {
        return FALSE;
    }
    std::memset(newBuffer, 0, size);

    // Do not free client->frameBuffer here: ownership may not always be ours,
    // and freeing an unowned pointer can crash with malloc/free errors.
    client->frameBuffer = newBuffer;

    emit self->framebufferResized(QSize(client->width, client->height));
    return TRUE;
}

void VncClientSession::onGotFrameBufferUpdate(rfbClient* client, int, int, int, int) {
    // Frame emission is deferred until FinishedFrameBufferUpdate to avoid
    // copying the full framebuffer once per updated rectangle.
    (void)client;
}

void VncClientSession::onFinishedFrameBufferUpdate(rfbClient* client) {
    auto* self = static_cast<VncClientSession*>(rfbClientGetClientData(client, &kClientTag));
    if (!self) {
        return;
    }
    self->emitFrame();
}

void VncClientSession::onGotXCutText(rfbClient* client, const char* text, int len) {
    auto* self = static_cast<VncClientSession*>(rfbClientGetClientData(client, &kClientTag));
    if (!self) {
        return;
    }

    const QString clip = QString::fromUtf8(text, len);
    emit self->remoteClipboardText(clip);
}

char* VncClientSession::onGetPassword(rfbClient* client) {
    auto* self = static_cast<VncClientSession*>(rfbClientGetClientData(client, &kClientTag));
    if (!self) {
        return nullptr;
    }
    return strdup(self->m_password.toUtf8().constData());
}

void VncClientSession::runWorker(QString host, int port, QString password) {
    m_password = password;

    rfbClient* client = rfbGetClient(8, 3, 4);
    if (!client) {
        emit disconnected("Unable to initialize VNC client.");
        m_running = false;
        return;
    }

    rfbClientSetClientData(client, &kClientTag, this);
    client->MallocFrameBuffer = &VncClientSession::onMallocFrameBuffer;
    client->GotFrameBufferUpdate = &VncClientSession::onGotFrameBufferUpdate;
    client->FinishedFrameBufferUpdate = &VncClientSession::onFinishedFrameBufferUpdate;
    client->GotXCutText = &VncClientSession::onGotXCutText;
    client->GetPassword = &VncClientSession::onGetPassword;

    // Prefer fidelity over bandwidth for desktop usage to avoid JPEG artifacts.
    client->appData.enableJPEG = FALSE;
    client->appData.qualityLevel = 9;
    client->appData.compressLevel = 1;

    // Prefer classic VNC auth first to interoperate with common local servers
    // that advertise VeNCrypt but require extra credential callbacks.
    const uint32_t preferredAuthSchemes[] = {rfbVncAuth, rfbNoAuth, 0};
    SetClientAuthSchemes(client, preferredAuthSchemes, 2);

    client->serverHost = strdup(host.toUtf8().constData());
    client->serverPort = port;

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_client = client;
    }

    if (!rfbInitClient(client, nullptr, nullptr)) {
        emit disconnected("Connection failed. Check host/port/password and server reachability.");
        // rfbInitClient may already perform internal cleanup on failure.
        // Calling rfbClientCleanup again here can double-free in that path.
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_client = nullptr;
        m_running = false;
        return;
    }

    m_connected = true;
    emit connected();
    emitFrame();

    while (m_running && client) {
        const int waitRes = WaitForMessage(client, 50);
        if (waitRes < 0) {
            break;
        }
        if (waitRes > 0) {
            if (!HandleRFBServerMessage(client)) {
                break;
            }
        }
    }

    if (client) {
        rfbClientCleanup(client);
    }
    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_client = nullptr;
    }

    const bool wasConnected = m_connected.exchange(false);
    m_running = false;

    if (wasConnected) {
        emit disconnected("Disconnected.");
    }
}

void VncClientSession::emitFrame() {
    if (!m_client || !m_client->frameBuffer || m_client->width <= 0 || m_client->height <= 0) {
        return;
    }

    const int width = m_client->width;
    const int height = m_client->height;
    const rfbPixelFormat& fmt = m_client->format;
    const int bytesPerPixel = std::max(1, fmt.bitsPerPixel / 8);
    const bool bigEndian = fmt.bigEndian != 0;

    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull()) {
        return;
    }

    const uint8_t* src = reinterpret_cast<const uint8_t*>(m_client->frameBuffer);
    for (int y = 0; y < height; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        const uint8_t* srcLine = src + static_cast<size_t>(y) * static_cast<size_t>(width) * static_cast<size_t>(bytesPerPixel);
        for (int x = 0; x < width; ++x) {
            const uint8_t* px = srcLine + static_cast<size_t>(x) * static_cast<size_t>(bytesPerPixel);
            const uint32_t raw = readPixelValue(px, bytesPerPixel, bigEndian);

            const uint32_t rRaw = (raw >> fmt.redShift) & fmt.redMax;
            const uint32_t gRaw = (raw >> fmt.greenShift) & fmt.greenMax;
            const uint32_t bRaw = (raw >> fmt.blueShift) & fmt.blueMax;

            const int r = scaleChannel(rRaw, fmt.redMax);
            const int g = scaleChannel(gRaw, fmt.greenMax);
            const int b = scaleChannel(bRaw, fmt.blueMax);
            dstLine[x] = qRgb(r, g, b);
        }
    }

    emit frameUpdated(image);
}
