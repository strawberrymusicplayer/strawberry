/***************************************************************************
    copyright            : (C) 2003 by Allan Sandfeld Jensen
    email                : kde@carewolf.org
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

#ifndef TAGLIB_FLACPROPERTIES_H
#define TAGLIB_FLACPROPERTIES_H

#include "taglib_export.h"
#include "audioproperties.h"

namespace Strawberry_TagLib {
namespace TagLib {

namespace FLAC {

class File;

//! An implementation of audio property reading for FLAC

/*!
 * This reads the data from an FLAC stream found in the AudioProperties API.
 */

class TAGLIB_EXPORT AudioProperties : public Strawberry_TagLib::TagLib::AudioProperties {
 public:
  /*!
   * Create an instance of FLAC::AudioProperties with the data read from the ByteVector \a data.
   */
  explicit AudioProperties(const ByteVector &data, long long streamLength, ReadStyle style = Average);

  /*!
   * Destroys this FLAC::AudioProperties instance.
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
   * Returns the number of bits per audio sample as read from the FLAC identification header.
   */
  int bitsPerSample() const;

  /*!
   * Return the number of sample frames.
   */
  unsigned long long sampleFrames() const;

  /*!
   * Returns the MD5 signature of the uncompressed audio stream as read from the stream info header.
   */
  ByteVector signature() const;

 private:
  void read(const ByteVector &data, long long streamLength);

  class AudioPropertiesPrivate;
  AudioPropertiesPrivate *d;
};
}  // namespace FLAC
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
