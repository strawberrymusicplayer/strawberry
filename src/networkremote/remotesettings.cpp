#include <QHostAddress>
#include <QNetworkInterface>

#include "remotesettings.h"
#include "core/logging.h"

const char *NetworkRemoteSettings::kSettingsGroup = "NetworkRemote";

NetworkRemoteSettings::NetworkRemoteSettings()
  : enabled_(false),
    local_only_(false),
    remote_port_(5050)
{}

NetworkRemoteSettings::~NetworkRemoteSettings()
{}

void NetworkRemoteSettings::Load()
{
  SetIpAdress();
  s_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  if (!s_.contains("useRemote")){
    qLog(Debug) << "First time run the Network Remote";
    s_.setValue("useRemote", false);
    s_.setValue("localOnly",false);
    s_.setValue("remotePort",5050);
    s_.setValue("ipAddress",ipAddr_);
  }
  else {
    enabled_ = s_.value("useRemote").toBool();
    local_only_ = s_.value("localOnly").toBool();
    remote_port_ = s_.value("remotePort").toInt();
    s_.setValue("ipAddress",ipAddr_);
  }
  s_.endGroup();
  qInfo("QSettings Loaded ++++++++++++++++");
}

void NetworkRemoteSettings::Save()
{
  s_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  s_.setValue("useRemote",enabled_);
  s_.setValue("localOnly",local_only_);
  s_.setValue("remotePort",remote_port_);
  s_.setValue("ipAddress",ipAddr_);
  s_.endGroup();
  s_.sync();
  qInfo("Saving QSettings ++++++++++++++++");
}

bool NetworkRemoteSettings::UserRemote()
{
  return enabled_;
}

bool NetworkRemoteSettings::LocalOnly()
{
  return local_only_;
}

QString NetworkRemoteSettings::GetIpAddress()
{
  return ipAddr_;
}

int NetworkRemoteSettings::GetPort()
{
  return remote_port_;
}

void NetworkRemoteSettings::SetUseRemote(bool useRemote)
{
  enabled_ = useRemote;
  Save();
}

void NetworkRemoteSettings::SetLocalOnly(bool localOnly)
{
  local_only_ = localOnly;
  Save();
}

void NetworkRemoteSettings::SetIpAdress()
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

void NetworkRemoteSettings::SetPort(int port)
{
  remote_port_ = port;
  Save();
}
