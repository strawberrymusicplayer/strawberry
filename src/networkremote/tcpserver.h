#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>

class TcpServer : public QObject
{
     Q_OBJECT
public:
    static const char *kSettingsGroup;

    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

  bool ServerUp();

public slots:
  void NewTcpConnection();
  void StartServer(QHostAddress ipAddr, int port);
  void StopServer();
  void CreateRemoteClient();


signals:

private:
  QTcpServer *server_;
  QTcpSocket *socket_;
};

#endif // TCPSERVER_H
