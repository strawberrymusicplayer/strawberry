#include "incomingmsg.h"
#include <QTcpSocket>
#include "networkremote/RemoteMessages.pb.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"

NetworkRemoteIncomingMsg::NetworkRemoteIncomingMsg(Application *app, QObject *parent)
  : QObject(parent),
    msg_(new nw::remote::Message),
    socket_(nullptr),
    bytesIn_(0),
    app_(app),
    msgType_(0)
{
}

NetworkRemoteIncomingMsg::~NetworkRemoteIncomingMsg()
{
  delete msg_;
}

void NetworkRemoteIncomingMsg::Init(QTcpSocket *socket)
{
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &NetworkRemoteIncomingMsg::ReadyRead);
}

void NetworkRemoteIncomingMsg::SetMsgType()
{
  msgString_ = msgStream_.toStdString();
  msg_->ParseFromString(msgString_);
  Q_EMIT InMsgParsed();
}

qint32 NetworkRemoteIncomingMsg::GetMsgType()
{
  return msg_->type();
}

void NetworkRemoteIncomingMsg::ReadyRead()
{
  qLog(Debug) << "Ready To Read";
  msgStream_ = socket_->readAll();
  if (msgStream_.length() > 0) SetMsgType();
}
