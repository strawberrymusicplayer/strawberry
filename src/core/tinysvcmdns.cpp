extern "C" {
#include "mdnsd.h"
}

#include <QObject>
#include <QList>
#include <QString>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QtEndian>

#include "tinysvcmdns.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

TinySVCMDNS::TinySVCMDNS(QObject *parent) : Zeroconf(parent) {

  // Get all network interfaces
  const QList<QNetworkInterface> network_interfaces = QNetworkInterface::allInterfaces();
  for (const QNetworkInterface &network_interface : network_interfaces) {
    // Only use up and non loopback interfaces
    if (network_interface.flags().testFlag(network_interface.IsUp) && !network_interface.flags().testFlag(network_interface.IsLoopBack)) {

      qLog(Debug) << "Interface" << network_interface.humanReadableName();

      uint32_t ipv4 = 0;
      QString ipv6;

      // Now check all network addresses for this device
      QList<QNetworkAddressEntry> network_address_entries = network_interface.addressEntries();

      for (QNetworkAddressEntry network_address_entry : network_address_entries) {
        QHostAddress host_address = network_address_entry.ip();
        if (host_address.protocol() == QAbstractSocket::IPv4Protocol) {
          ipv4 = qToBigEndian(host_address.toIPv4Address());
          qLog(Debug) << "  ipv4:" << host_address.toString();
        }
        else if (host_address.protocol() == QAbstractSocket::IPv6Protocol) {
          ipv6 = host_address.toString();
          qLog(Debug) << "  ipv6:" << host_address.toString();
        }
      }

      // Now start the service
      CreateMdnsd(ipv4, ipv6);
    }
  }

}

TinySVCMDNS::~TinySVCMDNS() {

  for (mdnsd *mdnsd : std::as_const(mdnsd_)) {
    mdnsd_stop(mdnsd);
  }

}

void TinySVCMDNS::CreateMdnsd(const uint32_t ipv4, const QString &ipv6) {

  const QString host = QHostInfo::localHostName();

  // Start the service
  mdnsd *mdnsd = mdnsd_start();

  // Set our hostname
  const QString fullhostname = host + ".local"_L1;
  mdnsd_set_hostname(mdnsd, fullhostname.toUtf8().constData(), ipv4);

  // Add to the list
  mdnsd_.append(mdnsd);

}

void TinySVCMDNS::PublishInternal(const QString &domain, const QString &type, const QByteArray &name, const quint16 port) {

  // Some pointless text, so tinymDNS publishes the service correctly.
  const char *txt[] = { "cat=nyan", nullptr };

  for (mdnsd *mdnsd : mdnsd_) {
    const QString fulltype = type + ".local"_L1;
    mdnsd_register_svc(mdnsd, name.constData(), fulltype.toUtf8().constData(), port, nullptr, txt);
  }

}
