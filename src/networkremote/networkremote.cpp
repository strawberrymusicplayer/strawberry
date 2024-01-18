
#include <QSettings>
#include <QThread>

#include "networkremote/networkremote.h"
#include "core/logging.h"

class TcpServer;

const char *NetworkRemote::kSettingsGroup = "Remote";

NetworkRemote::NetworkRemote(Application* app, QObject *parent)
    : QObject(parent),
      app_(app),
      original_thread_(nullptr)
{
  setObjectName("Network Remote");
  original_thread_ = thread();
}

NetworkRemote::~NetworkRemote()
{
}

void NetworkRemote::Init()
{
  LoadSettings();
  if (use_remote_){
    start();
  }
  else {
    stop();
  }
}

void NetworkRemote::LoadSettings()
{
  QSettings s;
  s.beginGroup(NetworkRemote::kSettingsGroup);
  use_remote_ = s.value("useRemote").toBool();
  local_only_ = s.value("localOnly").toBool();
  remote_port_ = s.value("remotePort").toInt();
  ipAddr_.setAddress(s.value("ipAddress").toString());
  s.endGroup();
}

void NetworkRemote::start()
{
  server_->StartServer(ipAddr_,remote_port_);
}

void NetworkRemote::stop()
{
  if (server_->ServerUp()){
    server_->StopServer();
  }
}

void NetworkRemote::useRemoteClicked()
{

}

