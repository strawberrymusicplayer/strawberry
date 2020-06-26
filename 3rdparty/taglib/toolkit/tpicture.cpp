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

#include <memory>
#include <algorithm>

#include "tpicture.h"

using namespace Strawberry_TagLib::TagLib;

namespace {
struct PictureData {
  explicit PictureData() : type(Picture::Other) {}

  ByteVector data;
  Picture::Type type;
  String mime;
  String description;
};
}  // namespace

class Picture::PicturePrivate {
 public:
  explicit PicturePrivate() : data(new PictureData()) {}

  std::shared_ptr<PictureData> data;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Picture::Picture() : d(new PicturePrivate()) {}

Picture::Picture(const ByteVector &data, Type type, const String &mime, const String &description) : d(new PicturePrivate()) {

  d->data->data = data;
  d->data->type = type;
  d->data->mime = mime;
  d->data->description = description;

}

Picture::Picture(const Picture &p) : d(new PicturePrivate(*p.d)) {}

Picture::~Picture() {
  delete d;
}

String Picture::description() const {
  return d->data->description;
}

ByteVector Picture::data() const {
  return d->data->data;
}

String Picture::mime() const {
  return d->data->mime;
}

Picture::Type Picture::type() const {
  return d->data->type;
}

void Picture::swap(Picture &other) {
  using std::swap;

  swap(d, other.d);
}

/* =========== OPERATORS =========== */

Picture &Picture::operator=(const Picture &p) {
  Picture(p).swap(*this);
  return *this;
}

std::ostream &operator<<(std::ostream &s, const Picture &p) {

  String type;
  switch (p.type()) {
    case Picture::FileIcon:
      type = "FileIcon";
      break;
    case Picture::OtherFileIcon:
      type = "OtherFileIcon";
      break;
    case Picture::FrontCover:
      type = "FrontCover";
      break;
    case Picture::BackCover:
      type = "BackCover";
      break;
    case Picture::LeafletPage:
      type = "LeafletPage";
      break;
    case Picture::Media:
      type = "Media";
      break;
    case Picture::LeadArtist:
      type = "LeadArtist";
      break;
    case Picture::Artist:
      type = "Artist";
      break;
    case Picture::Conductor:
      type = "Conductor";
      break;
    case Picture::Band:
      type = "Band";
      break;
    case Picture::Composer:
      type = "Composer";
      break;
    case Picture::Lyricist:
      type = "Lyricist";
      break;
    case Picture::RecordingLocation:
      type = "RecordingLocation";
      break;
    case Picture::DuringRecording:
      type = "DuringRecording";
      break;
    case Picture::DuringPerformance:
      type = "DuringPerformance";
      break;
    case Picture::MovieScreenCapture:
      type = "MovieScreenCapture";
      break;
    case Picture::ColouredFish:
      type = "ColouredFish";
      break;
    case Picture::Illustration:
      type = "Illustration";
      break;
    case Picture::BandLogo:
      type = "BandLogo";
      break;
    case Picture::PublisherLogo:
      type = "PublisherLogo";
      break;
    default:
      type = "Other";
      break;
  }

  ByteVector displayableData = p.data().mid(0, 20).toHex();
  s << "\nPicture:\n"
    << "\ttype: " << type.to8Bit() << std::endl
    << "\tdesc: " << p.description().to8Bit() << std::endl
    << "\tmime: " << p.mime().to8Bit() << std::endl
    << "\tdata: " << std::hex << displayableData << "..." << std::endl;

  return s;

}
