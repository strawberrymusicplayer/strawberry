#ifndef MAC_STARTUP_H
#define MAC_STARTUP_H

#include "config.h"

#include <QString>
#include <QKeySequence>

class QObject;
class QWidget;

class GlobalShortcutsBackendMacOS;

class PlatformInterface {
 public:
  PlatformInterface() = default;
  virtual ~PlatformInterface() {}

  // Called when the application should show itself.
  virtual void Activate() = 0;
  virtual bool LoadUrl(const QString &url) = 0;

 private:
  Q_DISABLE_COPY(PlatformInterface)
};

namespace mac {

void MacMain();
void SetShortcutHandler(GlobalShortcutsBackendMacOS *handler);
void SetApplicationHandler(PlatformInterface *handler);

void EnableFullScreen(const QWidget &main_window);

}  // namespace mac

#endif
