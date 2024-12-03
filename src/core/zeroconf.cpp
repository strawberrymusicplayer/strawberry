#include "config.h"

#include <QObject>
#include <QByteArray>
#include <QString>

#ifdef HAVE_DBUS
#  include "avahi/avahi.h"
#endif

#ifdef Q_OS_DARWIN
#  include "bonjour.h"
#endif

#ifdef Q_OS_WIN32
#  include "tinysvcmdns.h"
#endif

#include "zeroconf.h"

Zeroconf *Zeroconf::sInstance = nullptr;

Zeroconf::Zeroconf(QObject *parent) : QObject(parent) {}

Zeroconf::~Zeroconf() = default;

Zeroconf *Zeroconf::GetZeroconf() {

  if (!sInstance) {
#ifdef HAVE_DBUS
    sInstance = new Avahi;
#endif  // HAVE_DBUS

#ifdef Q_OS_DARWIN
    sInstance = new Bonjour;
#endif

#ifdef Q_OS_WIN32
    sInstance = new TinySVCMDNS;
#endif
  }

  return sInstance;

}

QByteArray Zeroconf::TruncateName(const QString &name) {

  QByteArray truncated_utf8;
  for (const QChar c : name) {
    if (truncated_utf8.size() + 1 >= 63) {
      break;
    }
    truncated_utf8 += c.toLatin1();
  }

  // NULL-terminate the string.
  truncated_utf8.append('\0');

  return truncated_utf8;

}

void Zeroconf::Publish(const QString &domain, const QString &type, const QString &name, quint16 port) {

  const QByteArray truncated_name = TruncateName(name);
  PublishInternal(domain, type, truncated_name, port);

}
