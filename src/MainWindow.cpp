#include "MainWindow.h"

#include "ConnectionDialog.h"
#include "ConnectionStore.h"
#include "SshTunnel.h"
#include "VncClientSession.h"
#include "VncViewerWidget.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_tunnel(new SshTunnel(this)) {
    setWindowTitle("VNC Client");
    resize(1200, 800);

    m_profiles = ConnectionStore::load();

    m_session = new VncClientSession(this);
    m_clipboard = QApplication::clipboard();

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);

    m_stack = new QStackedWidget(root);

    auto* homePage = new QWidget(m_stack);
    auto* homeLayout = new QVBoxLayout(homePage);

    auto* buttonsRow = new QHBoxLayout();
    auto* addBtn = new QPushButton("Add", homePage);
    auto* editBtn = new QPushButton("Edit", homePage);
    auto* removeBtn = new QPushButton("Remove", homePage);
    m_connectButton = new QPushButton("Connect", homePage);
    m_searchEdit = new QLineEdit(homePage);
    m_searchEdit->setPlaceholderText("Search profiles by name or host...");

    buttonsRow->addWidget(addBtn);
    buttonsRow->addWidget(editBtn);
    buttonsRow->addWidget(removeBtn);
    buttonsRow->addStretch(1);
    buttonsRow->addWidget(m_connectButton);

    m_profileList = new QListWidget(homePage);
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileList->setIconSize(QSize(160, 100));

    homeLayout->addLayout(buttonsRow);
    homeLayout->addWidget(m_searchEdit);
    homeLayout->addWidget(m_profileList);

    auto* viewerPage = new QWidget(m_stack);
    auto* viewerLayout = new QVBoxLayout(viewerPage);
    auto* viewerTop = new QHBoxLayout();
    m_disconnectButton = new QPushButton("Disconnect", viewerPage);
    m_statusLabel = new QLabel("Disconnected", viewerPage);
    viewerTop->addWidget(m_disconnectButton);
    viewerTop->addStretch(1);
    viewerTop->addWidget(m_statusLabel);

    m_viewer = new VncViewerWidget(viewerPage);

    viewerLayout->addLayout(viewerTop);
    viewerLayout->addWidget(m_viewer, 1);

    m_stack->addWidget(homePage);
    m_stack->addWidget(viewerPage);
    m_stack->setCurrentIndex(0);

    rootLayout->addWidget(m_stack);
    setCentralWidget(root);

    reloadList();

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addProfile);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editProfile);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeProfile);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectSelected);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectCurrent);
    connect(m_profileList, &QListWidget::itemDoubleClicked, this, [this]() { connectSelected(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::applySearchFilter);

    connect(m_session, &VncClientSession::frameUpdated, m_viewer, &VncViewerWidget::setFrame);
    connect(m_session, &VncClientSession::frameUpdated, this, &MainWindow::onFrameUpdated);
    connect(m_session, &VncClientSession::framebufferResized, m_viewer, &VncViewerWidget::setRemoteSize);
    connect(m_session, &VncClientSession::remoteClipboardText, this, &MainWindow::onRemoteClipboardChanged);
    connect(m_session, &VncClientSession::disconnected, this, &MainWindow::onConnectionClosed);
    connect(m_session, &VncClientSession::connected, this, [this]() {
        m_statusLabel->setText("Connected");
        m_connectionElapsed.restart();
    });

    connect(m_viewer, &VncViewerWidget::pointerEvent, m_session, &VncClientSession::sendPointerEvent);
    connect(m_viewer, &VncViewerWidget::keyEvent, m_session, &VncClientSession::sendKeyEvent);

    connect(m_clipboard, &QClipboard::dataChanged, this, &MainWindow::onLocalClipboardChanged);
}

MainWindow::~MainWindow() {
    disconnectCurrent();
}

void MainWindow::addProfile() {
    ConnectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto profile = dialog.profile();
    QString error;
    if (!validateProfile(profile, &error)) {
        QMessageBox::warning(this, "Invalid profile", error);
        return;
    }

    m_profiles.append(profile);
    saveProfiles();
    reloadList();
}

void MainWindow::editProfile() {
    const int idx = currentProfileIndex();
    if (idx < 0 || idx >= m_profiles.size()) {
        return;
    }

    ConnectionDialog dialog(this);
    dialog.setProfile(m_profiles.at(idx));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto profile = dialog.profile();
    QString error;
    if (!validateProfile(profile, &error)) {
        QMessageBox::warning(this, "Invalid profile", error);
        return;
    }

    m_profiles[idx] = profile;
    saveProfiles();
    reloadList();
    const int visibleRow = m_visibleProfileIndices.indexOf(idx);
    if (visibleRow >= 0) {
        m_profileList->setCurrentRow(visibleRow);
    }
}

void MainWindow::removeProfile() {
    const int idx = currentProfileIndex();
    if (idx < 0 || idx >= m_profiles.size()) {
        return;
    }
    m_profiles.removeAt(idx);
    saveProfiles();
    reloadList();
}

void MainWindow::connectSelected() {
    const int idx = currentProfileIndex();
    if (idx < 0 || idx >= m_profiles.size()) {
        return;
    }

    disconnectCurrent();

    auto& p = m_profiles[idx];
    QString error;
    if (!validateProfile(p, &error)) {
        QMessageBox::warning(this, "Invalid profile", error);
        return;
    }

    QString host = p.vncHost;
    int port = p.vncPort;

    if (p.useSshTunnel) {
        const int localPort = pickFreeLocalPort();
        if (localPort <= 0) {
            QMessageBox::warning(this, "Tunnel failed", "Unable to pick a free local port for SSH forwarding.");
            return;
        }

        if (!m_tunnel->start(p.sshUser,
                     p.sshAuthMode,
                     p.sshPassword,
                     QString(),
                     p.sshPrivateKeyPath,
                             p.gatewayHost,
                             p.gatewaySshPort,
                             localPort,
                             p.tunneledVncHost,
                             p.tunneledVncPort,
                             &error)) {
            QMessageBox::warning(this, "Tunnel failed", error);
            return;
        }

        host = "127.0.0.1";
        port = localPort;
    }

    m_statusLabel->setText(QString("Connecting to %1:%2...").arg(host).arg(port));
    m_activeProfileIndex = idx;
    m_thumbnailCapturedForSession = false;
    m_stack->setCurrentIndex(1);
    m_viewer->setFocus();
    m_session->start(host, port, p.vncPassword);
}

void MainWindow::disconnectCurrent() {
    m_session->stop();
    m_tunnel->stop();
    m_stack->setCurrentIndex(0);
    m_statusLabel->setText("Disconnected");
    m_activeProfileIndex = -1;
    m_thumbnailCapturedForSession = false;
    m_connectionElapsed.invalidate();
}

void MainWindow::onConnectionClosed(const QString& reason) {
    m_tunnel->stop();
    m_statusLabel->setText(reason);
    if (m_stack->currentIndex() != 0) {
        m_stack->setCurrentIndex(0);
        QMessageBox::information(this, "VNC connection", reason);
    }
}

void MainWindow::onRemoteClipboardChanged(const QString& text) {
    if (!m_clipboard) {
        return;
    }

    m_ignoreClipboardSignal = true;
    m_clipboard->setText(text);
    m_ignoreClipboardSignal = false;
}

void MainWindow::onLocalClipboardChanged() {
    if (m_ignoreClipboardSignal || !m_clipboard) {
        return;
    }

    m_session->sendClipboardText(m_clipboard->text());
}

void MainWindow::reloadList() {
    m_profileList->clear();
    m_visibleProfileIndices.clear();

    const QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();
    for (int i = 0; i < m_profiles.size(); ++i) {
        const auto& p = m_profiles.at(i);
        const QString searchable = QString("%1 %2 %3 %4")
            .arg(p.name, p.vncHost, p.gatewayHost, p.tunneledVncHost)
            .toLower();
        if (!filter.isEmpty() && !searchable.contains(filter)) {
            continue;
        }

        const QString subtitle = p.useSshTunnel
            ? QString("[SSH] %1:%2 via %3:%4").arg(p.tunneledVncHost).arg(p.tunneledVncPort).arg(p.gatewayHost).arg(p.gatewaySshPort)
            : QString("[Direct] %1:%2").arg(p.vncHost).arg(p.vncPort);

        auto* item = new QListWidgetItem(QString("%1\n    %2").arg(p.name, subtitle));
        if (!p.thumbnailPngBase64.isEmpty()) {
            const QByteArray png = QByteArray::fromBase64(p.thumbnailPngBase64.toUtf8());
            QPixmap pix;
            if (pix.loadFromData(png, "PNG")) {
                item->setIcon(QIcon(pix));
            }
        }

        m_profileList->addItem(item);
        m_visibleProfileIndices.append(i);
    }

    if (!m_visibleProfileIndices.isEmpty()) {
        m_profileList->setCurrentRow(0);
    }
}

void MainWindow::saveProfiles() {
    QString error;
    if (!ConnectionStore::save(m_profiles, &error)) {
        QMessageBox::warning(this, "Save failed", QString("Unable to save profiles: %1").arg(error));
    }
}

int MainWindow::currentProfileIndex() const {
    const int row = m_profileList->currentRow();
    if (row < 0 || row >= m_visibleProfileIndices.size()) {
        return -1;
    }
    return m_visibleProfileIndices.at(row);
}

bool MainWindow::validateProfile(const ConnectionProfile& p, QString* error) const {
    if (p.name.trimmed().isEmpty()) {
        if (error) {
            *error = "Profile name is required.";
        }
        return false;
    }

    if (p.vncHost.trimmed().isEmpty()) {
        if (error) {
            *error = "VNC server IP/host is required.";
        }
        return false;
    }

    if (p.useSshTunnel) {
        if (p.gatewayHost.trimmed().isEmpty()) {
            if (error) {
                *error = "Gateway IP/host is required for tunnel mode.";
            }
            return false;
        }
        if (p.tunneledVncHost.trimmed().isEmpty()) {
            if (error) {
                *error = "Remote VNC server IP/host is required for tunnel mode.";
            }
            return false;
        }
        if (p.sshAuthMode == "password" && p.sshPassword.isEmpty()) {
            if (error) {
                *error = "SSH password is required for password auth mode.";
            }
            return false;
        }
        if (p.sshAuthMode == "key" && p.sshPrivateKeyPath.trimmed().isEmpty()) {
            if (error) {
                *error = "SSH private key path is required for key auth mode.";
            }
            return false;
        }
    }

    return true;
}

int MainWindow::pickFreeLocalPort() const {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        ::close(fd);
        return -1;
    }

    const int port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

void MainWindow::onFrameUpdated(const QImage& image) {
    if (m_activeProfileIndex < 0 || m_activeProfileIndex >= m_profiles.size() || m_thumbnailCapturedForSession || image.isNull()) {
        return;
    }

    if (!m_connectionElapsed.isValid() || m_connectionElapsed.elapsed() < 30000) {
        return;
    }

    updateProfileThumbnail(m_activeProfileIndex, image);
    m_thumbnailCapturedForSession = true;
}

void MainWindow::applySearchFilter() {
    reloadList();
}

void MainWindow::updateProfileThumbnail(int profileIndex, const QImage& image) {
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) {
        return;
    }

    const QImage thumb = image.scaled(160, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    thumb.save(&buffer, "PNG");

    m_profiles[profileIndex].thumbnailPngBase64 = QString::fromUtf8(bytes.toBase64());
    saveProfiles();
    reloadList();
}
