#include "ConnectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Connection Profile");

    auto* root = new QVBoxLayout(this);

    auto* basicBox = new QGroupBox("VNC Target", this);
    auto* basicForm = new QFormLayout(basicBox);
    m_nameEdit = new QLineEdit(basicBox);
    m_vncHostEdit = new QLineEdit(basicBox);
    m_vncPortSpin = new QSpinBox(basicBox);
    m_vncPortSpin->setRange(1, 65535);
    m_vncPortSpin->setValue(5900);
    m_vncPasswordEdit = new QLineEdit(basicBox);
    m_vncPasswordEdit->setEchoMode(QLineEdit::Password);

    basicForm->addRow("Profile name", m_nameEdit);
    basicForm->addRow("VNC server IP/host", m_vncHostEdit);
    basicForm->addRow("VNC server port", m_vncPortSpin);
    basicForm->addRow("VNC password", m_vncPasswordEdit);

    root->addWidget(basicBox);

    auto* tunnelBox = new QGroupBox("SSH Tunnel (Gateway)", this);
    auto* tunnelForm = new QFormLayout(tunnelBox);

    m_tunnelCheck = new QCheckBox("Connect through SSH gateway", tunnelBox);
    m_gatewayHostEdit = new QLineEdit(tunnelBox);
    m_gatewaySshPortSpin = new QSpinBox(tunnelBox);
    m_gatewaySshPortSpin->setRange(1, 65535);
    m_gatewaySshPortSpin->setValue(22);
    m_sshUserEdit = new QLineEdit(tunnelBox);
    m_sshAuthModeCombo = new QComboBox(tunnelBox);
    m_sshAuthModeCombo->addItem("No auth options (agent/default)", "none");
    m_sshAuthModeCombo->addItem("Password", "password");
    m_sshAuthModeCombo->addItem("Private key file", "key");
    m_sshPasswordEdit = new QLineEdit(tunnelBox);
    m_sshPasswordEdit->setEchoMode(QLineEdit::Password);
    m_sshKeyPathEdit = new QLineEdit(tunnelBox);
    m_browseKeyButton = new QPushButton("Browse", tunnelBox);
    auto* keyRow = new QWidget(tunnelBox);
    auto* keyRowLayout = new QHBoxLayout(keyRow);
    keyRowLayout->setContentsMargins(0, 0, 0, 0);
    keyRowLayout->addWidget(m_sshKeyPathEdit, 1);
    keyRowLayout->addWidget(m_browseKeyButton);
    m_tunneledVncHostEdit = new QLineEdit(tunnelBox);
    m_tunneledVncPortSpin = new QSpinBox(tunnelBox);
    m_tunneledVncPortSpin->setRange(1, 65535);
    m_tunneledVncPortSpin->setValue(5900);

    tunnelForm->addRow(m_tunnelCheck);
    tunnelForm->addRow("Gateway IP/host", m_gatewayHostEdit);
    tunnelForm->addRow("Gateway SSH port", m_gatewaySshPortSpin);
    tunnelForm->addRow("SSH username", m_sshUserEdit);
    tunnelForm->addRow("SSH auth mode", m_sshAuthModeCombo);
    tunnelForm->addRow("SSH password", m_sshPasswordEdit);
    tunnelForm->addRow("SSH private key", keyRow);
    tunnelForm->addRow("Remote VNC server IP/host", m_tunneledVncHostEdit);
    tunnelForm->addRow("Remote VNC server port", m_tunneledVncPortSpin);

    root->addWidget(tunnelBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_tunnelCheck, &QCheckBox::toggled, this, &ConnectionDialog::onTunnelToggled);
    connect(m_sshAuthModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ConnectionDialog::onAuthModeChanged);
    connect(m_browseKeyButton, &QPushButton::clicked, this, &ConnectionDialog::browseKeyFile);
    onAuthModeChanged(m_sshAuthModeCombo->currentIndex());
    onTunnelToggled(false);
}

void ConnectionDialog::setProfile(const ConnectionProfile& profile) {
    m_nameEdit->setText(profile.name);
    m_vncHostEdit->setText(profile.vncHost);
    m_vncPortSpin->setValue(profile.vncPort);
    m_vncPasswordEdit->setText(profile.vncPassword);

    m_tunnelCheck->setChecked(profile.useSshTunnel);
    m_gatewayHostEdit->setText(profile.gatewayHost);
    m_gatewaySshPortSpin->setValue(profile.gatewaySshPort);
    m_sshUserEdit->setText(profile.sshUser);
    const int authIndex = m_sshAuthModeCombo->findData(profile.sshAuthMode);
    m_sshAuthModeCombo->setCurrentIndex(authIndex >= 0 ? authIndex : 0);
    m_sshPasswordEdit->setText(profile.sshPassword);
    m_sshKeyPathEdit->setText(profile.sshPrivateKeyPath);
    m_tunneledVncHostEdit->setText(profile.tunneledVncHost);
    m_tunneledVncPortSpin->setValue(profile.tunneledVncPort);
    onAuthModeChanged(m_sshAuthModeCombo->currentIndex());
    onTunnelToggled(profile.useSshTunnel);
}

ConnectionProfile ConnectionDialog::profile() const {
    ConnectionProfile out;
    out.name = m_nameEdit->text().trimmed();
    out.vncHost = m_vncHostEdit->text().trimmed();
    out.vncPort = m_vncPortSpin->value();
    out.vncPassword = m_vncPasswordEdit->text();

    out.useSshTunnel = m_tunnelCheck->isChecked();
    out.gatewayHost = m_gatewayHostEdit->text().trimmed();
    out.gatewaySshPort = m_gatewaySshPortSpin->value();
    out.sshUser = m_sshUserEdit->text().trimmed();
    out.sshAuthMode = m_sshAuthModeCombo->currentData().toString();
    out.sshPassword = m_sshPasswordEdit->text();
    out.sshPrivateKeyPath = m_sshKeyPathEdit->text().trimmed();
    out.tunneledVncHost = m_tunneledVncHostEdit->text().trimmed();
    out.tunneledVncPort = m_tunneledVncPortSpin->value();

    if (out.name.isEmpty()) {
        out.name = QString("%1:%2").arg(out.vncHost).arg(out.vncPort);
    }
    return out;
}

void ConnectionDialog::onTunnelToggled(bool enabled) {
    m_gatewayHostEdit->setEnabled(enabled);
    m_gatewaySshPortSpin->setEnabled(enabled);
    m_sshUserEdit->setEnabled(enabled);
    m_sshAuthModeCombo->setEnabled(enabled);
    m_sshPasswordEdit->setEnabled(enabled && m_sshAuthModeCombo->currentData().toString() == "password");
    m_sshKeyPathEdit->setEnabled(enabled && m_sshAuthModeCombo->currentData().toString() == "key");
    m_browseKeyButton->setEnabled(enabled && m_sshAuthModeCombo->currentData().toString() == "key");
    m_tunneledVncHostEdit->setEnabled(enabled);
    m_tunneledVncPortSpin->setEnabled(enabled);
}

void ConnectionDialog::onAuthModeChanged(int) {
    const QString mode = m_sshAuthModeCombo->currentData().toString();
    const bool passwordMode = mode == "password";
    const bool keyMode = mode == "key";
    m_sshPasswordEdit->setEnabled(m_tunnelCheck->isChecked() && passwordMode);
    m_sshKeyPathEdit->setEnabled(m_tunnelCheck->isChecked() && keyMode);
    m_browseKeyButton->setEnabled(m_tunnelCheck->isChecked() && keyMode);
}

void ConnectionDialog::browseKeyFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Select private key file");
    if (!path.isEmpty()) {
        m_sshKeyPathEdit->setText(path);
    }
}
