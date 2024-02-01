#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include "tcpserver.h"

class Application;
class QThread;

class NetworkRemote : public QObject
{
     Q_OBJECT
public:
    static const char* kSettingsGroup;
    explicit NetworkRemote(Application* app, QObject *parent = nullptr);
    ~NetworkRemote() override;

public slots:
  void Init();
  void startTcpServer();
  void stopTcpServer();

private:
  Application *app_;
  bool use_remote_;
  bool local_only_;
  int remote_port_;
  QHostAddress ipAddr_;
  TcpServer *server_ = new TcpServer();
  QThread *original_thread_;
};

#endif // NETWORKREMOTE_H
