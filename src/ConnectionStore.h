#pragma once

#include "ConnectionProfile.h"

#include <QList>

class ConnectionStore {
public:
    static QList<ConnectionProfile> load();
    static bool save(const QList<ConnectionProfile>& profiles, QString* error = nullptr);
};
