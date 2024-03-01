#include "client.h"

Client::Client(Application *app, QObject *parent)
    : QObject{parent},
      app_(app),
      msgReceived_(new IncomingMsg(app)),
      newMsg_(new OutgoingMsg(app))
{
}

Client::~Client()
{
  msgReceived_->deleteLater();
  newMsg_->deleteLater();
}

void Client::Init(QTcpSocket *socket)
{
  socket_ = socket;
  QObject::connect(msgReceived_,&IncomingMsg::InMsgParsed,this, &Client::Respond);

  msgReceived_->Init(socket_);
}

QTcpSocket* Client::GetSocket()
{
  return socket_;
}

void Client::Respond()
{
  newMsg_->ProcessMsg(socket_, msgReceived_->GetMsgType());
}

