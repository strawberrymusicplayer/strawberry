/***************************************************************************
    copyright            : (C) 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include "tstring.h"
#include "tdebug.h"
#include "aifffile.h"
#include "aiffproperties.h"

using namespace Strawberry_TagLib::TagLib;

class RIFF::AIFF::AudioProperties::AudioPropertiesPrivate {
 public:
  AudioPropertiesPrivate() : length(0),
                        bitrate(0),
                        sampleRate(0),
                        channels(0),
                        bitsPerSample(0),
                        sampleFrames(0) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;

  ByteVector compressionType;
  String compressionName;

  unsigned int sampleFrames;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::AIFF::AudioProperties::AudioProperties(File *file, ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
  read(file);
}

RIFF::AIFF::AudioProperties::~AudioProperties() {
  delete d;
}

int RIFF::AIFF::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int RIFF::AIFF::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int RIFF::AIFF::AudioProperties::bitrate() const {
  return d->bitrate;
}

int RIFF::AIFF::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int RIFF::AIFF::AudioProperties::channels() const {
  return d->channels;
}

int RIFF::AIFF::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

unsigned int RIFF::AIFF::AudioProperties::sampleFrames() const {
  return d->sampleFrames;
}

bool RIFF::AIFF::AudioProperties::isAiffC() const {
  return (!d->compressionType.isEmpty());
}

ByteVector RIFF::AIFF::AudioProperties::compressionType() const {
  return d->compressionType;
}

String RIFF::AIFF::AudioProperties::compressionName() const {
  return d->compressionName;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void RIFF::AIFF::AudioProperties::read(File *file) {

  ByteVector data;
  unsigned int streamLength = 0;
  for (unsigned int i = 0; i < file->chunkCount(); i++) {
    const ByteVector name = file->chunkName(i);
    if (name == "COMM") {
      if (data.isEmpty())
        data = file->chunkData(i);
      else
        debug("RIFF::AIFF::AudioProperties::read() - Duplicate 'COMM' chunk found.");
    }
    else if (name == "SSND") {
      if (streamLength == 0)
        streamLength = file->chunkDataSize(i) + file->chunkPadding(i);
      else
        debug("RIFF::AIFF::AudioProperties::read() - Duplicate 'SSND' chunk found.");
    }
  }

  if (data.size() < 18) {
    debug("RIFF::AIFF::AudioProperties::read() - 'COMM' chunk not found or too short.");
    return;
  }

  if (streamLength == 0) {
    debug("RIFF::AIFF::AudioProperties::read() - 'SSND' chunk not found.");
    return;
  }

  d->channels = data.toUInt16BE(0);
  d->sampleFrames = data.toUInt32BE(2);
  d->bitsPerSample = data.toUInt16BE(6);

  const long double sampleRate = data.toFloat80BE(8);
  if (sampleRate >= 1.0)
    d->sampleRate = static_cast<int>(sampleRate + 0.5);

  if (d->sampleFrames > 0 && d->sampleRate > 0) {
    const double length = d->sampleFrames * 1000.0 / sampleRate;
    d->length = static_cast<int>(length + 0.5);
    d->bitrate = static_cast<int>(streamLength * 8.0 / length + 0.5);
  }

  if (data.size() >= 23) {
    d->compressionType = data.mid(18, 4);
    d->compressionName = String(data.mid(23, static_cast<unsigned char>(data[22])), String::Latin1);
  }

}
