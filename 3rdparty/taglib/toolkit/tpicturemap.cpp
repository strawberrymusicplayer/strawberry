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

#include "tpicturemap.h"

using namespace Strawberry_TagLib::TagLib;

PictureMap::PictureMap() : Map<Picture::Type, PictureList>() {}

PictureMap::PictureMap(const PictureList &l) : Map<Picture::Type, PictureList>() {
  insert(l);
}

PictureMap::PictureMap(const Picture &p) : Map<Picture::Type, PictureList>() {
  insert(p);
}

void PictureMap::insert(const Picture &p) {

  PictureList list;
  if (contains(p.type())) {
    list = Map<Picture::Type, PictureList>::find(p.type())->second;
    list.append(p);
    Map<Picture::Type, PictureList>::insert(p.type(), list);
  }
  else {
    list.append(p);
    Map<Picture::Type, PictureList>::insert(p.type(), list);
  }

}

void PictureMap::insert(const PictureList &l) {

  for (PictureList::ConstIterator it = l.begin(); it != l.end(); ++it) {
    Picture picture = (*it);
    insert(picture);
  }

}

PictureMap::~PictureMap() {}

std::ostream &operator<<(std::ostream &s, const PictureMap &map) {

  for (PictureMap::ConstIterator it = map.begin(); it != map.end(); ++it) {
    PictureList list = it->second;
    for (PictureList::ConstIterator it2 = list.begin(); it2 != list.end(); ++it2)
      s << *it2;
  }
  return s;

}
