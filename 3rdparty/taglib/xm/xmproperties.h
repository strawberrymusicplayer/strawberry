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

#ifndef TAGLIB_XMPROPERTIES_H
#define TAGLIB_XMPROPERTIES_H

#include "taglib.h"
#include "tstring.h"
#include "audioproperties.h"

namespace Strawberry_TagLib {
namespace TagLib {
namespace XM {

class File;

class TAGLIB_EXPORT AudioProperties : public Strawberry_TagLib::TagLib::AudioProperties {

 public:
  /*! Flag bits. */
  enum {
    LinearFreqTable = 1  // otherwise its the amiga freq. table
  };

  explicit AudioProperties(AudioProperties::ReadStyle);
  ~AudioProperties() override;

  int lengthInSeconds() const override;
  int lengthInMilliseconds() const override;
  int bitrate() const override;
  int sampleRate() const override;
  int channels() const override;

  unsigned short lengthInPatterns() const;
  unsigned short version() const;
  unsigned short restartPosition() const;
  unsigned short patternCount() const;
  unsigned short instrumentCount() const;
  unsigned int sampleCount() const;
  unsigned short flags() const;
  unsigned short tempo() const;
  unsigned short bpmSpeed() const;

 private:
  void setChannels(int channels);

  void setLengthInPatterns(unsigned short lengthInPatterns);
  void setVersion(unsigned short version);
  void setRestartPosition(unsigned short restartPosition);
  void setPatternCount(unsigned short patternCount);
  void setInstrumentCount(unsigned short instrumentCount);
  void setSampleCount(unsigned int sampleCount);
  void setFlags(unsigned short flags);
  void setTempo(unsigned short tempo);
  void setBpmSpeed(unsigned short bpmSpeed);

 private:
  friend class File;
  class AudioPropertiesPrivate;
  AudioPropertiesPrivate *d;
};

}  // namespace XM
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
