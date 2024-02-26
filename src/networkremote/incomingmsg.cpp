#include "incomingmsg.h"
#include "core/logging.h"

IncomingMsg::IncomingMsg(QObject *parent)
    : QObject{parent},
      msg_(new nw::remote::Message)
{
}

void IncomingMsg::Init(QTcpSocket *socket)
{
  QObject::connect(socket, &QAbstractSocket::readyRead, this, &IncomingMsg::ReadyRead);
}

void IncomingMsg::ProcessMsg()
{

}

void IncomingMsg::ReadyRead()
{
   qLog(Debug) << "NetworkRemote Init() ";
}
