#include <QStyle>
#include "core/iconloader.h"
#include "core/logging.h"
#include "networkremote/networkremote.h"
#include "settings/settingsdialog.h"
#include "settings/networkremotesettingspage.h"
#include "ui_networkremotesettingspage.h"

using namespace Qt::Literals::StringLiterals;

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent) :
  SettingsPage(dialog,parent),
  ui_(new Ui_NetworkRemoteSettingsPage),
  settings_(new NetworkRemoteSettings)
{
  ui_->setupUi(this);
  const int iconSize = style()->pixelMetric(QStyle::PM_TabBarIconSize);
  setWindowIcon(IconLoader::Load(QStringLiteral("network-remote"), true, 0,iconSize));
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
  ui_->ip_address->setText(QStringLiteral("0.0.0.0"));
  settings_->Load();

  ui_->useRemoteClient->setCheckable(true);
  ui_->useRemoteClient->setChecked(settings_->UserRemote());
  if (settings_->UserRemote()){
    ui_->localConnectionsOnly->setCheckable(true);
    ui_->localConnectionsOnly->setChecked(settings_->LocalOnly());
    ui_->portSelected->setReadOnly(false);
    ui_->portSelected->setValue(settings_->GetPort());
  }
  else{
    ui_->localConnectionsOnly->setCheckable(false);
    ui_->portSelected->setReadOnly(true);
  }

  DisplayIP();
  qLog(Debug) << "SettingsPage Loaded QSettings ++++++++++++++++";

  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save()
{
  qLog(Debug) << "Saving QSettings ++++++++++++++++";
}

void NetworkRemoteSettingsPage::Refresh()
{
  if (NetworkRemote::Instance()) {
    qLog(Debug) << "NetworkRemote Instance is up";
    NetworkRemote::Instance()->Update();
  }
}

void NetworkRemoteSettingsPage::DisplayIP()
{
  ui_->ip_address->setText(settings_->GetIpAddress());
}

void NetworkRemoteSettingsPage::RemoteButtonClicked()
{
  settings_->SetUseRemote(ui_->useRemoteClient->isChecked());
  ui_->useRemoteClient->setChecked(settings_->UserRemote());
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
  settings_->SetLocalOnly(ui_->localConnectionsOnly->isChecked());
  Refresh();
}

void NetworkRemoteSettingsPage::PortChanged()
{
  settings_->SetPort(ui_->portSelected->value());
  Refresh();
}

