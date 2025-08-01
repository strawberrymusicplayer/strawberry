#ifndef REMOTESETTINGS_H
#define REMOTESETTINGS_H

#include <QObject>
#include "core/settings.h"

class NetworkRemoteSettings
{
public:
  static const char *kSettingsGroup;
  explicit NetworkRemoteSettings();
  ~NetworkRemoteSettings();
  void Load();
  void Save();
  bool UserRemote();
  bool LocalOnly();
  QString GetIpAddress();
  int GetPort();
  void SetUseRemote(bool);
  void SetLocalOnly(bool);
  void SetIpAdress ();
  void SetPort(int);

private:
  QSettings s_;
  bool enabled_;
  bool local_only_;
  int remote_port_;
  QString ipAddr_;
};

#endif // REMOTESETTINGS_H
