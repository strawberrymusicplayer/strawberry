/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
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


#include "xmproperties.h"

using namespace Strawberry_TagLib::TagLib;
using namespace XM;

class XM::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : lengthInPatterns(0),
                                      channels(0),
                                      version(0),
                                      restartPosition(0),
                                      patternCount(0),
                                      instrumentCount(0),
                                      sampleCount(0),
                                      flags(0),
                                      tempo(0),
                                      bpmSpeed(0) {
  }

  unsigned short lengthInPatterns;
  int channels;
  unsigned short version;
  unsigned short restartPosition;
  unsigned short patternCount;
  unsigned short instrumentCount;
  unsigned int sampleCount;
  unsigned short flags;
  unsigned short tempo;
  unsigned short bpmSpeed;
};

XM::AudioProperties::AudioProperties(AudioProperties::ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {}

XM::AudioProperties::~AudioProperties() {
  delete d;
}

int XM::AudioProperties::lengthInSeconds() const {
  return 0;
}

int XM::AudioProperties::lengthInMilliseconds() const {
  return 0;
}

int XM::AudioProperties::bitrate() const {
  return 0;
}

int XM::AudioProperties::sampleRate() const {
  return 0;
}

int XM::AudioProperties::channels() const {
  return d->channels;
}

unsigned short XM::AudioProperties::lengthInPatterns() const {
  return d->lengthInPatterns;
}

unsigned short XM::AudioProperties::version() const {
  return d->version;
}

unsigned short XM::AudioProperties::restartPosition() const {
  return d->restartPosition;
}

unsigned short XM::AudioProperties::patternCount() const {
  return d->patternCount;
}

unsigned short XM::AudioProperties::instrumentCount() const {
  return d->instrumentCount;
}

unsigned int XM::AudioProperties::sampleCount() const {
  return d->sampleCount;
}

unsigned short XM::AudioProperties::flags() const {
  return d->flags;
}

unsigned short XM::AudioProperties::tempo() const {
  return d->tempo;
}

unsigned short XM::AudioProperties::bpmSpeed() const {
  return d->bpmSpeed;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void XM::AudioProperties::setLengthInPatterns(unsigned short lengthInPatterns) {
  d->lengthInPatterns = lengthInPatterns;
}

void XM::AudioProperties::setChannels(int channels) {
  d->channels = channels;
}

void XM::AudioProperties::setVersion(unsigned short version) {
  d->version = version;
}

void XM::AudioProperties::setRestartPosition(unsigned short restartPosition) {
  d->restartPosition = restartPosition;
}

void XM::AudioProperties::setPatternCount(unsigned short patternCount) {
  d->patternCount = patternCount;
}

void XM::AudioProperties::setInstrumentCount(unsigned short instrumentCount) {
  d->instrumentCount = instrumentCount;
}

void XM::AudioProperties::setSampleCount(unsigned int sampleCount) {
  d->sampleCount = sampleCount;
}

void XM::AudioProperties::setFlags(unsigned short flags) {
  d->flags = flags;
}

void XM::AudioProperties::setTempo(unsigned short tempo) {
  d->tempo = tempo;
}

void XM::AudioProperties::setBpmSpeed(unsigned short bpmSpeed) {
  d->bpmSpeed = bpmSpeed;
}
