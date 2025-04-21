#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QHostAddress>

#include "tcpserver.h"
#include "networkremote/remotesettings.h"

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

public Q_SLOTS:
  void Init();
  void Update();
  void LoadSettings();
  void startTcpServer();
  void stopTcpServer();

private:
  Application *app_;
  bool enabled_;
  bool local_only_;
  int remote_port_;
  QHostAddress ipAddr_;
  NetworkRemoteTcpServer *server_;
  QThread *original_thread_;
  static NetworkRemote* sInstance_;
  NetworkRemoteSettings* settings_;
};

#endif // NETWORKREMOTE_H
