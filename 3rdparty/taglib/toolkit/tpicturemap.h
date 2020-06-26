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

#ifndef TAGLIB_PICTUREMAP_H
#define TAGLIB_PICTUREMAP_H

#include "tlist.h"
#include "tmap.h"
#include "taglib_export.h"
#include "tpicture.h"

namespace Strawberry_TagLib {
namespace TagLib {

//! A list of pictures
typedef List<Picture> PictureList;

/// TODO: review this interface before the release of TagLib v2.x in light of
/// https://github.com/taglib/taglib/issues/734#issuecomment-214001325

/*!
  * This is a spcialization of the List class with some members.
  */
class TAGLIB_EXPORT PictureMap : public Map<Picture::Type, PictureList> {
 public:
  /*!
   * Constructs an empty PictureList.
   */
  explicit PictureMap();

  /*!
   * Constructs a PictureMap with \a Picture.
   */
  explicit PictureMap(const Picture &p);

  /*!
   * Constructs a PictureMap with \a PictureList as a member.
   */
  explicit PictureMap(const PictureList &l);

  /*!
   * Destroys this PictureList instance.
   */
  ~PictureMap() override;

  /*!
   * Inserts a PictureList into the picture map
   */
  void insert(const PictureList &l);

  /*!
   * Inserts a Picture into the picture map
   */
  void insert(const Picture &p);
};

}  // namespace TagLib
}  // namespace Strawberry_TagLib

/*!
 * \relates TagLib::PictureMap
 *
 * Send the PictureMap to on output stream
 */
TAGLIB_EXPORT std::ostream &operator<<(std::ostream &s, const Strawberry_TagLib::TagLib::PictureMap &map);

#endif  // TAGLIB_PICTUREMAP_H
