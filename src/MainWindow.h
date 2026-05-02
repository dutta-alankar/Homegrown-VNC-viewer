#pragma once

#include "ConnectionProfile.h"

#include <QMainWindow>

class QListWidget;
class QPushButton;
class QStackedWidget;
class QLabel;
class QClipboard;
class QLineEdit;
class QImage;

class SshTunnel;
class VncClientSession;
class VncViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void addProfile();
    void editProfile();
    void removeProfile();
    void connectSelected();
    void disconnectCurrent();

    void onConnectionClosed(const QString& reason);
    void onRemoteClipboardChanged(const QString& text);
    void onLocalClipboardChanged();
    void onFrameUpdated(const QImage& image);
    void applySearchFilter();

private:
    void reloadList();
    void saveProfiles();
    int currentProfileIndex() const;
    bool validateProfile(const ConnectionProfile& p, QString* error) const;
    int pickFreeLocalPort() const;
    void updateProfileThumbnail(int profileIndex, const QImage& image);

    QList<ConnectionProfile> m_profiles;
    QList<int> m_visibleProfileIndices;
    int m_activeProfileIndex = -1;
    bool m_thumbnailCapturedForSession = false;

    QListWidget* m_profileList = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_connectButton = nullptr;
    QPushButton* m_disconnectButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QStackedWidget* m_stack = nullptr;
    VncViewerWidget* m_viewer = nullptr;

    VncClientSession* m_session = nullptr;
    SshTunnel* m_tunnel = nullptr;

    bool m_ignoreClipboardSignal = false;
    QClipboard* m_clipboard = nullptr;
};
