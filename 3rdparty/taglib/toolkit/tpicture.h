/***************************************************************************
    copyright            : (C) 2015 by Maxime Leblanc
    email                : lblnc.maxime@gmail.com
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

#ifndef TAGLIB_PICTURE_H
#define TAGLIB_PICTURE_H

#include "taglib_export.h"
#include "tbytevector.h"
#include "tmap.h"
#include "tstring.h"

namespace Strawberry_TagLib {
namespace TagLib {

class TAGLIB_EXPORT Picture {

  class PicturePrivate;

 public:
  /*!
   * The Type enum is based on types in id3v2 tags
   */
  enum Type {
    //! A type not enumerated below
    Other = 0x00,
    //! 32x32 PNG image that should be used as the file icon
    FileIcon = 0x01,
    //! File icon of a different size or format
    OtherFileIcon = 0x02,
    //! Front cover image of the album
    FrontCover = 0x03,
    //! Back cover image of the album
    BackCover = 0x04,
    //! Inside leaflet page of the album
    LeafletPage = 0x05,
    //! Image from the album itself
    Media = 0x06,
    //! Picture of the lead artist or soloist
    LeadArtist = 0x07,
    //! Picture of the artist or performer
    Artist = 0x08,
    //! Picture of the conductor
    Conductor = 0x09,
    //! Picture of the band or orchestra
    Band = 0x0A,
    //! Picture of the composer
    Composer = 0x0B,
    //! Picture of the lyricist or text writer
    Lyricist = 0x0C,
    //! Picture of the recording location or studio
    RecordingLocation = 0x0D,
    //! Picture of the artists during recording
    DuringRecording = 0x0E,
    //! Picture of the artists during performance
    DuringPerformance = 0x0F,
    //! Picture from a movie or video related to the track
    MovieScreenCapture = 0x10,
    //! Picture of a large, coloured fish
    ColouredFish = 0x11,
    //! Illustration related to the track
    Illustration = 0x12,
    //! Logo of the band or performer
    BandLogo = 0x13,
    //! Logo of the publisher (record company)
    PublisherLogo = 0x14
  };

  /*!
   * Constructs an empty Picture.
   */
  explicit Picture();

  /*!
   * Constructs a Picture object base on an other Picture
   */
  Picture(const Picture &p);

  /*!
   * Constructs a Picture object based on the \a ByteVector given.
   *
   * \note type, mime and description are optional
   */
  explicit Picture(const ByteVector &data, Type type = Other, const String &mime = "image/", const String &description = String());

  /*!
   * Destroys this Picture instance.
   */
  virtual ~Picture();

  /*!
   * Returns the mime of the picture
   */
  String mime() const;

  /*!
   * Returns the description of the picture
   */
  String description() const;

  /*!
   * Returns the type of the picture
   */
  Type type() const;

  /*!
   * Returns data of the picture
   */
  ByteVector data() const;

  /*!
   * Performs a shallow, implicitly shared, copy of \a p, overwriting the
   * Picture's current data.
   */
  Picture &operator=(const Picture &p);

  /*!
   * Exchanges the content of the Picture by the content of \a p.
   */
  void swap(Picture &p);

 private:
  PicturePrivate *d;
};

}  // namespace TagLib
}  // namespace Strawberry_TagLib

/*!
 * \relates TagLib::Picture
 *
 * Send the picture to an output stream.
 */
TAGLIB_EXPORT std::ostream &operator<<(std::ostream &s, const Strawberry_TagLib::TagLib::Picture &picture);

#endif  // TAGLIB_PICTUREMAP_H
