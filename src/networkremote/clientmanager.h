#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QList>

class Application;
class NetworkRemoteClient;

class NetworkRemoteClientManager : public QObject
{
  Q_OBJECT
public:
  explicit NetworkRemoteClientManager(Application *app, QObject *parent = nullptr);
  ~NetworkRemoteClientManager();
  void AddClient(QTcpSocket *socket);

private Q_SLOTS:
  void RemoveClient(NetworkRemoteClient *client);
  void Error(QAbstractSocket::SocketError socketError);
  void StateChanged();

private:
  Application *app_;
  QList<NetworkRemoteClient*> clients_;
};

#endif // CLIENTMANAGER_H
