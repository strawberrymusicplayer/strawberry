#include "client.h"

Client::Client(Application *app, QObject *parent)
    : QObject{parent},
      app_(app),
      incomingMsg_(new IncomingMsg(app)),
      outgoingMsg_(new OutgoingMsg(app))
{
}

Client::~Client()
{
  incomingMsg_->deleteLater();
  outgoingMsg_->deleteLater();
}

void Client::Init(QTcpSocket *socket)
{
  socket_ = socket;
  QObject::connect(incomingMsg_,&IncomingMsg::InMsgParsed,this, &Client::Respond);

  incomingMsg_->Init(socket_);
}

QTcpSocket* Client::GetSocket()
{
  return socket_;
}

void Client::Respond()
{
  outgoingMsg_->ProcessMsg(socket_, incomingMsg_->GetMsgType());
}

