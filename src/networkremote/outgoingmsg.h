#ifndef OUTGOINGMSG_H
#define OUTGOINGMSG_H

#include <QObject>
#include "core/application.h"
#include "playlist/playlistitem.h"
#include "qtcpsocket.h"
#include "networkremote/RemoteMessages.pb.h"

class OutgoingMsg : public QObject
{
     Q_OBJECT
public:
  explicit OutgoingMsg(Application *app, QObject *parent = nullptr);
  ~OutgoingMsg()  ;
  void ProcessMsg(QTcpSocket*, qint32);
  void SendCurrentTrackInfo();

private slots:


signals:

private:

  Application *app_;
  PlaylistItemPtr currentItem_;
  QTcpSocket *socket_;
  qint32 msgType_;
  QByteArray msgStream_;

};

#endif // OUTGOINGMSG_H
