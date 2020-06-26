/***************************************************************************
    copyright            : (C) 2006 by Lukáš Lalinský
    email                : lalinsky@gmail.com

    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
                           (original Vorbis implementation)
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

#ifndef TAGLIB_SPEEXFILE_H
#define TAGLIB_SPEEXFILE_H

#include "oggfile.h"
#include "xiphcomment.h"

#include "speexproperties.h"

namespace Strawberry_TagLib {
namespace TagLib {

namespace Ogg {

//! A namespace containing classes for Speex metadata

namespace Speex {

//! An implementation of Ogg::File with Speex specific methods

/*!
 * This is the central class in the Ogg Speex metadata processing collection of classes.
 * It's built upon Ogg::File which handles processing of the Ogg logical bitstream
 * and breaking it down into pages which are handled by the codec implementations,
 * in this case Speex specifically.
 *
 */

class TAGLIB_EXPORT File : public Ogg::File {
 public:
  /*!
   * Constructs a Speex file from \a file.
   * If \a readProperties is true the file's audio properties will also be read.
   *
   * \note In the current implementation, \a propertiesStyle is ignored.
   */
  explicit File(FileName file, bool readProperties = true, AudioProperties::ReadStyle propertiesStyle = AudioProperties::Average);

  /*!
   * Constructs a Speex file from \a stream.
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
   * Returns the XiphComment for this file.  XiphComment implements the tag interface, so this serves as the reimplementation of TagLib::File::tag().
   */
  Ogg::XiphComment *tag() const override;

  /*!
   * Returns the Speex::AudioProperties for this file.
   * If no audio properties were read then this will return a null pointer.
   */
  AudioProperties *audioProperties() const override;

  /*!
   * Save the file.
   *
   * This returns true if the save was successful.
   */
  bool save() override;

  /*!
   * Returns whether or not the given \a stream can be opened as a Speex file.
   *
   * \note This method is designed to do a quick check.  The result may not necessarily be correct.
   */
  static bool isSupported(IOStream *stream);

 private:
  File(const File&);
  File &operator=(const File&);

  void read(bool readProperties);

  class FilePrivate;
  FilePrivate *d;
};

}  // namespace Speex
}  // namespace Ogg
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
