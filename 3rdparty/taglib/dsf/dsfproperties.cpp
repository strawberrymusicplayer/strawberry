/***************************************************************************
 copyright            : (C) 2013 by Stephen F. Booth
 email                : me@sbooth.org
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

#include <tstring.h>
#include <tdebug.h>

#include "dsfproperties.h"

using namespace Strawberry_TagLib::TagLib;

class DSF::AudioProperties::AudioPropertiesPrivate {
 public:
  AudioPropertiesPrivate() : formatVersion(0),
                        formatID(0),
                        channelType(0),
                        channelNum(0),
                        samplingFrequency(0),
                        bitsPerSample(0),
                        sampleCount(0),
                        blockSizePerChannel(0),
                        bitrate(0),
                        length(0) {
  }

  // Nomenclature is from DSF file format specification
  unsigned int formatVersion;
  unsigned int formatID;
  unsigned int channelType;
  unsigned int channelNum;
  unsigned int samplingFrequency;
  unsigned int bitsPerSample;
  long long sampleCount;
  unsigned int blockSizePerChannel;

  // Computed
  unsigned int bitrate;
  unsigned int length;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

DSF::AudioProperties::AudioProperties(const ByteVector &data, ReadStyle style) : Strawberry_TagLib::TagLib::AudioProperties(style) {
  d = new AudioPropertiesPrivate;
  read(data);
}

DSF::AudioProperties::~AudioProperties() {
  delete d;
}

int DSF::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int DSF::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int DSF::AudioProperties::bitrate() const {
  return d->bitrate;
}

int DSF::AudioProperties::sampleRate() const {
  return d->samplingFrequency;
}

int DSF::AudioProperties::channels() const {
  return d->channelNum;
}

// DSF specific
int DSF::AudioProperties::formatVersion() const {
  return d->formatVersion;
}

int DSF::AudioProperties::formatID() const {
  return d->formatID;
}

int DSF::AudioProperties::channelType() const {
  return d->channelType;
}

int DSF::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

long long DSF::AudioProperties::sampleCount() const {
  return d->sampleCount;
}

int DSF::AudioProperties::blockSizePerChannel() const {
  return d->blockSizePerChannel;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void DSF::AudioProperties::read(const ByteVector &data) {
  d->formatVersion = data.toUInt(0U, false);
  d->formatID = data.toUInt(4U, false);
  d->channelType = data.toUInt(8U, false);
  d->channelNum = data.toUInt(12U, false);
  d->samplingFrequency = data.toUInt(16U, false);
  d->bitsPerSample = data.toUInt(20U, false);
  d->sampleCount = data.toLongLong(24U, false);
  d->blockSizePerChannel = data.toUInt(32U, false);

  d->bitrate = static_cast<unsigned int>((d->samplingFrequency * d->bitsPerSample * d->channelNum) / 1000.0 + 0.5);
  d->length = d->samplingFrequency > 0 ? static_cast<unsigned int>(d->sampleCount * 1000.0 / d->samplingFrequency + 0.5) : 0;
}
