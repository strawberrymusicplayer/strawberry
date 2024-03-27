#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QVector>
#include "networkremote/client.h"

class Application;

class ClientManager : public QObject
{
     Q_OBJECT
public:
    explicit ClientManager(Application *app, QObject *parent = nullptr);
    ~ClientManager();
    void AddClient(QTcpSocket *socket);
    void RemoveClient();

private slots:
  void Ready();
  void Error(QAbstractSocket::SocketError);
  void StateChanged();

private:
  Application *app_;
  QVector<Client*> *clients_;
  Client *client_ = nullptr;
  QTcpSocket *socket_ = nullptr;
};

#endif // CLIENTMANAGER_H
