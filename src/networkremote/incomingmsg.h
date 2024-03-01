#ifndef INCOMINGMSG_H
#define INCOMINGMSG_H

#include <QObject>
#include <QTcpSocket>
#include "networkremote/RemoteMessages.pb.h"
#include "core/application.h"
#include "playlist/playlistitem.h"

class IncomingMsg : public QObject
{
     Q_OBJECT
public:
  explicit IncomingMsg(Application *app, QObject *parent = nullptr);
  void Init(QTcpSocket*);
  void SetMsgType();
  qint32 GetMsgType();

private slots:
  void ReadyRead();

signals:
  void InMsgParsed();

private:
  nw::remote::Message *msg_;
  QTcpSocket *socket_;
  long bytesIn_;
  QByteArray msgStream_;
  std::string msgString_;
  Application *app_;

  qint32 msgType_;
};

#endif // INCOMINGMSG_H