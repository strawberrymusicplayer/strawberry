#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "core/shared_ptr.h"
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class TcpServer : public QTcpServer
{
     Q_OBJECT
public:
    static const char *kSettingsGroup;

    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

signals:

public slots:
  void SetupServer();
  void StartServer();
  void AcceptConnections();
  void StopServer();
  void CreateRemoteClient();

private:
  SharedPtr<QTcpServer> server_;
  bool use_remote_;
  bool use_local_only_;
  qint16 port_;

};

#endif // TCPSERVER_H
