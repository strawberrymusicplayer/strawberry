#include "tcpserver.h"
#include "core/logging.h"

TcpServer::TcpServer(QObject *parent)
    : QObject{parent}
{
  server_ = new QTcpServer(this);
  connect(server_, SIGNAL(newConnection()),this,SLOT(newConnection()));
}

TcpServer::~TcpServer()
{
}

void TcpServer::StartServer(QHostAddress ipAddr, int port)
{
  bool ok = false;

  ok = server_->listen(ipAddr, port);
  if (ok){
    qLog(Debug) << "Server Started";
  }
}

void TcpServer::NewConnection()
{
  //QTcpSocket *socket = server_->nextPendingConnection();
  socket_ = server_->nextPendingConnection();
  qLog(Debug) << "New Socket";
  qLog(Debug) << socket_->currentReadChannel();
}

void TcpServer::StopServer()
{
  server_->close();
}

void TcpServer::CreateRemoteClient()
{

}

bool TcpServer::ServerUp()
{
  return server_->isListening();
}

