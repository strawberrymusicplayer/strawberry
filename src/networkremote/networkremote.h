#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>
#include "tcpserver.h"

class Application;
class QThread;

class NetworkRemote : public QObject
{
     Q_OBJECT
public:
    static const char* kSettingsGroup;
    explicit NetworkRemote(Application* app, QObject *parent = nullptr);
    static NetworkRemote* Instance();
    ~NetworkRemote() override;

public slots:
  void Init();
  void Update();
  void startTcpServer();
  void stopTcpServer();

private:
  Application *app_;
  QSettings s_;
  bool use_remote_;
  bool local_only_;
  int remote_port_;
  QHostAddress ipAddr_;
  TcpServer *server_ = new TcpServer();
  QThread *original_thread_;
  static NetworkRemote* sInstance;
};

#endif // NETWORKREMOTE_H
