#include "tcpserver.h"
#include "core/logging.h"
#include "networkremote/clientmanager.h"


TcpServer::TcpServer(Application* app, QObject *parent)
    : QObject{parent},
      app_(app)
{
  server_ = new QTcpServer(this);
  clientMgr_ = new ClientManager(app_);
  connect(server_,&QTcpServer::newConnection, this, &TcpServer::NewTcpConnection);
}

TcpServer::~TcpServer()
{
}

void TcpServer::StartServer(QHostAddress ipAddr, int port)
{
  bool ok = false;
  ok = server_->listen(ipAddr, port);
  if (ok){
    qLog(Debug) << "TCP Server Started ----------------------";
  }
}

void TcpServer::NewTcpConnection()
{
  socket_ = server_->nextPendingConnection();
  clientMgr_->AddClient(socket_);
  qLog(Debug) << "New Socket -------------------";
}

void TcpServer::StopServer()
{
  server_->close();
  qLog(Debug) << "TCP Server Stopped ----------------------";
}


bool TcpServer::ServerUp()
{
  return server_->isListening();
}

