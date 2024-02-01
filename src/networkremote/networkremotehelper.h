#ifndef NETWORKREMOTEHELPER_H
#define NETWORKREMOTEHELPER_H

#include <QThread>

#include "networkremote.h"

class Application;

class NetworkRemoteHelper : public QObject {
  Q_OBJECT
 public:
  static NetworkRemoteHelper* Instance();

  NetworkRemoteHelper(Application* app, QObject *parent = nullptr);
  ~NetworkRemoteHelper();

  void ReloadSettings();

 signals:
  void ReloadSettingsSig();

 private:
  static NetworkRemoteHelper* sInstance;
  Application* app_;
};

#endif  // NETWORKREMOTEHELPER_H
