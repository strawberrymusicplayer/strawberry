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

#ifndef TAGLIB_ID3V1TAG_H
#define TAGLIB_ID3V1TAG_H

#include "tag.h"
#include "tbytevector.h"
#include "tstringhandler.h"
#include "taglib_export.h"

namespace Strawberry_TagLib {
namespace TagLib {

class File;

//! An ID3v1 implementation

namespace ID3v1 {

//! The main class in the ID3v1 implementation

/*!
 * This is an implementation of the ID3v1 format.
 * ID3v1 is both the simplest and most common of tag formats but is rather limited.
 * Because of its pervasiveness and the way that applications have been written around the
 * fields that it provides, the generic TagLib::Tag API is a mirror of what is provided by ID3v1.
 *
 * ID3v1 tags should generally only contain Latin1 information.
 * However because many applications do not follow this rule there is now support for overriding
 * the ID3v1 string handling using the ID3v1::StringHandler class.
 * Please see the documentation for that class for more information.
 *
 * \see StringHandler
 *
 * \note Most fields are truncated to a maximum of 28-30 bytes.
 * The truncation happens automatically when the tag is rendered.
 */

class TAGLIB_EXPORT Tag : public Strawberry_TagLib::TagLib::Tag {
 public:
  /*!
   * Create an ID3v1 tag with default values.
   */
  explicit Tag();

  /*!
   * Create an ID3v1 tag and parse the data in \a file starting at \a tagOffset.
   */
  explicit Tag(File *file, long long tagOffset);

  /*!
   * Destroys this Tag instance.
   */
  ~Tag() override;

  /*!
   * Renders the in memory values to a ByteVector suitable for writing to the file.
   */
  ByteVector render() const;

  /*!
   * Returns the string "TAG" suitable for usage in locating the tag in a file.
   */
  static ByteVector fileIdentifier();

  // Reimplementations.

  String title() const override;
  String artist() const override;
  String album() const override;
  String comment() const override;
  String genre() const override;
  unsigned int year() const override;
  unsigned int track() const override;
  PictureMap pictures() const override;

  void setTitle(const String &s) override;
  void setArtist(const String &s) override;
  void setAlbum(const String &s) override;
  void setComment(const String &s) override;
  void setGenre(const String &s) override;
  void setYear(unsigned int i) override;
  void setTrack(unsigned int i) override;
  void setPictures(const PictureMap&) override;

  /*!
   * Returns the genre in number.
   *
   * \note Normally 255 indicates that this tag contains no genre.
   */
  unsigned int genreNumber() const;

  /*!
   * Sets the genre in number to \a i.
   *
   * \note Valid value is from 0 up to 255. Normally 255 indicates that this tag contains no genre.
   */
  void setGenreNumber(unsigned int i);

  /*!
   * Sets the string handler that decides how the ID3v1 data will be converted to and from binary data.
   * If the parameter \a handler is null, the previous handler is released and default ISO-8859-1 handler is restored.
   *
   * \note The caller is responsible for deleting the previous handler as needed after it is released.
   *
   */
  static void setStringHandler(const Strawberry_TagLib::TagLib::StringHandler *handler);

 protected:
  /*!
   * Reads from the file specified in the constructor.
   */
  void read();
  /*!
   * Pareses the body of the tag in \a data.
   */
  void parse(const ByteVector &data);

 private:
  Tag(const Tag&);
  Tag &operator=(const Tag&);

  class TagPrivate;
  TagPrivate *d;
};

}  // namespace ID3v1
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
