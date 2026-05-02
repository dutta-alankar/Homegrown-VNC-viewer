#pragma once

#include <QString>

struct ConnectionProfile {
    QString name;

    QString vncHost;
    int vncPort = 5900;

    bool useSshTunnel = false;
    QString gatewayHost;
    int gatewaySshPort = 22;
    QString sshUser;
    QString sshAuthMode = "none"; // one of: none, password, key
    QString sshPassword;
    QString sshPrivateKeyPath;

    QString tunneledVncHost;
    int tunneledVncPort = 5900;

    QString vncPassword;
    QString thumbnailPngBase64;
};
