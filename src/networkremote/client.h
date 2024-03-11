#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

#include "incomingmsg.h"
#include "outgoingmsg.h"
#include "core/player.h"
#include "networkremote/RemoteMessages.pb.h"

class Application;

class Client : public QObject
{
     Q_OBJECT
public:
    explicit Client(Application *app, QObject *parent = nullptr);
    ~Client();
    void Init(QTcpSocket*);
    QTcpSocket* GetSocket();
    void ProcessIncoming();

public slots:
  void Respond();
  void EngineChanged();

signals:
  void ReceiveMsg();
  void PrepareResponse();

private:
  Application *app_;
  QTcpSocket *socket_;
  IncomingMsg *incomingMsg_;
  OutgoingMsg *outgoingMsg_;
  qint32 msgType_;
  SharedPtr<Player> player_;
};

#endif // CLIENT_H
