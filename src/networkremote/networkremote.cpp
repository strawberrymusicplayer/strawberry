#include <QThread>
#include "networkremote/networkremote.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"

NetworkRemote* NetworkRemote::sInstance_ = nullptr;
const char *NetworkRemote::kSettingsGroup = "Remote";

NetworkRemote::NetworkRemote(Application* app, QObject *parent)
  : QObject(parent),
    app_(app),
    enabled_(false),
    local_only_(false),
    remote_port_(5050),
    server_(new NetworkRemoteTcpServer(app_,this)),
    original_thread_(thread()),
    settings_(new NetworkRemoteSettings())
{
  setObjectName("NetworkRemote");
  sInstance_ = this;
}

NetworkRemote::~NetworkRemote()
{
  stopTcpServer();
}

void NetworkRemote::Init()
{
  LoadSettings();
  if (enabled_){
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
  qLog(Debug) << "NetworkRemote Init() ";
}

void NetworkRemote::Update()
{
  LoadSettings();
  if (enabled_){
    stopTcpServer();
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
  qLog(Debug) << "NetworkRemote Updated ==== ";
}

void NetworkRemote::LoadSettings()
{
  settings_->Load();
  enabled_ = settings_->UserRemote();
  local_only_ = settings_->LocalOnly();
  remote_port_ = settings_->GetPort();
  ipAddr_.setAddress(settings_->GetIpAddress());
}

void NetworkRemote::startTcpServer()
{
  server_->StartServer(ipAddr_,remote_port_);
}

void NetworkRemote::stopTcpServer()
{
  if (server_->ServerUp()){
    qLog(Debug) << "TcpServer stopped ";
    server_->StopServer();
  }
}

NetworkRemote* NetworkRemote::Instance() {
  if (!sInstance_) {
    qLog(Debug) << "NetworkRemote Fatal Instance Error ";
    return nullptr;
  }
  qLog(Debug) << "NetworkRemote instance is up ";
  return sInstance_;
}

