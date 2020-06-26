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

#ifndef TAGLIB_TRUEAUDIOPROPERTIES_H
#define TAGLIB_TRUEAUDIOPROPERTIES_H

#include "audioproperties.h"

namespace Strawberry_TagLib {
namespace TagLib {
namespace TrueAudio {

class File;

//! An implementation of audio property reading for TrueAudio

/*!
 * This reads the data from an TrueAudio stream found in the AudioProperties API.
 */

class TAGLIB_EXPORT AudioProperties : public Strawberry_TagLib::TagLib::AudioProperties {
 public:
  /*!
   * Create an instance of TrueAudio::AudioProperties with the data read from the ByteVector \a data.
   */
  explicit AudioProperties(const ByteVector &data, long long streamLength, ReadStyle style = Average);

  /*!
   * Destroys this TrueAudio::AudioProperties instance.
   */
  ~AudioProperties() override;

  /*!
   * Returns the length of the file in seconds.  The length is rounded down to the nearest whole second.
   *
   * \see lengthInMilliseconds()
   */
  int lengthInSeconds() const override;

  /*!
   * Returns the length of the file in milliseconds.
   *
   * \see lengthInSeconds()
   */
  int lengthInMilliseconds() const override;

  /*!
   * Returns the average bit rate of the file in kb/s.
   */
  int bitrate() const override;

  /*!
   * Returns the sample rate in Hz.
   */
  int sampleRate() const override;

  /*!
   * Returns the number of audio channels.
   */
  int channels() const override;

  /*!
   * Returns the number of bits per audio sample.
   */
  int bitsPerSample() const;

  /*!
   * Returns the total number of sample frames
   */
  unsigned int sampleFrames() const;

  /*!
   * Returns the major version number.
   */
  int ttaVersion() const;

 private:
  void read(const ByteVector &data, long long streamLength);

  class AudioPropertiesPrivate;
  AudioPropertiesPrivate *d;
};

}  // namespace TrueAudio
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
