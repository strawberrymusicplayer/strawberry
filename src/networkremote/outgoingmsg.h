#ifndef OUTGOINGMSG_H
#define OUTGOINGMSG_H

#include <QObject>
#include <QByteArray>
#include "playlist/playlistitem.h"
#include "includes/shared_ptr.h"
#include "networkremote/RemoteMessages.pb.h"

class Application;
class Playlist;
class Player;
class QTcpSocket;

namespace nw {namespace remote {
  class Message;
  class RequestSongMetadata;
  class ResponseSongMetadata;  
}}


class NetworkRemoteOutgoingMsg : public QObject
{
     Q_OBJECT
public:
  explicit NetworkRemoteOutgoingMsg(Application *app, QObject *parent = nullptr);
  ~NetworkRemoteOutgoingMsg();
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
