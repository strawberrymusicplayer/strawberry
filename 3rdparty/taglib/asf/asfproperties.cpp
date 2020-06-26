/**************************************************************************
    copyright            : (C) 2005-2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

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
#include "tstring.h"
#include "asfproperties.h"

using namespace Strawberry_TagLib::TagLib;

class ASF::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : length(0),
                                       bitrate(0),
                                       sampleRate(0),
                                       channels(0),
                                       bitsPerSample(0),
                                       codec(ASF::AudioProperties::Unknown),
                                       encrypted(false) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;
  ASF::AudioProperties::Codec codec;
  String codecName;
  String codecDescription;
  bool encrypted;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

ASF::AudioProperties::AudioProperties() : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
}

ASF::AudioProperties::~AudioProperties() {
  delete d;
}

int ASF::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int ASF::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int ASF::AudioProperties::bitrate() const {
  return d->bitrate;
}

int ASF::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int ASF::AudioProperties::channels() const {
  return d->channels;
}

int ASF::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

ASF::AudioProperties::Codec ASF::AudioProperties::codec() const {
  return d->codec;
}

String ASF::AudioProperties::codecName() const {
  return d->codecName;
}

String ASF::AudioProperties::codecDescription() const {
  return d->codecDescription;
}

bool ASF::AudioProperties::isEncrypted() const {
  return d->encrypted;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void ASF::AudioProperties::setLengthInMilliseconds(int value) {
  d->length = value;
}

void ASF::AudioProperties::setBitrate(int value) {
  d->bitrate = value;
}

void ASF::AudioProperties::setSampleRate(int value) {
  d->sampleRate = value;
}

void ASF::AudioProperties::setChannels(int value) {
  d->channels = value;
}

void ASF::AudioProperties::setBitsPerSample(int value) {
  d->bitsPerSample = value;
}

void ASF::AudioProperties::setCodec(int value) {

  switch (value) {
    case 0x0160:
      d->codec = WMA1;
      break;
    case 0x0161:
      d->codec = WMA2;
      break;
    case 0x0162:
      d->codec = WMA9Pro;
      break;
    case 0x0163:
      d->codec = WMA9Lossless;
      break;
    default:
      d->codec = Unknown;
      break;
  }

}

void ASF::AudioProperties::setCodecName(const String &value) {
  d->codecName = value;
}

void ASF::AudioProperties::setCodecDescription(const String &value) {
  d->codecDescription = value;
}

void ASF::AudioProperties::setEncrypted(bool value) {
  d->encrypted = value;
}
