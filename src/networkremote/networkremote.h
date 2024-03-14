#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>

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

public slots:
  void Init();
  void Update();
  void LoadSettings();
  void startTcpServer();
  void stopTcpServer();

private:
  Application *app_;
  bool use_remote_;
  bool local_only_;
  int remote_port_;
  QHostAddress ipAddr_;
  TcpServer *server_;
  QThread *original_thread_;
  static NetworkRemote* sInstance;
  RemoteSettings *s_ = new RemoteSettings;

};

#endif // NETWORKREMOTE_H
