/***************************************************************************
    copyright            : (C) 2010 by Alex Novichkov
    email                : novichko@atnet.ru

    copyright            : (C) 2006 by Lukáš Lalinský
    email                : lalinsky@gmail.com
                           (original WavPack implementation)
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

#include "id3v2tag.h"
#include "apeproperties.h"
#include "apefile.h"
#include "apetag.h"
#include "apefooter.h"

using namespace Strawberry_TagLib::TagLib;

class APE::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : length(0),
                                      bitrate(0),
                                      sampleRate(0),
                                      channels(0),
                                      version(0),
                                      bitsPerSample(0),
                                      sampleFrames(0) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int version;
  int bitsPerSample;
  unsigned int sampleFrames;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

APE::AudioProperties::AudioProperties(File *file, long long streamLength, ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
  read(file, streamLength);
}

APE::AudioProperties::~AudioProperties() {
  delete d;
}

int APE::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int APE::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int APE::AudioProperties::bitrate() const {
  return d->bitrate;
}

int APE::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int APE::AudioProperties::channels() const {
  return d->channels;
}

int APE::AudioProperties::version() const {
  return d->version;
}

int APE::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

unsigned int APE::AudioProperties::sampleFrames() const {
  return d->sampleFrames;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

namespace {
int headerVersion(const ByteVector &header) {
  if (header.size() < 6 || !header.startsWith("MAC "))
    return -1;

  return header.toUInt16LE(4);
}
}  // namespace

void APE::AudioProperties::read(File *file, long long streamLength) {

  // First, we assume that the file pointer is set at the first descriptor.
  long long offset = file->tell();
  int version = headerVersion(file->readBlock(6));

  // Next, we look for the descriptor.
  if (version < 0) {
    offset = file->find("MAC ", offset);
    file->seek(offset);
    version = headerVersion(file->readBlock(6));
  }

  if (version < 0) {
    debug("APE::AudioProperties::read() -- APE descriptor not found");
    return;
  }

  d->version = version;

  if (d->version >= 3980)
    analyzeCurrent(file);
  else
    analyzeOld(file);

  if (d->sampleFrames > 0 && d->sampleRate > 0) {
    const double length = d->sampleFrames * 1000.0 / d->sampleRate;
    d->length = static_cast<int>(length + 0.5);
    d->bitrate = static_cast<int>(streamLength * 8.0 / length + 0.5);
  }

}

void APE::AudioProperties::analyzeCurrent(File *file) {

  // Read the descriptor
  file->seek(2, File::Current);
  const ByteVector descriptor = file->readBlock(44);
  if (descriptor.size() < 44) {
    debug("APE::AudioProperties::analyzeCurrent() -- descriptor is too short.");
    return;
  }

  const unsigned int descriptorBytes = descriptor.toUInt32LE(0);

  if ((descriptorBytes - 52) > 0)
    file->seek(descriptorBytes - 52, File::Current);

  // Read the header
  const ByteVector header = file->readBlock(24);
  if (header.size() < 24) {
    debug("APE::AudioProperties::analyzeCurrent() -- MAC header is too short.");
    return;
  }

  // Get the APE info
  d->channels = header.toUInt16LE(18);
  d->sampleRate = header.toUInt32LE(20);
  d->bitsPerSample = header.toUInt16LE(16);

  const unsigned int totalFrames = header.toUInt32LE(12);
  if (totalFrames == 0)
    return;

  const unsigned int blocksPerFrame = header.toUInt32LE(4);
  const unsigned int finalFrameBlocks = header.toUInt32LE(8);
  d->sampleFrames = (totalFrames - 1) * blocksPerFrame + finalFrameBlocks;

}

void APE::AudioProperties::analyzeOld(File *file) {

  const ByteVector header = file->readBlock(26);
  if (header.size() < 26) {
    debug("APE::AudioProperties::analyzeOld() -- MAC header is too short.");
    return;
  }

  const unsigned int totalFrames = header.toUInt32LE(18);

  // Fail on 0 length APE files (catches non-finalized APE files)
  if (totalFrames == 0)
    return;

  const short compressionLevel = header.toUInt32LE(0);
  unsigned int blocksPerFrame;
  if (d->version >= 3950)
    blocksPerFrame = 73728 * 4;
  else if (d->version >= 3900 || (d->version >= 3800 && compressionLevel == 4000))
    blocksPerFrame = 73728;
  else
    blocksPerFrame = 9216;

  // Get the APE info
  d->channels = header.toUInt16LE(4);
  d->sampleRate = header.toUInt32LE(6);

  const unsigned int finalFrameBlocks = header.toUInt32LE(22);
  d->sampleFrames = (totalFrames - 1) * blocksPerFrame + finalFrameBlocks;

  // Get the bit depth from the RIFF-fmt chunk.
  file->seek(16, File::Current);
  const ByteVector fmt = file->readBlock(28);
  if (fmt.size() < 28 || !fmt.startsWith("WAVEfmt ")) {
    debug("APE::AudioProperties::analyzeOld() -- fmt header is too short.");
    return;
  }

  d->bitsPerSample = fmt.toUInt16LE(26);

}
