#ifndef INCOMINGMSG_H
#define INCOMINGMSG_H

#include <QObject>
#include <QTcpSocket>
#include "networkremote/RemoteMessages.pb.h"

class IncomingMsg : public QObject
{
     Q_OBJECT
public:
  explicit IncomingMsg(QObject *parent = nullptr);
  void Init(QTcpSocket*);
  void ProcessMsg();

private slots:
  void ReadyRead();

signals:

private:
  nw::remote::Message *msg_;

};

#endif // INCOMINGMSG_H
