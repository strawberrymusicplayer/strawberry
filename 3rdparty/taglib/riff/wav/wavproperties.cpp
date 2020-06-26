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

#include "tdebug.h"
#include "wavfile.h"
#include "wavproperties.h"

using namespace Strawberry_TagLib::TagLib;

namespace {
// Quoted from RFC 2361.
enum WaveFormat {
  FORMAT_UNKNOWN = 0x0000,
  FORMAT_PCM = 0x0001
};
}  // namespace

class RIFF::WAV::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : format(0),
                                      length(0),
                                      bitrate(0),
                                      sampleRate(0),
                                      channels(0),
                                      bitsPerSample(0),
                                      sampleFrames(0) {}

  int format;
  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;
  unsigned int sampleFrames;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Strawberry_TagLib::TagLib::RIFF::WAV::AudioProperties::AudioProperties(File *file, ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
  read(file);
}

RIFF::WAV::AudioProperties::~AudioProperties() {
  delete d;
}

int RIFF::WAV::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int RIFF::WAV::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int RIFF::WAV::AudioProperties::bitrate() const {
  return d->bitrate;
}

int RIFF::WAV::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int RIFF::WAV::AudioProperties::channels() const {
  return d->channels;
}

int RIFF::WAV::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

unsigned int RIFF::WAV::AudioProperties::sampleFrames() const {
  return d->sampleFrames;
}

int RIFF::WAV::AudioProperties::format() const {
  return d->format;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void RIFF::WAV::AudioProperties::read(File *file) {

  ByteVector data;
  unsigned int streamLength = 0;
  unsigned int totalSamples = 0;

  for (unsigned int i = 0; i < file->chunkCount(); ++i) {
    const ByteVector name = file->chunkName(i);
    if (name == "fmt ") {
      if (data.isEmpty())
        data = file->chunkData(i);
      else
        debug("RIFF::WAV::AudioProperties::read() - Duplicate 'fmt ' chunk found.");
    }
    else if (name == "data") {
      if (streamLength == 0)
        streamLength = file->chunkDataSize(i) + file->chunkPadding(i);
      else
        debug("RIFF::WAV::AudioProperties::read() - Duplicate 'data' chunk found.");
    }
    else if (name == "fact") {
      if (totalSamples == 0)
        totalSamples = file->chunkData(i).toUInt32LE(0);
      else
        debug("RIFF::WAV::AudioProperties::read() - Duplicate 'fact' chunk found.");
    }
  }

  if (data.size() < 16) {
    debug("RIFF::WAV::AudioProperties::read() - 'fmt ' chunk not found or too short.");
    return;
  }

  if (streamLength == 0) {
    debug("RIFF::WAV::AudioProperties::read() - 'data' chunk not found.");
    return;
  }

  d->format = data.toUInt16LE(0);
  if (d->format != FORMAT_PCM && totalSamples == 0) {
    debug("RIFF::WAV::AudioProperties::read() - Non-PCM format, but 'fact' chunk not found.");
    return;
  }

  d->channels = data.toUInt16LE(2);
  d->sampleRate = data.toUInt32LE(4);
  d->bitsPerSample = data.toUInt16LE(14);

  if (d->format != FORMAT_PCM)
    d->sampleFrames = totalSamples;
  else if (d->channels > 0 && d->bitsPerSample > 0)
    d->sampleFrames = streamLength / (d->channels * ((d->bitsPerSample + 7) / 8));

  if (d->sampleFrames > 0 && d->sampleRate > 0) {
    const double length = d->sampleFrames * 1000.0 / d->sampleRate;
    d->length = static_cast<int>(length + 0.5);
    d->bitrate = static_cast<int>(streamLength * 8.0 / length + 0.5);
  }
  else {
    const unsigned int byteRate = data.toUInt32LE(8);
    if (byteRate > 0) {
      d->length = static_cast<int>(streamLength * 1000.0 / byteRate + 0.5);
      d->bitrate = static_cast<int>(byteRate * 8.0 / 1000.0 + 0.5);
    }
  }

}
