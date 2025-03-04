#include "incomingmsg.h"
#include "core/logging.h"
#include "core/player.h"

IncomingMsg::IncomingMsg(Application *app, QObject *parent)
    : QObject{parent},
      msg_(new nw::remote::Message),
      app_(app)
{
}

void IncomingMsg::Init(QTcpSocket *socket)
{
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &IncomingMsg::ReadyRead);
}

void IncomingMsg::SetMsgType()
{
  msgString_ = msgStream_.toStdString();
  msg_->ParseFromString(msgString_);
  Q_EMIT InMsgParsed();
}

qint32 IncomingMsg::GetMsgType()
{
  return msg_->type();
}

void IncomingMsg::ReadyRead()
{
   qLog(Debug) << "Ready To Read";
  msgStream_ = socket_->readAll();
  if (msgStream_.length() > 0) SetMsgType();
}
