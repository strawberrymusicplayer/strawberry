#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include "networkremote/clientmanager.h"

class Application;

class TcpServer : public QObject
{
     Q_OBJECT
public:
    static const char *kSettingsGroup;

    explicit TcpServer(Application* app, QObject *parent = nullptr);
    ~TcpServer();

  bool ServerUp();

public slots:
  void NewTcpConnection();
  void StartServer(QHostAddress ipAddr, int port);
  void StopServer();

signals:

private:
  Application *app_;
  QTcpServer *server_;
  QTcpSocket *socket_;
  ClientManager *clientMgr_;
};

#endif // TCPSERVER_H
