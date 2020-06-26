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


#include "s3mproperties.h"

using namespace Strawberry_TagLib::TagLib;
using namespace S3M;

class S3M::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : lengthInPatterns(0),
                                      channels(0),
                                      stereo(false),
                                      sampleCount(0),
                                      patternCount(0),
                                      flags(0),
                                      trackerVersion(0),
                                      fileFormatVersion(0),
                                      globalVolume(0),
                                      masterVolume(0),
                                      tempo(0),
                                      bpmSpeed(0) {
  }

  unsigned short lengthInPatterns;
  int channels;
  bool stereo;
  unsigned short sampleCount;
  unsigned short patternCount;
  unsigned short flags;
  unsigned short trackerVersion;
  unsigned short fileFormatVersion;
  unsigned char globalVolume;
  unsigned char masterVolume;
  unsigned char tempo;
  unsigned char bpmSpeed;
};

S3M::AudioProperties::AudioProperties(AudioProperties::ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {}

S3M::AudioProperties::~AudioProperties() {
  delete d;
}

int S3M::AudioProperties::lengthInSeconds() const {
  return 0;
}

int S3M::AudioProperties::lengthInMilliseconds() const {
  return 0;
}

int S3M::AudioProperties::bitrate() const {
  return 0;
}

int S3M::AudioProperties::sampleRate() const {
  return 0;
}

int S3M::AudioProperties::channels() const {
  return d->channels;
}

unsigned short S3M::AudioProperties::lengthInPatterns() const {
  return d->lengthInPatterns;
}

bool S3M::AudioProperties::stereo() const {
  return d->stereo;
}

unsigned short S3M::AudioProperties::sampleCount() const {
  return d->sampleCount;
}

unsigned short S3M::AudioProperties::patternCount() const {
  return d->patternCount;
}

unsigned short S3M::AudioProperties::flags() const {
  return d->flags;
}

unsigned short S3M::AudioProperties::trackerVersion() const {
  return d->trackerVersion;
}

unsigned short S3M::AudioProperties::fileFormatVersion() const {
  return d->fileFormatVersion;
}

unsigned char S3M::AudioProperties::globalVolume() const {
  return d->globalVolume;
}

unsigned char S3M::AudioProperties::masterVolume() const {
  return d->masterVolume;
}

unsigned char S3M::AudioProperties::tempo() const {
  return d->tempo;
}

unsigned char S3M::AudioProperties::bpmSpeed() const {
  return d->bpmSpeed;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void S3M::AudioProperties::setLengthInPatterns(unsigned short lengthInPatterns) {
  d->lengthInPatterns = lengthInPatterns;
}

void S3M::AudioProperties::setChannels(int channels) {
  d->channels = channels;
}

void S3M::AudioProperties::setStereo(bool stereo) {
  d->stereo = stereo;
}

void S3M::AudioProperties::setSampleCount(unsigned short sampleCount) {
  d->sampleCount = sampleCount;
}

void S3M::AudioProperties::setPatternCount(unsigned short patternCount) {
  d->patternCount = patternCount;
}

void S3M::AudioProperties::setFlags(unsigned short flags) {
  d->flags = flags;
}

void S3M::AudioProperties::setTrackerVersion(unsigned short trackerVersion) {
  d->trackerVersion = trackerVersion;
}

void S3M::AudioProperties::setFileFormatVersion(unsigned short fileFormatVersion) {
  d->fileFormatVersion = fileFormatVersion;
}

void S3M::AudioProperties::setGlobalVolume(unsigned char globalVolume) {
  d->globalVolume = globalVolume;
}

void S3M::AudioProperties::setMasterVolume(unsigned char masterVolume) {
  d->masterVolume = masterVolume;
}

void S3M::AudioProperties::setTempo(unsigned char tempo) {
  d->tempo = tempo;
}

void S3M::AudioProperties::setBpmSpeed(unsigned char bpmSpeed) {
  d->bpmSpeed = bpmSpeed;
}
