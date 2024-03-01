#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

#include "incomingmsg.h"
#include "outgoingmsg.h"
#include "core/player.h"

class Application;

class Client : public QObject
{
     Q_OBJECT
public:
    explicit Client(Application *app, QObject *parent = nullptr);
    ~Client();
    void Init(QTcpSocket*);
    QTcpSocket* GetSocket();

public slots:
  void Respond();

signals:
  void ReceiveMsg();
  void PrepareResponse();

private:
  Application *app_;
  QTcpSocket *socket_;
  IncomingMsg *msgReceived_;
  OutgoingMsg *newMsg_;
};

#endif // CLIENT_H
