#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

#include "incomingmsg.h"
#include "outgoingmsg.h"

class Application;

class Client : public QObject
{
     Q_OBJECT
public:
    explicit Client(Application *app, QObject *parent = nullptr);
    ~Client();
    void Init(QTcpSocket*);
    QTcpSocket* GetSocket();

signals:

private:
  Application *app_;
  QTcpSocket *socket_;
  IncomingMsg *msgReceived_;
  OutgoingMsg *newMsg_;
};

#endif // CLIENT_H
