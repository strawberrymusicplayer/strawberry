#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include "incomingmsg.h"
#include "outgoingmsg.h"
#include "core/player.h"

class Application;

class NetworkRemoteClient : public QObject
{
  Q_OBJECT
public:
  explicit NetworkRemoteClient(Application *app, QObject *parent = nullptr);
  ~NetworkRemoteClient();
  void Init(QTcpSocket*);
  QTcpSocket* GetSocket();
  void ProcessIncoming();

Q_SIGNALS:
  void ReceiveMsg();
  void PrepareResponse();
  void ClientIsLeaving();

private:
  Application *app_;
  QTcpSocket *socket_;
  NetworkRemoteIncomingMsg *incomingMsg_;
  NetworkRemoteOutgoingMsg *outgoingMsg_;
  SharedPtr<Player> player_;
};

#endif // CLIENT_H
