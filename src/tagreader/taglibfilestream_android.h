// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TAGLIBFILESTREAM_ANDROID_H
#define TAGLIBFILESTREAM_ANDROID_H

#include <QString>
#include <taglib/tiostream.h>

TagLib::IOStream *CreateAndroidTagLibStream(const QString &filename);

#endif
