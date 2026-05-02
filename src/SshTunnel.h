#pragma once

#include <QObject>
#include <QTemporaryFile>
#include <QProcess>

class SshTunnel : public QObject {
    Q_OBJECT

public:
    explicit SshTunnel(QObject* parent = nullptr);
    ~SshTunnel() override;

    bool start(const QString& sshUser,
               const QString& authMode,
               const QString& sshPassword,
               const QString& sshOtp,
               const QString& privateKeyPath,
               const QString& gatewayHost,
               int gatewayPort,
               int localPort,
               const QString& remoteHost,
               int remotePort,
               QString* error);

    void stop();
    bool isRunning() const;

private:
    QString createAskpassScript(const QString& password, const QString& otp, QString* error);
    bool waitForLocalForward(int localPort, QString* error);

    QProcess m_process;
    QString m_askpassScriptPath;
};
