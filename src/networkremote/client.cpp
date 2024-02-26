#include "client.h"

Client::Client(Application *app, QObject *parent)
    : QObject{parent},
      app_(app),
      msgReceived_(new IncomingMsg),
      newMsg_(new OutgoingMsg)
{
}

Client::~Client()
{
}

void Client::Init(QTcpSocket *socket)
{
  socket_ = socket;
  msgReceived_->Init(socket_);
}

QTcpSocket* Client::GetSocket()
{
  return socket_;
}

