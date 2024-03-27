#ifndef REMOTESETTINGS_H
#define REMOTESETTINGS_H

#include <QObject>
#include <QSettings>

class RemoteSettings : public QObject
{
     Q_OBJECT
public:
    static const char *kSettingsGroup;
    explicit RemoteSettings(QObject *parent = nullptr);
    ~RemoteSettings();
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
  bool use_remote_ = false;
  bool local_only_ = false;
  int remote_port_ = 5050;
  QString ipAddr_ = "0.0.0.0";
};

#endif // REMOTESETTINGS_H
