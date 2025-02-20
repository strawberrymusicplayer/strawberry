#include "tcpserver.h"
#include "core/logging.h"
#include "networkremote/clientmanager.h"
#include <QNetworkProxy>


NetworkRemoteTcpServer::NetworkRemoteTcpServer(Application* app, QObject *parent)
  : QObject(parent),
    app_(app)
{
  server_ = new QTcpServer(this);
  clientMgr_ = new NetworkRemoteClientManager(app_);
  connect(server_,&QTcpServer::newConnection, this, &NetworkRemoteTcpServer::NewTcpConnection);
}

NetworkRemoteTcpServer::~NetworkRemoteTcpServer()
{
}

void NetworkRemoteTcpServer::StartServer(QHostAddress ipAddr, int port)
{
  bool ok = false;
  server_->setProxy(QNetworkProxy::NoProxy);
  ok = server_->listen(ipAddr, port);
  if (ok){
    qLog(Debug) << "TCP Server Started on --- " << ipAddr.toString() << " and port -- " << port;
  }
}

void NetworkRemoteTcpServer::NewTcpConnection()
{
  socket_ = server_->nextPendingConnection();
  clientMgr_->AddClient(socket_);
  qLog(Debug) << "New Socket -------------------";
}

void NetworkRemoteTcpServer::StopServer()
{
  server_->close();
  qLog(Debug) << "TCP Server Stopped ----------------------";
}


bool NetworkRemoteTcpServer::ServerUp()
{
  return server_->isListening();
}

