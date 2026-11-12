// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TAGLIBQFILESTREAM_H
#define TAGLIBQFILESTREAM_H

#include <limits>
#include <QFile>
#include <taglib/tiostream.h>
#include "core/logging.h"

// Network file engines have no native descriptor. Keep TagLib's random reads on QFile.
class TagLibQFileStream : public TagLib::IOStream {
 public:
  explicit TagLibQFileStream(const QString &path) : file_(path), name_(QFile::encodeName(path)) {
    if (!file_.open(QIODevice::ReadOnly)) qLog(Error) << "Could not open NAS metadata stream" << file_.errorString();
  }
  TagLib::FileName name() const override { return name_.constData(); }
  TagLib::ByteVector readBlock(unsigned long count) override {
    if (count > static_cast<unsigned long>(std::numeric_limits<int>::max())) {
      qLog(Error) << "NAS metadata block is too large";
      return {};
    }
    const QByteArray data = file_.read(static_cast<qint64>(count));
    if (file_.error() != QFile::NoError) qLog(Error) << "Could not read NAS metadata" << file_.errorString();
    return TagLib::ByteVector(data.constData(), static_cast<unsigned int>(data.size()));
  }
  bool readOnly() const override { return true; }
  bool isOpen() const override { return file_.isOpen(); }
  void seek(long offset, Position origin) override {
    const qint64 base = origin == Beginning ? 0 : (origin == Current ? file_.pos() : file_.size());
    if (base < 0 || (offset > 0 && base > std::numeric_limits<qint64>::max() - offset)) {
      qLog(Error) << "Invalid NAS metadata seek";
      return;
    }
    if (!file_.seek(base + offset)) qLog(Error) << "Could not seek NAS metadata" << file_.errorString();
  }
  long tell() const override { return static_cast<long>(file_.pos()); }
  long length() override { return static_cast<long>(file_.size()); }
  void writeBlock(const TagLib::ByteVector &) override { ReadOnlyError(); }
  void insert(const TagLib::ByteVector &, unsigned long, unsigned long) override { ReadOnlyError(); }
  void removeBlock(unsigned long, unsigned long) override { ReadOnlyError(); }
  void truncate(long) override { ReadOnlyError(); }

 private:
  void ReadOnlyError() { qLog(Error) << "NAS metadata streams are read-only"; }
  QFile file_;
  QByteArray name_;
};

#endif
