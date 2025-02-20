#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include "networkremote/clientmanager.h"

class Application;

class NetworkRemoteTcpServer : public QObject
{
  Q_OBJECT
public:
  static const char *kSettingsGroup;
  explicit NetworkRemoteTcpServer(Application* app, QObject *parent = nullptr);
  ~NetworkRemoteTcpServer();
  bool ServerUp();

public Q_SLOTS:
  void NewTcpConnection();
  void StartServer(QHostAddress ipAddr, int port);
  void StopServer();

private:
  Application *app_;
  QTcpServer *server_;
  QTcpSocket *socket_;
  NetworkRemoteClientManager *clientMgr_;
};

#endif // TCPSERVER_H
