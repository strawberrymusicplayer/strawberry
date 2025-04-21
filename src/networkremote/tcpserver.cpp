#include "tcpserver.h"
#include "core/logging.h"
#include "networkremote/clientmanager.h"
#include <QNetworkProxy>


NetworkRemoteTcpServer::NetworkRemoteTcpServer(Application* app, QObject *parent)
  : QObject(parent),
    app_(app),
    server_(new QTcpServer(this)),
    clientMgr_(new NetworkRemoteClientManager(app_,this))
{
  connect(server_,&QTcpServer::newConnection, this, &NetworkRemoteTcpServer::NewTcpConnection);
}

void NetworkRemoteTcpServer::StartServer(QHostAddress ipAddr, int port)
{
  server_->setProxy(QNetworkProxy::NoProxy);
  if (server_->listen(ipAddr, port)){
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

