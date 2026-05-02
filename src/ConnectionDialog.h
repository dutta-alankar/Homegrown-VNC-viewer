#pragma once

#include "ConnectionProfile.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;

class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);

    void setProfile(const ConnectionProfile& profile);
    ConnectionProfile profile() const;

private slots:
    void onTunnelToggled(bool enabled);
    void onAuthModeChanged(int index);
    void browseKeyFile();

private:
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_vncHostEdit = nullptr;
    QSpinBox* m_vncPortSpin = nullptr;
    QLineEdit* m_vncPasswordEdit = nullptr;

    QCheckBox* m_tunnelCheck = nullptr;
    QLineEdit* m_gatewayHostEdit = nullptr;
    QSpinBox* m_gatewaySshPortSpin = nullptr;
    QLineEdit* m_sshUserEdit = nullptr;
    QComboBox* m_sshAuthModeCombo = nullptr;
    QLineEdit* m_sshPasswordEdit = nullptr;
    QLineEdit* m_sshKeyPathEdit = nullptr;
    QPushButton* m_browseKeyButton = nullptr;
    QLineEdit* m_tunneledVncHostEdit = nullptr;
    QSpinBox* m_tunneledVncPortSpin = nullptr;
};
