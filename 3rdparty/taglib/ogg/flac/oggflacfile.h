/***************************************************************************
    copyright            : (C) 2004 by Allan Sandfeld Jensen
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

#ifndef TAGLIB_OGGFLACFILE_H
#define TAGLIB_OGGFLACFILE_H

#include "taglib_export.h"
#include "oggfile.h"
#include "xiphcomment.h"

#include "flacproperties.h"

namespace Strawberry_TagLib {
namespace TagLib {

class Tag;

namespace Ogg {

//! An implementation of Ogg FLAC metadata

/*!
   * This is implementation of FLAC metadata for Ogg FLAC files.  For "pure"
   * FLAC files look under the FLAC hierarchy.
   *
   * Unlike "pure" FLAC-files, Ogg FLAC only supports Xiph-comments,
   * while the audio-properties are the same.
   */
namespace FLAC {

using Strawberry_TagLib::TagLib::FLAC::AudioProperties;

//! An implementation of TagLib::File with Ogg/FLAC specific methods

/*!
 * This implements and provides an interface for Ogg/FLAC files to the
 * TagLib::Tag and TagLib::AudioProperties interfaces by way of implementing
 * the abstract TagLib::File API as well as providing some additional
 * information specific to Ogg FLAC files.
 */

class TAGLIB_EXPORT File : public Ogg::File {
 public:
  /*!
   * Constructs an Ogg/FLAC file from \a file.
   * If \a readProperties is true the file's audio properties will also be read.
   *
   * \note In the current implementation, \a propertiesStyle is ignored.
   */
  explicit File(FileName file, bool readProperties = true, AudioProperties::ReadStyle propertiesStyle = AudioProperties::Average);

  /*!
   * Constructs an Ogg/FLAC file from \a stream.
   * If \a readProperties is true the file's audio properties will also be read.
   *
   * \note TagLib will *not* take ownership of the stream, the caller is responsible for deleting it after the File object.
   *
   * \note In the current implementation, \a propertiesStyle is ignored.
   */
  explicit File(IOStream *stream, bool readProperties = true, AudioProperties::ReadStyle propertiesStyle = AudioProperties::Average);

  /*!
   * Destroys this instance of the File.
   */
  ~File() override;

  /*!
   * Returns the Tag for this file.  This will always be a XiphComment.
   *
   * \note This always returns a valid pointer regardless of whether or not the file on disk has a XiphComment.
   * Use hasXiphComment() to check if the file on disk actually has a XiphComment.
   *
   * \note The Tag <b>is still</b> owned by the FLAC::File and should not be deleted by the user.
   * It will be deleted when the file (object) is destroyed.
   *
   * \see hasXiphComment()
   */
  XiphComment *tag() const override;

  /*!
   * Returns the FLAC::AudioProperties for this file.
   * If no audio properties were read then this will return a null pointer.
   */
  AudioProperties *audioProperties() const override;

  /*!
   * Save the file.  This will primarily save and update the XiphComment.
   * Returns true if the save is successful.
   */
  bool save() override;

  /*!
   * Returns the length of the audio-stream, used by FLAC::AudioProperties for calculating the bitrate.
   */
  long long streamLength();

  /*!
   * Returns whether or not the file on disk actually has a XiphComment.
   *
   * \see tag()
   */
  bool hasXiphComment() const;

  /*!
   * Check if the given \a stream can be opened as an Ogg FLAC file.
   *
   * \note This method is designed to do a quick check.  The result may not necessarily be correct.
   */
  static bool isSupported(IOStream *stream);

 private:
  File(const File&);
  File &operator=(const File&);

  void read(bool readProperties, AudioProperties::ReadStyle propertiesStyle);
  void scan();
  ByteVector streamInfoData();
  ByteVector xiphCommentData();

  class FilePrivate;
  FilePrivate *d;
};

}  // namespace FLAC
}  // namespace Ogg
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
