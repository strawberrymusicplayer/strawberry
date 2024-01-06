#include <QVariant>
#include <QSettings>
#include <QNetworkProxy>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>

#include "core/iconloader.h"
#include "config.h"
#include "qpushbutton.h"
#include "settings/settingsdialog.h"
#include "networkremotesettingspage.h"
#include "ui_networkremotesettingspage.h"

const char *NetworkRemoteSettingsPage::kSettingsGroup = "Remote";

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent) :
    SettingsPage(dialog,parent),
    ui_(new Ui_NetworkRemoteSettingsPage)
{
  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("network-remote", true, 0,32));

  ui_->portSelected->setRange(5050, 65535);
  ui_->ip_address->setText("0.0.0.0");

  QObject::connect(ui_->useRemoteClient,&QPushButton::clicked, this, &NetworkRemoteSettingsPage::EnableRemote);
  QObject::connect(ui_->localConnectionsOnly,&QPushButton::clicked, this,&NetworkRemoteSettingsPage::LocalConnectButtonClicked);
  QObject::connect(ui_->portSelected,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&NetworkRemoteSettingsPage::RemotePortSet);

}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage()
{
    delete ui_;
}

void NetworkRemoteSettingsPage::Load()
{
  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{

}

void NetworkRemoteSettingsPage::EnableRemote()
{
  qLog(Debug) << "Enable Remote Code";
}

void NetworkRemoteSettingsPage::LocalConnectOnly()
{
  qLog(Debug) << "Local Connection Code";
}

void NetworkRemoteSettingsPage::DisplayIP()
{
  qLog(Debug) << "Display IP Code";
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  qLog(Debug) << "Remote Button Code";
}

void NetworkRemoteSettingsPage::LocalConnectButtonClicked()
{
  qLog(Debug) << "ELocal Connection Code";
}

void NetworkRemoteSettingsPage::RemotePortSet()
{
  qLog(Debug) << "Remote Port Code";
}
