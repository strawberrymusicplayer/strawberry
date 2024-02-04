

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
  use_remote_ = s_->UserRemote();
  local_only_ = s_->LocalOnly();
  remote_port_ = s_->GetPort();
  ipAddr_.setAddress(s_->GetIpAddress());

  if (use_remote_){
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
  qLog(Debug) << "NetworkRemote Init() ";
}

void NetworkRemote::Update()
{
  //s_->Save();
  //stopTcpServer();
  qLog(Debug) << "NetworkRemote Update() ";
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

