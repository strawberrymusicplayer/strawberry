#ifndef MAC_STARTUP_H
#define MAC_STARTUP_H

#include "config.h"

#include <QString>
#include <QKeySequence>

class QObject;
class QWidget;

class GlobalShortcutBackendMacOS;

class PlatformInterface {
 public:
  // Called when the application should show itself.
  virtual void Activate() = 0;
  virtual bool LoadUrl(const QString &url) = 0;

  virtual ~PlatformInterface() {}
};

namespace mac {

void MacMain();
void SetShortcutHandler(GlobalShortcutBackendMacOS *handler);
void SetApplicationHandler(PlatformInterface *handler);
void CheckForUpdates();

QString GetBundlePath();
QString GetResourcesPath();
QString GetApplicationSupportPath();
QString GetMusicDirectory();

void EnableFullScreen(const QWidget &main_window);

}  // namespace mac

#endif
