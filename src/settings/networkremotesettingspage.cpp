#include <QVariant>
#include <QSettings>
#include <QNetworkProxy>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QSettings>

#include "core/iconloader.h"
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

  QObject::connect(ui_->useRemoteClient,&QPushButton::clicked, this, &NetworkRemoteSettingsPage::RemoteButtonClicked);
  // QObject::connect(ui_->localConnectionsOnly,&QPushButton::clicked, this,&NetworkRemoteSettingsPage::LocalConnectButtonClicked);
  // QObject::connect(ui_->portSelected,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&NetworkRemoteSettingsPage::RemotePortSet);
}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage()
{
    delete ui_;
}

void NetworkRemoteSettingsPage::Load()
{
  ui_->portSelected->setRange(5050, 65535);
  ui_->ip_address->setText("0.0.0.0");

  s.beginGroup(NetworkRemoteSettingsPage::kSettingsGroup);
  qLog(Debug) << "QSettings file is in" << s.fileName() << "Group" << kSettingsGroup;
  if (s.contains("useRemote")){
    qLog(Debug) << "Loading QSettings";
    ui_->useRemoteClient->setChecked(s.value("useRemote", false).toBool());
    if (s.value("useRemote").toBool()){
      ui_->localConnectionsOnly->setCheckable(true);
      ui_->localConnectionsOnly->setChecked(s.value("localOnly", false).toBool());
      ui_->portSelected->setValue(s.value("remotePort", 5050).toInt());
    }
    else {
      ui_->localConnectionsOnly->setCheckable(false);
      ui_->portSelected->isReadOnly();
    }
  }
  else{
    qLog(Debug) << "First time run the Network Remote";
    s.setValue("useRemote", false);
    s.setValue("localOnly",false);
    s.setValue("remotePort",5050);
  }
  qLog(Debug) << s.allKeys();
  s.endGroup();

  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{
  qLog(Debug) << "Save Settings =================";
  s.beginGroup(NetworkRemoteSettingsPage::kSettingsGroup);
  s.setValue("useRemote",ui_->useRemoteClient->isChecked());
  s.setValue("localOnly",ui_->localConnectionsOnly->isChecked());
  s.setValue("remotePort",int(ui_->portSelected->value()));
  s.endGroup();
}

void NetworkRemoteSettingsPage::DisplayIP()
{
  qLog(Debug) << "Display IP Code";
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  qLog(Debug) << "Remote Button Code";
  Save();
  Load();
  // NetworkRemoteSettingsPage::Load();
}

