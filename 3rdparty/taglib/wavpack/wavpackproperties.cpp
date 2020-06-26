/***************************************************************************
    copyright            : (C) 2006 by Lukáš Lalinský
    email                : lalinsky@gmail.com

    copyright            : (C) 2004 by Allan Sandfeld Jensen
    email                : kde@carewolf.org
                           (original MPC implementation)
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

#include "wavpackproperties.h"
#include "wavpackfile.h"

// Implementation of this class is based on the information at:
// http://www.wavpack.com/file_format.txt

using namespace Strawberry_TagLib::TagLib;

class WavPack::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : length(0),
                                      bitrate(0),
                                      sampleRate(0),
                                      channels(0),
                                      version(0),
                                      bitsPerSample(0),
                                      lossless(false),
                                      sampleFrames(0) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int version;
  int bitsPerSample;
  bool lossless;
  unsigned int sampleFrames;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

WavPack::AudioProperties::AudioProperties(File *file, long long streamLength, ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
  read(file, streamLength);
}

WavPack::AudioProperties::~AudioProperties() {
  delete d;
}

int WavPack::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int WavPack::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int WavPack::AudioProperties::bitrate() const {
  return d->bitrate;
}

int WavPack::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int WavPack::AudioProperties::channels() const {
  return d->channels;
}

int WavPack::AudioProperties::version() const {
  return d->version;
}

int WavPack::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

bool WavPack::AudioProperties::isLossless() const {
  return d->lossless;
}

unsigned int WavPack::AudioProperties::sampleFrames() const {
  return d->sampleFrames;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

namespace {
const unsigned int sample_rates[] = {
  6000, 8000, 9600, 11025, 12000, 16000, 22050, 24000,
  32000, 44100, 48000, 64000, 88200, 96000, 192000, 0
};
}

#define BYTES_STORED  3
#define MONO_FLAG     4
#define LOSSLESS_FLAG 8

#define SHIFT_LSB  13
#define SHIFT_MASK (0x1fL << SHIFT_LSB)

#define SRATE_LSB  23
#define SRATE_MASK (0xfL << SRATE_LSB)

#define MIN_STREAM_VERS 0x402
#define MAX_STREAM_VERS 0x410

#define FINAL_BLOCK 0x1000

void WavPack::AudioProperties::read(File *file, long long streamLength) {

  long offset = 0;

  while (true) {
    file->seek(offset);
    const ByteVector data = file->readBlock(32);

    if (data.size() < 32) {
      debug("WavPack::AudioProperties::read() -- data is too short.");
      break;
    }

    if (!data.startsWith("wvpk")) {
      debug("WavPack::AudioProperties::read() -- Block header not found.");
      break;
    }

    const unsigned int flags = data.toUInt32LE(24);

    if (offset == 0) {
      d->version = data.toUInt16LE(8);
      if (d->version < MIN_STREAM_VERS || d->version > MAX_STREAM_VERS)
        break;

      d->bitsPerSample = ((flags & BYTES_STORED) + 1) * 8 - ((flags & SHIFT_MASK) >> SHIFT_LSB);
      d->sampleRate = sample_rates[(flags & SRATE_MASK) >> SRATE_LSB];
      d->lossless = !(flags & LOSSLESS_FLAG);
      d->sampleFrames = data.toUInt32LE(12);
    }

    d->channels += (flags & MONO_FLAG) ? 1 : 2;

    if (flags & FINAL_BLOCK)
      break;

    const unsigned int blockSize = data.toUInt32LE(4);
    offset += blockSize + 8;
  }

  if (d->sampleFrames == ~0u)
    d->sampleFrames = seekFinalIndex(file, streamLength);

  if (d->sampleFrames > 0 && d->sampleRate > 0) {
    const double length = d->sampleFrames * 1000.0 / d->sampleRate;
    d->length = static_cast<int>(length + 0.5);
    d->bitrate = static_cast<int>(streamLength * 8.0 / length + 0.5);
  }

}

unsigned int WavPack::AudioProperties::seekFinalIndex(File *file, long long streamLength) {

  const long long offset = file->rfind("wvpk", streamLength);
  if (offset == -1)
    return 0;

  file->seek(offset);
  const ByteVector data = file->readBlock(32);
  if (data.size() < 32)
    return 0;

  const int version = data.toUInt16LE(8);
  if (version < MIN_STREAM_VERS || version > MAX_STREAM_VERS)
    return 0;

  const unsigned int flags = data.toUInt32LE(24);
  if (!(flags & FINAL_BLOCK))
    return 0;

  const unsigned int blockIndex = data.toUInt32LE(16);
  const unsigned int blockSamples = data.toUInt32LE(20);

  return blockIndex + blockSamples;

}
