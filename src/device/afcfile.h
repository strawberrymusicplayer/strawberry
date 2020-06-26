#ifndef AFCFILE_H
#define AFCFILE_H

#include "config.h"

#include <cstdint>
#include <libimobiledevice/afc.h>

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QString>

class iMobileDeviceConnection;

class AfcFile : public QIODevice {
  Q_OBJECT

 public:
  explicit AfcFile(iMobileDeviceConnection* connection, const QString &path, QObject *parent = nullptr);
  ~AfcFile() override;

  // QIODevice
  void close() override;
  bool open(OpenMode mode) override;
  bool seek(qint64 pos) override;
  qint64 size() const override;

 private:
  // QIODevice
  qint64 readData(char *data, qint64 max_size) override;
  qint64 writeData(const char *data, qint64 max_size) override;

  iMobileDeviceConnection *connection_;
  uint64_t handle_;

  QString path_;
};

#endif
