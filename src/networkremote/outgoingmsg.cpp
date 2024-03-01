#include "outgoingmsg.h"
#include "core/player.h"
#include "core/logging.h"
#include "playlist/playlistmanager.h"

OutgoingMsg::OutgoingMsg(Application *app, QObject *parent)
    : QObject{parent},
      app_(app)
{
}

OutgoingMsg::~OutgoingMsg()
{
}

void OutgoingMsg::ProcessMsg(QTcpSocket * socket, qint32 msgType)
{
  socket_ = socket;
  msgType_ = msgType;

  switch (msgType_) {
    case nw::remote::MSG_TYPE_CONNECT:
      SendCurrentTrackInfo();
      break;
  default:
      break;
}
}

void OutgoingMsg::SendCurrentTrackInfo()
{
  currentItem_ = app_->playlist_manager()->active()->current_item();

  qLog(Debug) << "Current item " << &currentItem_->Metadata().albumartist();
}
