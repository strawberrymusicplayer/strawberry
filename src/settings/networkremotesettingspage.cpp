#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>

#include "core/iconloader.h"
#include "networkremote/networkremote.h"
#include "settings/settingsdialog.h"
#include "settings/networkremotesettingspage.h"
#include "ui_networkremotesettingspage.h"

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
  s_->Load();

  ui_->useRemoteClient->setCheckable(true);
  ui_->useRemoteClient->setChecked(s_->UserRemote());
  if (s_->UserRemote()){
    ui_->localConnectionsOnly->setCheckable(true);
    ui_->localConnectionsOnly->setChecked(s_->LocalOnly());
    ui_->portSelected->setReadOnly(false);
    ui_->portSelected->setValue(s_->GetPort());
  }
  else{
      ui_->localConnectionsOnly->setCheckable(false);
      ui_->portSelected->setReadOnly(true);
  }

  DisplayIP();
  qInfo("SettingsPage Loaded QSettings ++++++++++++++++");

  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{
  qInfo("Saving QSettings ++++++++++++++++");
}

void NetworkRemoteSettingsPage::Refresh()
{
  if (NetworkRemote::Instance()) {
    qInfo() << "NetworkRemote Instance is up";
    NetworkRemote::Instance()->Update();
  }
}

void NetworkRemoteSettingsPage::DisplayIP()
{
  ui_->ip_address->setText(s_->GetIpAddress());
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  s_->SetUseRemote(ui_->useRemoteClient->isChecked());
  ui_->useRemoteClient->setChecked(s_->UserRemote());
  if (ui_->useRemoteClient->isChecked()){
    ui_->localConnectionsOnly->setCheckable(true);
    ui_->portSelected->setReadOnly(false);
  }
  else{
    ui_->localConnectionsOnly->setCheckable(false);
    ui_->portSelected->setReadOnly(true);
    }
  Refresh();
}


void NetworkRemoteSettingsPage::LocalConnectButtonClicked()
{
  s_->SetLocalOnly(ui_->localConnectionsOnly->isChecked());
  Refresh();
}

void NetworkRemoteSettingsPage::PortChanged()
{
  s_->SetPort(ui_->portSelected->value());
  Refresh();
}

