// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstring>
#include <unistd.h>

#include <QFile>
#include <taglib/tfilestream.h>

#include "core/logging.h"
#include "taglibfilestream_android.h"

namespace {

int OpenDescriptor(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open document for metadata" << filename << file.errorString();
    return -1;
  }
  const int fd = dup(file.handle());
  if (fd < 0) qLog(Error) << "Could not duplicate metadata descriptor" << filename << strerror(errno);
  return fd;
}

class AndroidTagLibStream : public TagLib::FileStream {
 public:
  explicit AndroidTagLibStream(const QString &filename)
      : TagLib::FileStream(OpenDescriptor(filename), true), filename_(QFile::encodeName(filename)) {}

  // Preserve the extension for TagLib's format selection, rather than exposing an unnamed descriptor.
  TagLib::FileName name() const override { return filename_.constData(); }

 private:
  const QByteArray filename_;
};

}  // namespace

TagLib::IOStream *CreateAndroidTagLibStream(const QString &filename) {
  return new AndroidTagLibStream(filename);
}
