/***************************************************************************
    copyright           :(C) 2011 by Mathias Panzenböck
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


#include "itproperties.h"

using namespace Strawberry_TagLib::TagLib;
using namespace IT;

class IT::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : channels(0),
                                      lengthInPatterns(0),
                                      instrumentCount(0),
                                      sampleCount(0),
                                      patternCount(0),
                                      version(0),
                                      compatibleVersion(0),
                                      flags(0),
                                      special(0),
                                      globalVolume(0),
                                      mixVolume(0),
                                      tempo(0),
                                      bpmSpeed(0),
                                      panningSeparation(0),
                                      pitchWheelDepth(0) {
  }

  int channels;
  unsigned short lengthInPatterns;
  unsigned short instrumentCount;
  unsigned short sampleCount;
  unsigned short patternCount;
  unsigned short version;
  unsigned short compatibleVersion;
  unsigned short flags;
  unsigned short special;
  unsigned char globalVolume;
  unsigned char mixVolume;
  unsigned char tempo;
  unsigned char bpmSpeed;
  unsigned char panningSeparation;
  unsigned char pitchWheelDepth;
};

IT::AudioProperties::AudioProperties(AudioProperties::ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {}

IT::AudioProperties::~AudioProperties() {
  delete d;
}

int IT::AudioProperties::lengthInSeconds() const {
  return 0;
}

int IT::AudioProperties::lengthInMilliseconds() const {
  return 0;
}

int IT::AudioProperties::bitrate() const {
  return 0;
}

int IT::AudioProperties::sampleRate() const {
  return 0;
}

int IT::AudioProperties::channels() const {
  return d->channels;
}

unsigned short IT::AudioProperties::lengthInPatterns() const {
  return d->lengthInPatterns;
}

bool IT::AudioProperties::stereo() const {
  return d->flags & Stereo;
}

unsigned short IT::AudioProperties::instrumentCount() const {
  return d->instrumentCount;
}

unsigned short IT::AudioProperties::sampleCount() const {
  return d->sampleCount;
}

unsigned short IT::AudioProperties::patternCount() const {
  return d->patternCount;
}

unsigned short IT::AudioProperties::version() const {
  return d->version;
}

unsigned short IT::AudioProperties::compatibleVersion() const {
  return d->compatibleVersion;
}

unsigned short IT::AudioProperties::flags() const {
  return d->flags;
}

unsigned short IT::AudioProperties::special() const {
  return d->special;
}

unsigned char IT::AudioProperties::globalVolume() const {
  return d->globalVolume;
}

unsigned char IT::AudioProperties::mixVolume() const {
  return d->mixVolume;
}

unsigned char IT::AudioProperties::tempo() const {
  return d->tempo;
}

unsigned char IT::AudioProperties::bpmSpeed() const {
  return d->bpmSpeed;
}

unsigned char IT::AudioProperties::panningSeparation() const {
  return d->panningSeparation;
}

unsigned char IT::AudioProperties::pitchWheelDepth() const {
  return d->pitchWheelDepth;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void IT::AudioProperties::setChannels(int channels) {
  d->channels = channels;
}

void IT::AudioProperties::setLengthInPatterns(unsigned short lengthInPatterns) {
  d->lengthInPatterns = lengthInPatterns;
}

void IT::AudioProperties::setInstrumentCount(unsigned short instrumentCount) {
  d->instrumentCount = instrumentCount;
}

void IT::AudioProperties::setSampleCount(unsigned short sampleCount) {
  d->sampleCount = sampleCount;
}

void IT::AudioProperties::setPatternCount(unsigned short patternCount) {
  d->patternCount = patternCount;
}

void IT::AudioProperties::setFlags(unsigned short flags) {
  d->flags = flags;
}

void IT::AudioProperties::setSpecial(unsigned short special) {
  d->special = special;
}

void IT::AudioProperties::setCompatibleVersion(unsigned short compatibleVersion) {
  d->compatibleVersion = compatibleVersion;
}

void IT::AudioProperties::setVersion(unsigned short version) {
  d->version = version;
}

void IT::AudioProperties::setGlobalVolume(unsigned char globalVolume) {
  d->globalVolume = globalVolume;
}

void IT::AudioProperties::setMixVolume(unsigned char mixVolume) {
  d->mixVolume = mixVolume;
}

void IT::AudioProperties::setTempo(unsigned char tempo) {
  d->tempo = tempo;
}

void IT::AudioProperties::setBpmSpeed(unsigned char bpmSpeed) {
  d->bpmSpeed = bpmSpeed;
}

void IT::AudioProperties::setPanningSeparation(unsigned char panningSeparation) {
  d->panningSeparation = panningSeparation;
}

void IT::AudioProperties::setPitchWheelDepth(unsigned char pitchWheelDepth) {
  d->pitchWheelDepth = pitchWheelDepth;
}
