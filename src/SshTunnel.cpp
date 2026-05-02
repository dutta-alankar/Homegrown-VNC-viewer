#include "SshTunnel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

SshTunnel::SshTunnel(QObject* parent)
    : QObject(parent) {}

SshTunnel::~SshTunnel() {
    stop();
}

bool SshTunnel::start(const QString& sshUser,
                      const QString& authMode,
                      const QString& sshPassword,
                      const QString& privateKeyPath,
                      const QString& gatewayHost,
                      int gatewayPort,
                      int localPort,
                      const QString& remoteHost,
                      int remotePort,
                      QString* error) {
    stop();

    QString destination = gatewayHost;
    if (!sshUser.trimmed().isEmpty()) {
        destination = QString("%1@%2").arg(sshUser, gatewayHost);
    }

    QStringList args;
    args << "-N"
         << "-o" << "ExitOnForwardFailure=yes"
         << "-o" << "StrictHostKeyChecking=accept-new"
         << "-o" << "ServerAliveInterval=30"
         << "-p" << QString::number(gatewayPort)
         << "-L" << QString("%1:%2:%3").arg(localPort).arg(remoteHost).arg(remotePort)
         << destination;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (authMode == "password") {
        if (sshPassword.isEmpty()) {
            if (error) {
                *error = "SSH password auth selected but no password was provided.";
            }
            return false;
        }
        m_askpassScriptPath = createAskpassScript(sshPassword, error);
        if (m_askpassScriptPath.isEmpty()) {
            return false;
        }
        args << "-o" << "BatchMode=no"
             << "-o" << "PubkeyAuthentication=no"
             << "-o" << "PreferredAuthentications=password,keyboard-interactive";
        env.insert("SSH_ASKPASS", m_askpassScriptPath);
        env.insert("SSH_ASKPASS_REQUIRE", "force");
        env.insert("DISPLAY", ":0");
    } else if (authMode == "key") {
        if (privateKeyPath.trimmed().isEmpty()) {
            if (error) {
                *error = "SSH key auth selected but no key file path was provided.";
            }
            return false;
        }
        if (!QFileInfo::exists(privateKeyPath)) {
            if (error) {
                *error = "SSH private key file not found.";
            }
            return false;
        }
        args << "-o" << "BatchMode=yes"
             << "-o" << "IdentitiesOnly=yes"
             << "-i" << privateKeyPath;
    } else {
        args << "-o" << "BatchMode=yes";
    }

    m_process.setProgram("ssh");
    m_process.setArguments(args);
    m_process.setProcessEnvironment(env);
    m_process.closeWriteChannel();
    m_process.start();

    if (!m_process.waitForStarted(5000)) {
        if (error) {
            *error = "Unable to start ssh process.";
        }
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        if (m_process.state() == QProcess::NotRunning) {
            if (error) {
                const QString stderrText = QString::fromUtf8(m_process.readAllStandardError());
                *error = stderrText.isEmpty() ? "SSH tunnel terminated unexpectedly." : stderrText;
            }
            return false;
        }
        if (m_process.waitForReadyRead(100)) {
            // Read and ignore any normal output while waiting for early errors.
            m_process.readAllStandardOutput();
        }
    }

    return m_process.state() == QProcess::Running;
}

void SshTunnel::stop() {
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) {
            m_process.kill();
            m_process.waitForFinished(1500);
        }
    }

    if (!m_askpassScriptPath.isEmpty()) {
        QFile::remove(m_askpassScriptPath);
        m_askpassScriptPath.clear();
    }
}

bool SshTunnel::isRunning() const {
    return m_process.state() == QProcess::Running;
}

QString SshTunnel::createAskpassScript(const QString& password, QString* error) {
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        if (error) {
            *error = "Unable to create temporary askpass script.";
        }
        return {};
    }

    QByteArray escaped = password.toUtf8();
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    const QByteArray script = "#!/bin/sh\nprintf '%s' \"" + escaped + "\"\n";
    if (tempFile.write(script) != script.size()) {
        if (error) {
            *error = "Unable to write temporary askpass script.";
        }
        return {};
    }
    tempFile.close();

    QFile::setPermissions(tempFile.fileName(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return tempFile.fileName();
}
