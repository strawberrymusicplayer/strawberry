

#include <QThread>

#include "networkremote/networkremote.h"
#include "core/logging.h"


class TcpServer;


NetworkRemote* NetworkRemote::sInstance = nullptr;
const char *NetworkRemote::kSettingsGroup = "Remote";

NetworkRemote::NetworkRemote(Application* app, QObject *parent)
    : QObject(parent),
      app_(app),
      original_thread_(nullptr)
{
  setObjectName("Network Remote");
  original_thread_ = thread();
  sInstance = this;
}

NetworkRemote::~NetworkRemote()
{
  stopTcpServer();
}

void NetworkRemote::Init()
{
  s_.beginGroup(NetworkRemote::kSettingsGroup);
  use_remote_ = s_.value("useRemote").toBool();
  local_only_ = s_.value("localOnly").toBool();
  remote_port_ = s_.value("remotePort").toInt();
  ipAddr_.setAddress(s_.value("ipAddress").toString());

  bool aa = s_.value("useRemote").toBool();
  bool bb = s_.value("localOnly").toBool();
  int cc = s_.value("remotePort").toInt();
  QString dd =s_.value("ipAddress").toString();
  qLog(Debug) << "Settings " << s_.fileName();
  qLog(Debug) << "Keys are " << s_.allKeys();
  qLog(Debug) << "aa = " << aa;
  qLog(Debug) << "bb = " << bb;
  qLog(Debug) << "cc = " << cc;
  qLog(Debug) << "dd = " << dd;

  s_.endGroup();

  if (use_remote_){
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
}

void NetworkRemote::Update()
{
  s_.beginGroup(NetworkRemote::kSettingsGroup);
  bool aa = s_.value("useRemote").toBool();
  bool bb = s_.value("localOnly").toBool();
  int cc = s_.value("remotePort").toInt();
  QString dd =s_.value("ipAddress").toString();

  if (remote_port_ != s_.value("useRemote").toBool()){
    qLog(Debug) << "use_remote_ changed";
  }
  if (use_remote_ != s_.value("remotePort").toInt()){
    qLog(Debug) << "remote_port_ changed";
  }
  if (ipAddr_.toString() != s_.value("ipAddress").toString()){
    qLog(Debug) << "IP addres changed";
  }
/*
  use_remote_ = s.value("useRemote").toBool();
  local_only_ = s.value("localOnly").toBool();
  remote_port_ = s.value("remotePort").toInt();
  ipAddr_.setAddress(s.value("ipAddress").toString());
*/
  s_.endGroup();

}

void NetworkRemote::startTcpServer()
{
  server_->StartServer(ipAddr_,remote_port_);
  qLog(Debug) << "TcpServer started on IP " << ipAddr_<< " and port" << remote_port_;
}

void NetworkRemote::stopTcpServer()
{
  if (server_->ServerUp()){
    qLog(Debug) << "TcpServer stopped ";
    server_->StopServer();
  }
}

NetworkRemote* NetworkRemote::Instance() {
  if (!sInstance) {
    // Error
    return nullptr;
  }
  qLog(Debug) << "NetworkRemote instance is up ";
  return sInstance;
}

