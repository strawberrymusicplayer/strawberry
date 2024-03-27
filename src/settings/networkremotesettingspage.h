#ifndef NETWORKREMOTESETTINGSPAGE_H
#define NETWORKREMOTESETTINGSPAGE_H

#include <QWidget>
#include <QObject>

#include "settingspage.h"
#include "networkremote/remotesettings.h"

class SettingsDialog;
class Ui_NetworkRemoteSettingsPage;
class NetworkRemote;

class NetworkRemoteSettingsPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
    ~NetworkRemoteSettingsPage() override;
    void Load() override;
    void Save() override;
    void Refresh();

signals:
  void remoteSettingsChanged();

private:
  Ui_NetworkRemoteSettingsPage *ui_;
  RemoteSettings *s_ = new RemoteSettings;

private slots:
  void RemoteButtonClicked();
  void LocalConnectButtonClicked();
  void PortChanged();
  void DisplayIP();
};

#endif // NETWORKREMOTESETTINGSPAGE_H
