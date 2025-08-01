#include "client.h"
#include "core/application.h"

NetworkRemoteClient::NetworkRemoteClient(Application *app, QObject *parent)
  : QObject(parent),
    app_(app),
    incomingMsg_(new NetworkRemoteIncomingMsg(this)),
    outgoingMsg_(new NetworkRemoteOutgoingMsg(app, this))
{
  player_ = app_->player();
}

NetworkRemoteClient::~NetworkRemoteClient()
{
}

void NetworkRemoteClient::Init(QTcpSocket *socket)
{
  socket_ = socket;
  QObject::connect(incomingMsg_,&NetworkRemoteIncomingMsg::InMsgParsed,this, &NetworkRemoteClient::ProcessIncoming);
  incomingMsg_->Init(socket_);
  outgoingMsg_->Init(socket_, player_);
}

QTcpSocket* NetworkRemoteClient::GetSocket()
{
  return socket_;
}

void NetworkRemoteClient::ProcessIncoming()
{
  switch (incomingMsg_->GetMsgType())
  {
    case nw::remote::MSG_TYPE_REQUEST_SONG_INFO:
      outgoingMsg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PLAY:
      player_->Play();
      // In case the player was paused when the client started send the song info again
      outgoingMsg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_NEXT:
      player_->Next();
      outgoingMsg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PREVIOUS:
      player_->Previous();
      outgoingMsg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PAUSE:
      player_->Pause();
      break;
    case nw::remote::MSG_TYPE_REQUEST_STOP:
      break;
    case nw::remote::MSG_TYPE_REQUEST_FINISH:
      Q_EMIT ClientIsLeaving();
      break;
    case nw::remote::MSG_TYPE_DISCONNECT:
      break;
    default:
        qInfo("Unknown mwessage type");
      break;
  }
}

