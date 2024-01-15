#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>

#include "core/iconloader.h"
#include "qpushbutton.h"
#include "settings/settingsdialog.h"
#include "settings/networkremotesettingspage.h"
#include "ui_networkremotesettingspage.h"

const char *NetworkRemoteSettingsPage::kSettingsGroup = "Remote";

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent) :
    SettingsPage(dialog,parent),
    ui_(new Ui_NetworkRemoteSettingsPage)
{
  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("network-remote", true, 0,32));

  QObject::connect(ui_->useRemoteClient,&QPushButton::clicked, this, &NetworkRemoteSettingsPage::RemoteButtonClicked);
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
  if (s.contains("useRemote")){
    ui_->useRemoteClient->setChecked(s.value("useRemote", false).toBool());
    if (s.value("useRemote").toBool()){
      ui_->localConnectionsOnly->setCheckable(true);
      ui_->portSelected->setReadOnly(false);
      ui_->localConnectionsOnly->setChecked(s.value("localOnly", true).toBool());
      ui_->portSelected->setValue(s.value("remotePort", 5050).toInt());
    }
    else {
      ui_->localConnectionsOnly->setCheckable(false);
      ui_->portSelected->setReadOnly(true);
    }
  }
  else{
    qLog(Debug) << "First time run the Network Remote";
    s.setValue("useRemote", false);
    s.setValue("localOnly",false);
    s.setValue("remotePort",5050);
    s.setValue("ipAddress","0.0.0.0");
  }
  s.endGroup();
  DisplayIP();

  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{
  s.beginGroup(NetworkRemoteSettingsPage::kSettingsGroup);
  s.setValue("useRemote",ui_->useRemoteClient->isChecked());
  s.setValue("localOnly",ui_->localConnectionsOnly->isChecked());
  s.setValue("remotePort",int(ui_->portSelected->value()));
  s.setValue("ipAddress",ipAddr_);
  s.endGroup();
}

void NetworkRemoteSettingsPage::DisplayIP()
{
  bool found = false;
  QList<QHostAddress> hostList = QNetworkInterface::allAddresses();

  for (const QHostAddress &address : hostList)
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol && address.isLoopback() == false && !found){
    // NOTE: this code currently only takes the first ip address it finds
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    qInfo("Warning: The code only pickst the first IPv4 address");
      found = true;
      ipAddr_ = address.toString();
   }
  }
  ui_->ip_address->setText(ipAddr_);
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  Save();
  Load();
}

