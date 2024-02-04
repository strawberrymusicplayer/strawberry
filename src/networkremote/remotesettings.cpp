#include <QHostAddress>
#include <QNetworkInterface>

#include "remotesettings.h"
#include "core/logging.h"


const char *RemoteSettings::kSettingsGroup = "NetworkRemote";

RemoteSettings::RemoteSettings(QObject *parent)
    : QObject{parent}
{
}

RemoteSettings::~RemoteSettings()
{}

void RemoteSettings::Load()
{
  SetIpAdress();
  s_.beginGroup(RemoteSettings::kSettingsGroup);
  if (!s_.contains("useRemote")){
    qLog(Debug) << "First time run the Network Remote";
    s_.setValue("useRemote", false);
    s_.setValue("localOnly",false);
    s_.setValue("remotePort",5050);
    s_.setValue("ipAddress",ipAddr_);
  }
  else {
    use_remote_ = s_.value("useRemote").toBool();
    local_only_ = s_.value("localOnly").toBool();
    remote_port_ = s_.value("remotePort").toInt();
    s_.setValue("ipAddress",ipAddr_);
  }
  s_.endGroup();
  qInfo("QSettings Loaded ++++++++++++++++");
}

void RemoteSettings::Save()
{
  s_.beginGroup(RemoteSettings::kSettingsGroup);
  s_.setValue("useRemote",use_remote_);
  s_.setValue("localOnly",local_only_);
  s_.setValue("remotePort",remote_port_);
  s_.setValue("ipAddress",ipAddr_);
  s_.endGroup();
  s_.sync();
  qInfo("Saving QSettings ++++++++++++++++");
}

bool RemoteSettings::UserRemote()
{
  return use_remote_;
}

bool RemoteSettings::LocalOnly()
{
  return local_only_;
}

QString RemoteSettings::GetIpAddress()
{
  return ipAddr_;
}

int RemoteSettings::GetPort()
{
  return remote_port_;
}

void RemoteSettings::SetUseRemote(bool useRemote)
{
  use_remote_ = useRemote;
  Save();
}

void RemoteSettings::SetLocalOnly(bool localOnly)
{
  local_only_ = localOnly;
  Save();
}

void RemoteSettings::SetIpAdress()
{
  bool found = false;
  QList<QHostAddress> hostList = QNetworkInterface::allAddresses();

  for (const QHostAddress &address : hostList)
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol && address.isLoopback() == false && !found){
    // NOTE: this code currently only takes the first ip address it finds
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    qInfo("Warning: The code only picks the first IPv4 address");
      found = true;
      ipAddr_ = address.toString();
    }
  }
}

void RemoteSettings::SetPort(int port)
{
  remote_port_ = port;
  Save();
}
