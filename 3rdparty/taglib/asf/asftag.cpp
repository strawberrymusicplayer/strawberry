/**************************************************************************
    copyright            : (C) 2005-2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

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
#include "tpropertymap.h"
#include "asftag.h"

using namespace Strawberry_TagLib::TagLib;

class ASF::Tag::TagPrivate {
 public:
  String title;
  String artist;
  String copyright;
  String comment;
  String rating;
  AttributeListMap attributeListMap;
};

ASF::Tag::Tag() : d(new TagPrivate()) {}

ASF::Tag::~Tag() {
  delete d;
}

String ASF::Tag::title() const {
  return d->title;
}

String ASF::Tag::artist() const {
  return d->artist;
}

String ASF::Tag::album() const {

  if (d->attributeListMap.contains("WM/AlbumTitle"))
    return d->attributeListMap["WM/AlbumTitle"][0].toString();
  return String();

}

String ASF::Tag::copyright() const {
  return d->copyright;
}

String ASF::Tag::comment() const {
  return d->comment;
}

String ASF::Tag::rating() const {
  return d->rating;
}

unsigned int ASF::Tag::year() const {

  if (d->attributeListMap.contains("WM/Year"))
    return d->attributeListMap["WM/Year"][0].toString().toInt();
  return 0;

}

unsigned int ASF::Tag::track() const {

  if (d->attributeListMap.contains("WM/TrackNumber")) {
    const ASF::Attribute attr = d->attributeListMap["WM/TrackNumber"][0];
    if (attr.type() == ASF::Attribute::DWordType)
      return attr.toUInt();
    else
      return attr.toString().toInt();
  }
  if (d->attributeListMap.contains("WM/Track"))
    return d->attributeListMap["WM/Track"][0].toUInt();
  return 0;

}

String ASF::Tag::genre() const {

  if (d->attributeListMap.contains("WM/Genre"))
    return d->attributeListMap["WM/Genre"][0].toString();
  return String();

}

PictureMap ASF::Tag::pictures() const {

  PictureMap map;
  if (d->attributeListMap.contains("WM/Picture")) {
    AttributeList list = d->attributeListMap["WM/Picture"];
    for (AttributeList::ConstIterator it = list.begin(); it != list.end(); ++it) {
      ASF::Picture asfPicture = (*it).toPicture();
      TagLib::Picture::Type type;
      switch (asfPicture.type()) {
        case ASF::Picture::FileIcon:
          type = TagLib::Picture::FileIcon;
          break;
        case ASF::Picture::OtherFileIcon:
          type = TagLib::Picture::OtherFileIcon;
          break;
        case ASF::Picture::FrontCover:
          type = TagLib::Picture::FrontCover;
          break;
        case ASF::Picture::BackCover:
          type = TagLib::Picture::BackCover;
          break;
        case ASF::Picture::LeafletPage:
          type = TagLib::Picture::LeafletPage;
          break;
        case ASF::Picture::Media:
          type = TagLib::Picture::Media;
          break;
        case ASF::Picture::LeadArtist:
          type = TagLib::Picture::LeadArtist;
          break;
        case ASF::Picture::Artist:
          type = TagLib::Picture::Artist;
          break;
        case ASF::Picture::Conductor:
          type = TagLib::Picture::Conductor;
          break;
        case ASF::Picture::Band:
          type = TagLib::Picture::Band;
          break;
        case ASF::Picture::Composer:
          type = TagLib::Picture::Composer;
          break;
        case ASF::Picture::Lyricist:
          type = TagLib::Picture::Lyricist;
          break;
        case ASF::Picture::RecordingLocation:
          type = TagLib::Picture::RecordingLocation;
          break;
        case ASF::Picture::DuringRecording:
          type = TagLib::Picture::DuringRecording;
          break;
        case ASF::Picture::DuringPerformance:
          type = TagLib::Picture::DuringPerformance;
          break;
        case ASF::Picture::MovieScreenCapture:
          type = TagLib::Picture::MovieScreenCapture;
          break;
        case ASF::Picture::ColouredFish:
          type = TagLib::Picture::ColouredFish;
          break;
        case ASF::Picture::Illustration:
          type = TagLib::Picture::Illustration;
          break;
        case ASF::Picture::BandLogo:
          type = TagLib::Picture::BandLogo;
          break;
        case ASF::Picture::PublisherLogo:
          type = TagLib::Picture::PublisherLogo;
          break;
        default:
          type = TagLib::Picture::Other;
          break;
      }
      TagLib::Picture picture(asfPicture.picture(), type, asfPicture.mimeType(), asfPicture.description());
      map.insert(picture);
    }
  }
  return PictureMap(map);

}

void ASF::Tag::setTitle(const String &value) {
  d->title = value;
}

void ASF::Tag::setArtist(const String &value) {
  d->artist = value;
}

void ASF::Tag::setCopyright(const String &value) {
  d->copyright = value;
}

void ASF::Tag::setComment(const String &value) {
  d->comment = value;
}

void ASF::Tag::setRating(const String &value) {
  d->rating = value;
}

void ASF::Tag::setAlbum(const String &value) {
  setAttribute("WM/AlbumTitle", value);
}

void ASF::Tag::setGenre(const String &value) {
  setAttribute("WM/Genre", value);
}

void ASF::Tag::setYear(unsigned int value) {
  setAttribute("WM/Year", String::number(value));
}

void ASF::Tag::setTrack(unsigned int value) {
  setAttribute("WM/TrackNumber", String::number(value));
}

void ASF::Tag::setPictures(const PictureMap &l) {

  removeItem("WM/Picture");
  for (PictureMap::ConstIterator pictureMapIt = l.begin(); pictureMapIt != l.end(); ++pictureMapIt) {
    PictureList list = pictureMapIt->second;
    for (PictureList::ConstIterator pictureListIt = list.begin(); pictureListIt != list.end(); ++pictureListIt) {
      const TagLib::Picture picture = (*pictureListIt);
      ASF::Picture asfPicture;
      asfPicture.setPicture(picture.data());
      asfPicture.setMimeType(picture.mime());
      asfPicture.setDescription(picture.description());
      switch (picture.type()) {
        case TagLib::Picture::Other:
          asfPicture.setType(ASF::Picture::Other);
          break;
        case TagLib::Picture::FileIcon:
          asfPicture.setType(ASF::Picture::FileIcon);
          break;
        case TagLib::Picture::OtherFileIcon:
          asfPicture.setType(ASF::Picture::OtherFileIcon);
          break;
        case TagLib::Picture::FrontCover:
          asfPicture.setType(ASF::Picture::FrontCover);
          break;
        case TagLib::Picture::BackCover:
          asfPicture.setType(ASF::Picture::BackCover);
          break;
        case TagLib::Picture::LeafletPage:
          asfPicture.setType(ASF::Picture::LeafletPage);
          break;
        case TagLib::Picture::Media:
          asfPicture.setType(ASF::Picture::Media);
          break;
        case TagLib::Picture::LeadArtist:
          asfPicture.setType(ASF::Picture::LeadArtist);
          break;
        case TagLib::Picture::Artist:
          asfPicture.setType(ASF::Picture::Artist);
          break;
        case TagLib::Picture::Conductor:
          asfPicture.setType(ASF::Picture::Conductor);
          break;
        case TagLib::Picture::Band:
          asfPicture.setType(ASF::Picture::Band);
          break;
        case TagLib::Picture::Composer:
          asfPicture.setType(ASF::Picture::Composer);
          break;
        case TagLib::Picture::Lyricist:
          asfPicture.setType(ASF::Picture::Lyricist);
          break;
        case TagLib::Picture::RecordingLocation:
          asfPicture.setType(ASF::Picture::RecordingLocation);
          break;
        case TagLib::Picture::DuringRecording:
          asfPicture.setType(ASF::Picture::DuringRecording);
          break;
        case TagLib::Picture::DuringPerformance:
          asfPicture.setType(ASF::Picture::DuringPerformance);
          break;
        case TagLib::Picture::MovieScreenCapture:
          asfPicture.setType(ASF::Picture::MovieScreenCapture);
          break;
        case TagLib::Picture::ColouredFish:
          asfPicture.setType(ASF::Picture::ColouredFish);
          break;
        case TagLib::Picture::Illustration:
          asfPicture.setType(ASF::Picture::Illustration);
          break;
        case TagLib::Picture::BandLogo:
          asfPicture.setType(ASF::Picture::BandLogo);
          break;
        case TagLib::Picture::PublisherLogo:
          asfPicture.setType(ASF::Picture::PublisherLogo);
          break;
      }
      addAttribute("WM/Picture", Attribute(asfPicture));
    }
  }

}

const ASF::AttributeListMap &ASF::Tag::attributeListMap() const {
  return d->attributeListMap;
}

bool ASF::Tag::contains(const String &key) const {
  return d->attributeListMap.contains(key);
}

void ASF::Tag::removeItem(const String &key) {
  d->attributeListMap.erase(key);
}

ASF::AttributeList ASF::Tag::attribute(const String &name) const {
  return d->attributeListMap[name];
}

void ASF::Tag::setAttribute(const String &name, const Attribute &attribute) {

  AttributeList value;
  value.append(attribute);
  d->attributeListMap.insert(name, value);

}

void ASF::Tag::setAttribute(const String &name, const AttributeList &values) {
  d->attributeListMap.insert(name, values);
}

void ASF::Tag::addAttribute(const String &name, const Attribute &attribute) {

  if (d->attributeListMap.contains(name)) {
    d->attributeListMap[name].append(attribute);
  }
  else {
    setAttribute(name, attribute);
  }

}

bool ASF::Tag::isEmpty() const {

  return Strawberry_TagLib::TagLib::Tag::isEmpty() &&
    copyright().isEmpty() &&
    rating().isEmpty() &&
    d->attributeListMap.isEmpty();

}

namespace {
const char *keyTranslation[][2] = {
  { "WM/AlbumTitle", "ALBUM" },
  { "WM/AlbumArtist", "ALBUMARTIST" },
  { "WM/Composer", "COMPOSER" },
  { "WM/Writer", "WRITER" },
  { "WM/Conductor", "CONDUCTOR" },
  { "WM/ModifiedBy", "REMIXER" },
  { "WM/Year", "DATE" },
  { "WM/OriginalReleaseYear", "ORIGINALDATE" },
  { "WM/Producer", "PRODUCER" },
  { "WM/ContentGroupDescription", "GROUPING" },
  { "WM/SubTitle", "SUBTITLE" },
  { "WM/SetSubTitle", "DISCSUBTITLE" },
  { "WM/TrackNumber", "TRACKNUMBER" },
  { "WM/PartOfSet", "DISCNUMBER" },
  { "WM/Genre", "GENRE" },
  { "WM/BeatsPerMinute", "BPM" },
  { "WM/Mood", "MOOD" },
  { "WM/ISRC", "ISRC" },
  { "WM/Lyrics", "LYRICS" },
  { "WM/Media", "MEDIA" },
  { "WM/Publisher", "LABEL" },
  { "WM/CatalogNo", "CATALOGNUMBER" },
  { "WM/Barcode", "BARCODE" },
  { "WM/EncodedBy", "ENCODEDBY" },
  { "WM/AlbumSortOrder", "ALBUMSORT" },
  { "WM/AlbumArtistSortOrder", "ALBUMARTISTSORT" },
  { "WM/ArtistSortOrder", "ARTISTSORT" },
  { "WM/TitleSortOrder", "TITLESORT" },
  { "WM/Script", "SCRIPT" },
  { "WM/Language", "LANGUAGE" },
  { "MusicBrainz/Track Id", "MUSICBRAINZ_TRACKID" },
  { "MusicBrainz/Artist Id", "MUSICBRAINZ_ARTISTID" },
  { "MusicBrainz/Album Id", "MUSICBRAINZ_ALBUMID" },
  { "MusicBrainz/Album Artist Id", "MUSICBRAINZ_ALBUMARTISTID" },
  { "MusicBrainz/Release Group Id", "MUSICBRAINZ_RELEASEGROUPID" },
  { "MusicBrainz/Work Id", "MUSICBRAINZ_WORKID" },
  { "MusicIP/PUID", "MUSICIP_PUID" },
  { "Acoustid/Id", "ACOUSTID_ID" },
  { "Acoustid/Fingerprint", "ACOUSTID_FINGERPRINT" },
};
const size_t keyTranslationSize = sizeof(keyTranslation) / sizeof(keyTranslation[0]);

String translateKey(const String &key) {

  for (size_t i = 0; i < keyTranslationSize; ++i) {
    if (key == keyTranslation[i][0])
      return keyTranslation[i][1];
  }

  return String();

}
}  // namespace

PropertyMap ASF::Tag::properties() const {

  PropertyMap props;

  if (!d->title.isEmpty()) {
    props["TITLE"] = d->title;
  }
  if (!d->artist.isEmpty()) {
    props["ARTIST"] = d->artist;
  }
  if (!d->copyright.isEmpty()) {
    props["COPYRIGHT"] = d->copyright;
  }
  if (!d->comment.isEmpty()) {
    props["COMMENT"] = d->comment;
  }

  ASF::AttributeListMap::ConstIterator it = d->attributeListMap.begin();
  for (; it != d->attributeListMap.end(); ++it) {
    const String key = translateKey(it->first);
    if (!key.isEmpty()) {
      AttributeList::ConstIterator it2 = it->second.begin();
      for (; it2 != it->second.end(); ++it2) {
        if (key == "TRACKNUMBER") {
          if (it2->type() == ASF::Attribute::DWordType)
            props.insert(key, String::number(it2->toUInt()));
          else
            props.insert(key, it2->toString());
        }
        else {
          props.insert(key, it2->toString());
        }
      }
    }
    else {
      props.unsupportedData().append(it->first);
    }
  }
  return props;

}

void ASF::Tag::removeUnsupportedProperties(const StringList &props) {

  StringList::ConstIterator it = props.begin();
  for (; it != props.end(); ++it)
    d->attributeListMap.erase(*it);

}

PropertyMap ASF::Tag::setProperties(const PropertyMap &props) {

  static Map<String, String> reverseKeyMap;
  if (reverseKeyMap.isEmpty()) {
    int numKeys = sizeof(keyTranslation) / sizeof(keyTranslation[0]);
    for (int i = 0; i < numKeys; i++) {
      reverseKeyMap[keyTranslation[i][1]] = keyTranslation[i][0];
    }
  }

  PropertyMap origProps = properties();
  PropertyMap::ConstIterator it = origProps.begin();
  for (; it != origProps.end(); ++it) {
    if (!props.contains(it->first) || props[it->first].isEmpty()) {
      if (it->first == "TITLE") {
        d->title.clear();
      }
      else if (it->first == "ARTIST") {
        d->artist.clear();
      }
      else if (it->first == "COMMENT") {
        d->comment.clear();
      }
      else if (it->first == "COPYRIGHT") {
        d->copyright.clear();
      }
      else {
        d->attributeListMap.erase(reverseKeyMap[it->first]);
      }
    }
  }

  PropertyMap ignoredProps;
  it = props.begin();
  for (; it != props.end(); ++it) {
    if (reverseKeyMap.contains(it->first)) {
      String name = reverseKeyMap[it->first];
      removeItem(name);
      StringList::ConstIterator it2 = it->second.begin();
      for (; it2 != it->second.end(); ++it2) {
        addAttribute(name, *it2);
      }
    }
    else if (it->first == "TITLE") {
      d->title = it->second.toString();
    }
    else if (it->first == "ARTIST") {
      d->artist = it->second.toString();
    }
    else if (it->first == "COMMENT") {
      d->comment = it->second.toString();
    }
    else if (it->first == "COPYRIGHT") {
      d->copyright = it->second.toString();
    }
    else {
      ignoredProps.insert(it->first, it->second);
    }
  }

  return ignoredProps;

}
