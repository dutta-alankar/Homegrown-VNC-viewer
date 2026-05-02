#include "SshTunnel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <chrono>
#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

SshTunnel::SshTunnel(QObject* parent)
    : QObject(parent) {}

SshTunnel::~SshTunnel() {
    stop();
}

bool SshTunnel::start(const QString& sshUser,
                      const QString& authMode,
                      const QString& sshPassword,
                      const QString& sshOtp,
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
        m_askpassScriptPath = createAskpassScript(sshPassword, sshOtp, error);
        if (m_askpassScriptPath.isEmpty()) {
            return false;
        }
        args << "-o" << "BatchMode=no"
               << "-o" << "GSSAPIAuthentication=no"
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

    if (!waitForLocalForward(localPort, error)) {
        return false;
    }

    return true;
}

bool SshTunnel::waitForLocalForward(int localPort, QString* error) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < 90000) {
        if (m_process.state() == QProcess::NotRunning) {
            if (error) {
                const QString stderrText = QString::fromUtf8(m_process.readAllStandardError());
                *error = stderrText.isEmpty() ? "SSH tunnel terminated unexpectedly." : stderrText;
            }
            return false;
        }

        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<uint16_t>(localPort));
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            const int connectRes = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            ::close(fd);
            if (connectRes == 0) {
                return true;
            }
        }

        m_process.waitForReadyRead(100);
        m_process.readAllStandardOutput();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (error) {
        *error = "Timed out waiting for SSH tunnel authentication/OTP completion.";
    }
    return false;
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

QString SshTunnel::createAskpassScript(const QString& password, const QString& otp, QString* error) {
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        if (error) {
            *error = "Unable to create temporary askpass script.";
        }
        return {};
    }

    // Use single-quoted shell strings and escape embedded single quotes.
    // This prevents shell expansion of $, backticks, and other metacharacters.
    QByteArray escapedPassword = password.toUtf8();
    escapedPassword.replace("'", "'\"'\"'");
    QByteArray escapedOtp = otp.toUtf8();
    escapedOtp.replace("'", "'\"'\"'");

    const QByteArray script =
        "#!/bin/sh\n"
        "prompt=\"$1\"\n"
        "password='" + escapedPassword + "'\n"
        "otp='" + escapedOtp + "'\n"
        "case \"$prompt\" in\n"
        "  *[Pp]assword*)\n"
        "    printf '%s\\n' \"$password\"\n"
        "    exit 0\n"
        "    ;;\n"
        "esac\n"
        "\n"
        "# Optional pre-supplied OTP (kept for compatibility with future UI options).\n"
        "if [ -n \"$otp\" ]; then\n"
        "  printf '%s\\n' \"$otp\"\n"
        "  exit 0\n"
        "fi\n"
        "\n"
        "# Challenge/response prompt (e.g., OTP) shown only when server asks for it.\n"
        "if command -v osascript >/dev/null 2>&1; then\n"
        "  response=$(osascript - \"$prompt\" <<'APPLESCRIPT'\n"
        "on run argv\n"
        "  set promptText to item 1 of argv\n"
        "  try\n"
        "    display dialog promptText default answer \"\" with title \"SSH Challenge\" with hidden answer buttons {\"Cancel\", \"OK\"} default button \"OK\"\n"
        "    return text returned of result\n"
        "  on error number -128\n"
        "    return \"\"\n"
        "  end try\n"
        "end run\n"
        "APPLESCRIPT\n"
        "  )\n"
        "  printf '%s\\n' \"$response\"\n"
        "  exit 0\n"
        "fi\n"
        "\n"
        "if command -v zenity >/dev/null 2>&1; then\n"
        "  response=$(zenity --password --title=\"SSH Challenge\" --text=\"$prompt\" 2>/dev/null || true)\n"
        "  printf '%s\\n' \"$response\"\n"
        "  exit 0\n"
        "fi\n"
        "\n"
        "# Last-resort fallback when no GUI prompt helper is available.\n"
        "if [ -t 0 ]; then\n"
        "  printf '%s ' \"$prompt\" >&2\n"
        "  IFS= read -r response || response=\"\"\n"
        "  printf '%s\\n' \"$response\"\n"
        "  exit 0\n"
        "fi\n"
        "\n"
        "printf '\\n'\n";
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
