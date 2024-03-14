#ifndef OUTGOINGMSG_H
#define OUTGOINGMSG_H

#include <QObject>
#include "core/application.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "qtcpsocket.h"
#include "networkremote/RemoteMessages.pb.h"

class OutgoingMsg : public QObject
{
     Q_OBJECT
public:
  explicit OutgoingMsg(Application *app, QObject *parent = nullptr);
  ~OutgoingMsg();
  void Init(QTcpSocket*, SharedPtr<Player>);
  void SendCurrentTrackInfo();
  void SendMsg();

private:
  Application *app_;
  PlaylistItemPtr currentItem_;
  Playlist *playlist_;
  QTcpSocket *socket_;
  qint32 msgType_;
  QByteArray msgStream_;
  nw::remote::Message *msg_;
  long bytesOut_;
  std::string msgString_;
  nw::remote::SongMetadata *song_;
  nw::remote::ResponseSongMetadata *responeSong_;
  SharedPtr<Player> player_ ;
  bool statusOk_;

};

#endif // OUTGOINGMSG_H
