#include "ConnectionStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
QString storePath() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(baseDir);
    dir.mkpath(".");
    return dir.filePath("connections.json");
}

QJsonObject toJson(const ConnectionProfile& p) {
    return {
        {"name", p.name},
        {"vncHost", p.vncHost},
        {"vncPort", p.vncPort},
        {"useSshTunnel", p.useSshTunnel},
        {"gatewayHost", p.gatewayHost},
        {"gatewaySshPort", p.gatewaySshPort},
        {"sshUser", p.sshUser},
        {"sshAuthMode", p.sshAuthMode},
        {"sshPassword", p.sshPassword},
        {"sshPrivateKeyPath", p.sshPrivateKeyPath},
        {"tunneledVncHost", p.tunneledVncHost},
        {"tunneledVncPort", p.tunneledVncPort},
        {"vncPassword", p.vncPassword},
        {"thumbnailPngBase64", p.thumbnailPngBase64},
    };
}

ConnectionProfile fromJson(const QJsonObject& o) {
    ConnectionProfile p;
    p.name = o.value("name").toString();
    p.vncHost = o.value("vncHost").toString();
    p.vncPort = o.value("vncPort").toInt(5900);
    p.useSshTunnel = o.value("useSshTunnel").toBool(false);
    p.gatewayHost = o.value("gatewayHost").toString();
    p.gatewaySshPort = o.value("gatewaySshPort").toInt(22);
    p.sshUser = o.value("sshUser").toString();
    p.sshAuthMode = o.value("sshAuthMode").toString("none");
    p.sshPassword = o.value("sshPassword").toString();
    p.sshPrivateKeyPath = o.value("sshPrivateKeyPath").toString();
    p.tunneledVncHost = o.value("tunneledVncHost").toString();
    p.tunneledVncPort = o.value("tunneledVncPort").toInt(5900);
    p.vncPassword = o.value("vncPassword").toString();
    p.thumbnailPngBase64 = o.value("thumbnailPngBase64").toString();
    return p;
}
} // namespace

QList<ConnectionProfile> ConnectionStore::load() {
    QFile file(storePath());
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {};
    }
    if (!doc.isArray()) {
        return {};
    }

    QList<ConnectionProfile> out;
    for (const auto& val : doc.array()) {
        if (val.isObject()) {
            out.append(fromJson(val.toObject()));
        }
    }
    return out;
}

bool ConnectionStore::save(const QList<ConnectionProfile>& profiles, QString* error) {
    QJsonArray array;
    for (const auto& profile : profiles) {
        array.append(toJson(profile));
    }

    QFile file(storePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}
