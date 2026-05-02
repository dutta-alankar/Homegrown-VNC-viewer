#pragma once

#include <QObject>
#include <QImage>
#include <QSize>

#include <atomic>
#include <mutex>
#include <memory>
#include <thread>

extern "C" {
#include <rfb/rfbclient.h>
}

class VncClientSession : public QObject {
    Q_OBJECT

public:
    explicit VncClientSession(QObject* parent = nullptr);
    ~VncClientSession() override;

    void start(const QString& host, int port, const QString& password);
    void stop();

public slots:
    void sendPointerEvent(int x, int y, int buttonMask);
    void sendKeyEvent(quint32 keySym, bool down);
    void sendClipboardText(const QString& text);

signals:
    void connected();
    void disconnected(const QString& reason);
    void frameUpdated(const QImage& image);
    void remoteClipboardText(const QString& text);
    void framebufferResized(const QSize& size);

private:
    static rfbBool onMallocFrameBuffer(rfbClient* client);
    static void onGotFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h);
    static void onFinishedFrameBufferUpdate(rfbClient* client);
    static void onGotXCutText(rfbClient* client, const char* text, int len);
    static char* onGetPassword(rfbClient* client);

    void runWorker(QString host, int port, QString password);
    void emitFrame();

    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};

    rfbClient* m_client = nullptr;
    QString m_password;
    std::mutex m_clientMutex;
};
