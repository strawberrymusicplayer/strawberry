#ifndef TINYSVCMDNS_H
#define TINYSVCMDNS_H

#include <QList>
#include <QByteArray>
#include <QString>

#include "zeroconf.h"

struct mdnsd;

class TinySVCMDNS : public Zeroconf {

 public:
  explicit TinySVCMDNS(QObject *parent = nullptr);
  virtual ~TinySVCMDNS();

 protected:
  virtual void PublishInternal(const QString &domain, const QString &type, const QByteArray &name, const quint16 port) override;

 private:
  void CreateMdnsd(const uint32_t ipv4, const QString &ipv6);
  QList<mdnsd*> mdnsd_;
};

#endif  // TINYSVCMDNS_H
