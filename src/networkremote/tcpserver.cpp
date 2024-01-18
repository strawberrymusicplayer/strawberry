#include "tcpserver.h"
#include "core/logging.h"

TcpServer::TcpServer(QObject *parent)
    : QObject{parent}
{
  server_ = new QTcpServer(this);
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
  //QTcpSocket *socket = server_->nextPendingConnection();
  socket_ = server_->nextPendingConnection();
  qLog(Debug) << "New Socket -------------------";
}

void TcpServer::StopServer()
{
  server_->close();
  qLog(Debug) << "TCP Server Stopped ----------------------";
}

void TcpServer::CreateRemoteClient()
{

}

bool TcpServer::ServerUp()
{
  return server_->isListening();
}

