#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>

#include "core/iconloader.h"
#include "networkremote/networkremote.h"
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

  QObject::connect(ui_->useRemoteClient,&QAbstractButton::clicked, this, &NetworkRemoteSettingsPage::RemoteButtonClicked);
  QObject::connect(ui_->localConnectionsOnly, &QAbstractButton::clicked, this, &NetworkRemoteSettingsPage::LocalConnectButtonClicked);
  QObject::connect(ui_->portSelected, &QAbstractSpinBox::editingFinished, this, &NetworkRemoteSettingsPage::PortChanged);
}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage()
{
    delete ui_;
}

void NetworkRemoteSettingsPage::Load()
{
  ui_->portSelected->setRange(5050, 65535);
  ui_->ip_address->setText("0.0.0.0");

  s_.beginGroup(NetworkRemoteSettingsPage::kSettingsGroup);

  if (s_.contains("useRemote")){
    ui_->useRemoteClient->setCheckable(true);
    ui_->useRemoteClient->setChecked(s_.value("useRemote", false).toBool());
    if (s_.value("useRemote").toBool()){
      ui_->localConnectionsOnly->setCheckable(true);
      ui_->localConnectionsOnly->setChecked(s_.value("localOnly", false).toBool());
      ui_->portSelected->setReadOnly(false);
      ui_->portSelected->setValue(s_.value("remotePort", 5050).toInt());
    }
    else {
      ui_->localConnectionsOnly->setCheckable(false);
      ui_->portSelected->setReadOnly(true);
    }
  }
  else{
    qLog(Debug) << "First time run the Network Remote";
    s_.setValue("useRemote", false);
    s_.setValue("localOnly",false);
    s_.setValue("remotePort",5050);
    s_.setValue("ipAddress","0.0.0.0");
  }
  s_.endGroup();

  DisplayIP();
  qInfo("Loaded QSettings ++++++++++++++++");

  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{
  s_.beginGroup(NetworkRemoteSettingsPage::kSettingsGroup);
  s_.setValue("useRemote",ui_->useRemoteClient->isChecked());
  s_.setValue("localOnly",ui_->localConnectionsOnly->isChecked());
  s_.setValue("remotePort",int(ui_->portSelected->value()));
  s_.setValue("ipAddress",ipAddr_);
  s_.endGroup();

  qInfo("Saving QSettings ++++++++++++++++");
}

void NetworkRemoteSettingsPage::Refresh()
{
  s_.sync();
  Save();
  Load();

  if (NetworkRemote::Instance()) {
    qInfo() << "NetworkRemote Instance is up";
    NetworkRemote::Instance()->Update();
  }
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
    // qInfo("Warning: The code only picks the first IPv4 address");
      found = true;
      ipAddr_ = address.toString();
   }
  }
  ui_->ip_address->setText(ipAddr_);
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  Refresh();
}

void NetworkRemoteSettingsPage::LocalConnectButtonClicked()
{
  Refresh();
}

void NetworkRemoteSettingsPage::PortChanged()
{
  Refresh();
}

