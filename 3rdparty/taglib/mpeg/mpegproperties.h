/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
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

#ifndef TAGLIB_MPEGPROPERTIES_H
#define TAGLIB_MPEGPROPERTIES_H

#include "taglib_export.h"
#include "audioproperties.h"

#include "mpegheader.h"

namespace Strawberry_TagLib {
namespace TagLib {
namespace MPEG {

class File;
class XingHeader;

//! An implementation of audio property reading for MP3

/*!
 * This reads the data from an MPEG Layer III stream found in the AudioProperties API.
 */

class TAGLIB_EXPORT AudioProperties : public Strawberry_TagLib::TagLib::AudioProperties {
 public:
  /*!
   * Create an instance of MPEG::AudioProperties with the data read from the MPEG::File \a file.
   */
  explicit AudioProperties(File *file, ReadStyle style = Average);

  /*!
   * Destroys this MPEG AudioProperties instance.
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
   * Returns a pointer to the Xing/VBRI header if one exists or null if no Xing/VBRI header was found.
   */
  const XingHeader *xingHeader() const;

  /*!
   * Returns the MPEG Version of the file.
   */
  Header::Version version() const;

  /*!
   * Returns the layer version.  This will be between the values 1-3.
   */
  int layer() const;

  /*!
   * Returns true if the MPEG protection bit is enabled.
   */
  bool protectionEnabled() const;

  /*!
   * Returns the channel mode for this frame.
   */
  Header::ChannelMode channelMode() const;

  /*!
   * Returns true if the copyrighted bit is set.
   */
  bool isCopyrighted() const;

  /*!
   * Returns true if the "original" bit is set.
   */
  bool isOriginal() const;

 private:
  void read(File *file);

  class AudioPropertiesPrivate;
  AudioPropertiesPrivate *d;
};

}  // namespace MPEG
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
