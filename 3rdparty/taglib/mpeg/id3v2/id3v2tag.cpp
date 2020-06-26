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

#include <algorithm>

#include "tfile.h"
#include "tbytevector.h"
#include "tpropertymap.h"
#include "tpicturemap.h"
#include "tdebug.h"

#include "id3v2tag.h"
#include "id3v2header.h"
#include "id3v2extendedheader.h"
#include "id3v2footer.h"
#include "id3v2synchdata.h"
#include "id3v1genres.h"

#include "frames/attachedpictureframe.h"
#include "frames/textidentificationframe.h"
#include "frames/commentsframe.h"
#include "frames/urllinkframe.h"
#include "frames/uniquefileidentifierframe.h"
#include "frames/unsynchronizedlyricsframe.h"
#include "frames/unknownframe.h"

using namespace Strawberry_TagLib::TagLib;
using namespace ID3v2;

namespace {
class DefaultStringHandler : public Strawberry_TagLib::TagLib::StringHandler {
 public:
  explicit DefaultStringHandler() : Strawberry_TagLib::TagLib::StringHandler() {}

  String parse(const ByteVector &data) const override {
    return String(data, String::Latin1);
  }

  ByteVector render(const String&) const override {
    // Not implemented on purpose. This function is never used.
    return ByteVector();
  }
};

const DefaultStringHandler defaultStringHandler;
const Strawberry_TagLib::TagLib::StringHandler *stringHandler = &defaultStringHandler;

const long long MinPaddingSize = 1024;
const long long MaxPaddingSize = 1024 * 1024;
}  // namespace

class ID3v2::Tag::TagPrivate {
 public:
  explicit TagPrivate() : factory(nullptr),
                          file(nullptr),
                          tagOffset(0),
                          extendedHeader(nullptr),
                          footer(nullptr) {
    frameList.setAutoDelete(true);
  }

  ~TagPrivate() {
    delete extendedHeader;
    delete footer;
  }

  const FrameFactory *factory;

  File *file;
  long long tagOffset;

  Header header;
  ExtendedHeader *extendedHeader;
  Footer *footer;

  FrameListMap frameListMap;
  FrameList frameList;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

ID3v2::Tag::Tag() : d(new TagPrivate()) {
  d->factory = FrameFactory::instance();
}

ID3v2::Tag::Tag(File *file, long long tagOffset, const FrameFactory *factory) : d(new TagPrivate()) {
  d->factory = factory;
  d->file = file;
  d->tagOffset = tagOffset;

  read();
}

ID3v2::Tag::~Tag() {
  delete d;
}

String ID3v2::Tag::title() const {
  if (!d->frameListMap["TIT2"].isEmpty())
    return d->frameListMap["TIT2"].front()->toString();
  return String();
}

String ID3v2::Tag::artist() const {
  if (!d->frameListMap["TPE1"].isEmpty())
    return d->frameListMap["TPE1"].front()->toString();
  return String();
}

String ID3v2::Tag::album() const {
  if (!d->frameListMap["TALB"].isEmpty())
    return d->frameListMap["TALB"].front()->toString();
  return String();
}

String ID3v2::Tag::comment() const {

  const FrameList &comments = d->frameListMap["COMM"];

  if (comments.isEmpty())
    return String();

  for (FrameList::ConstIterator it = comments.begin(); it != comments.end(); ++it) {
    CommentsFrame *frame = dynamic_cast<CommentsFrame *>(*it);

    if (frame && frame->description().isEmpty())
      return (*it)->toString();
  }

  return comments.front()->toString();

}

String ID3v2::Tag::genre() const {

  // TODO: In the next major version (TagLib 2.0) a list of multiple genres
  // should be separated by " / " instead of " ".  For the moment to keep
  // the behavior the same as released versions it is being left with " ".

  if (d->frameListMap["TCON"].isEmpty() ||
    !dynamic_cast<TextIdentificationFrame *>(d->frameListMap["TCON"].front())) {
    return String();
  }

  // ID3v2.4 lists genres as the fields in its frames field list.  If the field
  // is simply a number it can be assumed that it is an ID3v1 genre number.
  // Here was assume that if an ID3v1 string is present that it should be
  // appended to the genre string.  Multiple fields will be appended as the
  // string is built.

  TextIdentificationFrame *f = static_cast<TextIdentificationFrame *>(
    d->frameListMap["TCON"].front());

  StringList fields = f->fieldList();

  StringList genres;

  for (StringList::Iterator it = fields.begin(); it != fields.end(); ++it) {

    if ((*it).isEmpty())
      continue;

    bool ok;
    int number = (*it).toInt(&ok);
    if (ok && number >= 0 && number <= 255) {
      *it = ID3v1::genre(number);
    }

    if (std::find(genres.begin(), genres.end(), *it) == genres.end())
      genres.append(*it);
  }

  return genres.toString();

}

unsigned int ID3v2::Tag::year() const {

  if (!d->frameListMap["TDRC"].isEmpty())
    return d->frameListMap["TDRC"].front()->toString().substr(0, 4).toInt();
  return 0;

}

unsigned int ID3v2::Tag::track() const {

  if (!d->frameListMap["TRCK"].isEmpty())
    return d->frameListMap["TRCK"].front()->toString().toInt();

  return 0;
  
}

Strawberry_TagLib::TagLib::PictureMap ID3v2::Tag::pictures() const {

  if (!d->frameListMap.contains("APIC"))
    return PictureMap();

  PictureMap map;
  FrameList frameListMap = d->frameListMap["APIC"];
  for (FrameList::ConstIterator it = frameListMap.begin(); it != frameListMap.end(); ++it) {
    const AttachedPictureFrame *frame = static_cast<AttachedPictureFrame *>(*it);
    Picture::Type type;
    switch (frame->type()) {
      case AttachedPictureFrame::FileIcon:
        type = Picture::FileIcon;
        break;
      case AttachedPictureFrame::OtherFileIcon:
        type = Picture::OtherFileIcon;
        break;
      case AttachedPictureFrame::FrontCover:
        type = Picture::FrontCover;
        break;
      case AttachedPictureFrame::BackCover:
        type = Picture::BackCover;
        break;
      case AttachedPictureFrame::LeafletPage:
        type = Picture::LeafletPage;
        break;
      case AttachedPictureFrame::Media:
        type = Picture::Media;
        break;
      case AttachedPictureFrame::LeadArtist:
        type = Picture::LeadArtist;
        break;
      case AttachedPictureFrame::Artist:
        type = Picture::Artist;
        break;
      case AttachedPictureFrame::Conductor:
        type = Picture::Conductor;
        break;
      case AttachedPictureFrame::Band:
        type = Picture::Band;
        break;
      case AttachedPictureFrame::Composer:
        type = Picture::Composer;
        break;
      case AttachedPictureFrame::Lyricist:
        type = Picture::Lyricist;
        break;
      case AttachedPictureFrame::RecordingLocation:
        type = Picture::RecordingLocation;
        break;
      case AttachedPictureFrame::DuringRecording:
        type = Picture::DuringRecording;
        break;
      case AttachedPictureFrame::DuringPerformance:
        type = Picture::DuringPerformance;
        break;
      case AttachedPictureFrame::MovieScreenCapture:
        type = Picture::MovieScreenCapture;
        break;
      case AttachedPictureFrame::ColouredFish:
        type = Picture::ColouredFish;
        break;
      case AttachedPictureFrame::Illustration:
        type = Picture::Illustration;
        break;
      case AttachedPictureFrame::BandLogo:
        type = Picture::BandLogo;
        break;
      case AttachedPictureFrame::PublisherLogo:
        type = Picture::PublisherLogo;
        break;
      default:
        type = Picture::Other;
        break;
    }
    Picture picture(frame->picture(), type, frame->mimeType(), frame->description());
    map.insert(picture);
  }
  return PictureMap(map);

}

void ID3v2::Tag::setTitle(const String &s) {
  setTextFrame("TIT2", s);
}

void ID3v2::Tag::setArtist(const String &s) {
  setTextFrame("TPE1", s);
}

void ID3v2::Tag::setAlbum(const String &s) {
  setTextFrame("TALB", s);
}

void ID3v2::Tag::setComment(const String &s) {

  if (s.isEmpty()) {
    removeFrames("COMM");
    return;
  }

  if (!d->frameListMap["COMM"].isEmpty())
    d->frameListMap["COMM"].front()->setText(s);
  else {
    CommentsFrame *f = new CommentsFrame(d->factory->defaultTextEncoding());
    addFrame(f);
    f->setText(s);
  }

}

void ID3v2::Tag::setGenre(const String &s) {

  if (s.isEmpty()) {
    removeFrames("TCON");
    return;
  }

  // iTunes can't handle correctly encoded ID3v2.4 numerical genres.  Just use
  // strings until iTunes sucks less.

#ifdef NO_ITUNES_HACKS

  int index = ID3v1::genreIndex(s);

  if (index != 255)
    setTextFrame("TCON", String::number(index));
  else
    setTextFrame("TCON", s);

#else

  setTextFrame("TCON", s);

#endif

}

void ID3v2::Tag::setYear(unsigned int i) {
  if (i == 0) {
    removeFrames("TDRC");
    return;
  }
  setTextFrame("TDRC", String::number(i));
}

void ID3v2::Tag::setTrack(unsigned int i) {
  if (i == 0) {
    removeFrames("TRCK");
    return;
  }
  setTextFrame("TRCK", String::number(i));
}

void ID3v2::Tag::setPictures(const PictureMap &l) {

  removeFrames("APIC");

  for (PictureMap::ConstIterator it = l.begin(); it != l.end(); ++it) {

    PictureList list = it->second;
    FrameList framesAdded;
    for (PictureList::ConstIterator it2 = list.begin(); it2 != list.end(); ++it2) {
      const Picture picture = (*it2);
      AttachedPictureFrame *frame = new AttachedPictureFrame();
      frame->setPicture(picture.data());
      frame->setMimeType(picture.mime());
      frame->setDescription(picture.description());
      switch (picture.type()) {
        case Picture::Other:
          frame->setType(AttachedPictureFrame::Other);
          break;
        case Picture::FileIcon:
          frame->setType(AttachedPictureFrame::FileIcon);
          break;
        case Picture::OtherFileIcon:
          frame->setType(AttachedPictureFrame::OtherFileIcon);
          break;
        case Picture::FrontCover:
          frame->setType(AttachedPictureFrame::FrontCover);
          break;
        case Picture::BackCover:
          frame->setType(AttachedPictureFrame::BackCover);
          break;
        case Picture::LeafletPage:
          frame->setType(AttachedPictureFrame::LeafletPage);
          break;
        case Picture::Media:
          frame->setType(AttachedPictureFrame::Media);
          break;
        case Picture::LeadArtist:
          frame->setType(AttachedPictureFrame::LeadArtist);
          break;
        case Picture::Artist:
          frame->setType(AttachedPictureFrame::Artist);
          break;
        case Picture::Conductor:
          frame->setType(AttachedPictureFrame::Conductor);
          break;
        case Picture::Band:
          frame->setType(AttachedPictureFrame::Band);
          break;
        case Picture::Composer:
          frame->setType(AttachedPictureFrame::Composer);
          break;
        case Picture::Lyricist:
          frame->setType(AttachedPictureFrame::Lyricist);
          break;
        case Picture::RecordingLocation:
          frame->setType(AttachedPictureFrame::RecordingLocation);
          break;
        case Picture::DuringRecording:
          frame->setType(AttachedPictureFrame::DuringRecording);
          break;
        case Picture::DuringPerformance:
          frame->setType(AttachedPictureFrame::DuringPerformance);
          break;
        case Picture::MovieScreenCapture:
          frame->setType(AttachedPictureFrame::MovieScreenCapture);
          break;
        case Picture::ColouredFish:
          frame->setType(AttachedPictureFrame::ColouredFish);
          break;
        case Picture::Illustration:
          frame->setType(AttachedPictureFrame::Illustration);
          break;
        case Picture::BandLogo:
          frame->setType(AttachedPictureFrame::BandLogo);
          break;
        case Picture::PublisherLogo:
          frame->setType(AttachedPictureFrame::PublisherLogo);
          break;
      }
      framesAdded.append(frame);
    }
    for (FrameList::ConstIterator it2 = framesAdded.begin(); it2 != framesAdded.end(); ++it2)
      addFrame(*it2);
  }
}

bool ID3v2::Tag::isEmpty() const {
  return d->frameList.isEmpty();
}

Header *ID3v2::Tag::header() const {
  return &(d->header);
}

ExtendedHeader *ID3v2::Tag::extendedHeader() const {
  return d->extendedHeader;
}

const FrameListMap &ID3v2::Tag::frameListMap() const {
  return d->frameListMap;
}

const FrameList &ID3v2::Tag::frameList() const {
  return d->frameList;
}

const FrameList &ID3v2::Tag::frameList(const ByteVector &frameID) const {
  return d->frameListMap[frameID];
}

void ID3v2::Tag::addFrame(Frame *frame) {
  d->frameList.append(frame);
  d->frameListMap[frame->frameID()].append(frame);
}

void ID3v2::Tag::removeFrame(Frame *frame, bool del) {

  // remove the frame from the frame list
  FrameList::Iterator it = d->frameList.find(frame);
  d->frameList.erase(it);

  // ...and from the frame list map
  it = d->frameListMap[frame->frameID()].find(frame);
  d->frameListMap[frame->frameID()].erase(it);

  // ...and delete as desired
  if (del)
    delete frame;

}

void ID3v2::Tag::removeFrames(const ByteVector &id) {
  FrameList l = d->frameListMap[id];
  for (FrameList::ConstIterator it = l.begin(); it != l.end(); ++it)
    removeFrame(*it, true);
}

PropertyMap ID3v2::Tag::properties() const {
  PropertyMap properties;
  for (FrameList::ConstIterator it = frameList().begin(); it != frameList().end(); ++it) {
    PropertyMap props = (*it)->asProperties();
    properties.merge(props);
  }
  return properties;
}

void ID3v2::Tag::removeUnsupportedProperties(const StringList &properties) {

  for (StringList::ConstIterator it = properties.begin(); it != properties.end(); ++it) {
    if (it->startsWith("UNKNOWN/")) {
      String frameID = it->substr(String("UNKNOWN/").size());
      if (frameID.size() != 4)
        continue;  // invalid specification
      ByteVector id = frameID.data(String::Latin1);
      // delete all unknown frames of given type
      FrameList l = frameList(id);
      for (FrameList::ConstIterator fit = l.begin(); fit != l.end(); fit++)
        if (dynamic_cast<const UnknownFrame *>(*fit) != nullptr)
          removeFrame(*fit);
    }
    else if (it->size() == 4) {
      ByteVector id = it->data(String::Latin1);
      removeFrames(id);
    }
    else {
      ByteVector id = it->substr(0, 4).data(String::Latin1);
      if (it->size() <= 5)
        continue;  // invalid specification
      String description = it->substr(5);
      Frame *frame = nullptr;
      if (id == "TXXX")
        frame = UserTextIdentificationFrame::find(this, description);
      else if (id == "WXXX")
        frame = UserUrlLinkFrame::find(this, description);
      else if (id == "COMM")
        frame = CommentsFrame::findByDescription(this, description);
      else if (id == "USLT")
        frame = UnsynchronizedLyricsFrame::findByDescription(this, description);
      else if (id == "UFID")
        frame = UniqueFileIdentifierFrame::findByOwner(this, description);
      if (frame)
        removeFrame(frame);
    }
  }

}

PropertyMap ID3v2::Tag::setProperties(const PropertyMap &origProps) {

  FrameList framesToDelete;
  // we split up the PropertyMap into the "normal" keys and the "complicated" ones,
  // which are those according to TIPL or TMCL frames.
  PropertyMap properties;
  PropertyMap tiplProperties;
  PropertyMap tmclProperties;
  Frame::splitProperties(origProps, properties, tiplProperties, tmclProperties);
  for (FrameListMap::ConstIterator it = frameListMap().begin(); it != frameListMap().end(); ++it) {
    for (FrameList::ConstIterator lit = it->second.begin(); lit != it->second.end(); ++lit) {
      PropertyMap frameProperties = (*lit)->asProperties();
      if (it->first == "TIPL") {
        if (tiplProperties != frameProperties)
          framesToDelete.append(*lit);
        else
          tiplProperties.erase(frameProperties);
      }
      else if (it->first == "TMCL") {
        if (tmclProperties != frameProperties)
          framesToDelete.append(*lit);
        else
          tmclProperties.erase(frameProperties);
      }
      else if (!properties.contains(frameProperties))
        framesToDelete.append(*lit);
      else
        properties.erase(frameProperties);
    }
  }
  for (FrameList::ConstIterator it = framesToDelete.begin(); it != framesToDelete.end(); ++it)
    removeFrame(*it);

  // now create remaining frames:
  // start with the involved people list (TIPL)
  if (!tiplProperties.isEmpty())
    addFrame(TextIdentificationFrame::createTIPLFrame(tiplProperties));
  // proceed with the musician credit list (TMCL)
  if (!tmclProperties.isEmpty())
    addFrame(TextIdentificationFrame::createTMCLFrame(tmclProperties));
  // now create the "one key per frame" frames
  for (PropertyMap::ConstIterator it = properties.begin(); it != properties.end(); ++it)
    addFrame(Frame::createTextualFrame(it->first, it->second));
  return PropertyMap();  // ID3 implements the complete PropertyMap interface, so an empty map is returned

}

void ID3v2::Tag::downgradeFrames(FrameList *frames, FrameList *newFrames) const {
#ifdef NO_ITUNES_HACKS
  const char *unsupportedFrames[] = { "ASPI", "EQU2", "RVA2", "SEEK", "SIGN", "TDRL", "TDTG", "TMOO", "TPRO", "TSOA", "TSOT", "TSST", "TSOP", nullptr };
#else
  // iTunes writes and reads TSOA, TSOT, TSOP to ID3v2.3.
  const char *unsupportedFrames[] = { "ASPI", "EQU2", "RVA2", "SEEK", "SIGN", "TDRL", "TDTG", "TMOO", "TPRO", "TSST", nullptr };
#endif
  ID3v2::TextIdentificationFrame *frameTDOR = nullptr;
  ID3v2::TextIdentificationFrame *frameTDRC = nullptr;
  ID3v2::TextIdentificationFrame *frameTIPL = nullptr;
  ID3v2::TextIdentificationFrame *frameTMCL = nullptr;
  for (FrameList::ConstIterator it = d->frameList.begin(); it != d->frameList.end(); it++) {
    ID3v2::Frame *frame = *it;
    ByteVector frameID = frame->header()->frameID();
    for (int i = 0; unsupportedFrames[i]; i++) {
      if (frameID == unsupportedFrames[i]) {
        debug("A frame that is not supported in ID3v2.3 \'" + String(frameID) + "\' has been discarded");
        frame = nullptr;
        break;
      }
    }
    if (frame && frameID == "TDOR") {
      frameTDOR = dynamic_cast<ID3v2::TextIdentificationFrame *>(frame);
      frame = nullptr;
    }
    if (frame && frameID == "TDRC") {
      frameTDRC = dynamic_cast<ID3v2::TextIdentificationFrame *>(frame);
      frame = nullptr;
    }
    if (frame && frameID == "TIPL") {
      frameTIPL = dynamic_cast<ID3v2::TextIdentificationFrame *>(frame);
      frame = nullptr;
    }
    if (frame && frameID == "TMCL") {
      frameTMCL = dynamic_cast<ID3v2::TextIdentificationFrame *>(frame);
      frame = nullptr;
    }
    if (frame) {
      frames->append(frame);
    }
  }
  if (frameTDOR) {
    String content = frameTDOR->toString();
    if (content.size() >= 4) {
      ID3v2::TextIdentificationFrame *frameTORY = new ID3v2::TextIdentificationFrame("TORY", String::Latin1);
      frameTORY->setText(content.substr(0, 4));
      frames->append(frameTORY);
      newFrames->append(frameTORY);
    }
  }
  if (frameTDRC) {
    String content = frameTDRC->toString();
    if (content.size() >= 4) {
      ID3v2::TextIdentificationFrame *frameTYER = new ID3v2::TextIdentificationFrame("TYER", String::Latin1);
      frameTYER->setText(content.substr(0, 4));
      frames->append(frameTYER);
      newFrames->append(frameTYER);
      if (content.size() >= 10 && content[4] == '-' && content[7] == '-') {
        ID3v2::TextIdentificationFrame *frameTDAT = new ID3v2::TextIdentificationFrame("TDAT", String::Latin1);
        frameTDAT->setText(content.substr(8, 2) + content.substr(5, 2));
        frames->append(frameTDAT);
        newFrames->append(frameTDAT);
        if (content.size() >= 16 && content[10] == 'T' && content[13] == ':') {
          ID3v2::TextIdentificationFrame *frameTIME = new ID3v2::TextIdentificationFrame("TIME", String::Latin1);
          frameTIME->setText(content.substr(11, 2) + content.substr(14, 2));
          frames->append(frameTIME);
          newFrames->append(frameTIME);
        }
      }
    }
  }
  if (frameTIPL || frameTMCL) {
    ID3v2::TextIdentificationFrame *frameIPLS = new ID3v2::TextIdentificationFrame("IPLS", String::Latin1);
    StringList people;
    if (frameTMCL) {
      StringList v24People = frameTMCL->fieldList();
      for (unsigned int i = 0; i + 1 < v24People.size(); i += 2) {
        people.append(v24People[i]);
        people.append(v24People[i + 1]);
      }
    }
    if (frameTIPL) {
      StringList v24People = frameTIPL->fieldList();
      for (unsigned int i = 0; i + 1 < v24People.size(); i += 2) {
        people.append(v24People[i]);
        people.append(v24People[i + 1]);
      }
    }
    frameIPLS->setText(people);
    frames->append(frameIPLS);
    newFrames->append(frameIPLS);
  }

}

ByteVector ID3v2::Tag::render(Version version) const {

  // We need to render the "tag data" first so that we have to correct size to
  // render in the tag's header.  The "tag data" -- everything that is included
  // in ID3v2::Header::tagSize() -- includes the extended header, frames and
  // padding, but does not include the tag's header or footer.

  // TODO: Render the extended header.

  // Downgrade the frames that ID3v2.3 doesn't support.

  FrameList newFrames;
  newFrames.setAutoDelete(true);

  FrameList frameList;
  if (version == v4) {
    frameList = d->frameList;
  }
  else {
    downgradeFrames(&frameList, &newFrames);
  }

  // Reserve a 10-byte blank space for an ID3v2 tag header.

  ByteVector tagData(Header::size(), '\0');

  // Loop through the frames rendering them and adding them to the tagData.

  for (FrameList::ConstIterator it = frameList.begin(); it != frameList.end(); it++) {
    (*it)->header()->setVersion(version == v3 ? 3 : 4);
    if ((*it)->header()->frameID().size() != 4) {
      debug("An ID3v2 frame of unsupported or unknown type \'" + String((*it)->header()->frameID()) + "\' has been discarded");
      continue;
    }
    if (!(*it)->header()->tagAlterPreservation()) {
      const ByteVector frameData = (*it)->render();
      if (frameData.size() == Frame::headerSize((*it)->header()->version())) {
        debug("An empty ID3v2 frame \'" + String((*it)->header()->frameID()) + "\' has been discarded");
        continue;
      }
      tagData.append(frameData);
    }
  }

  // Compute the amount of padding, and append that to tagData.

  long long originalSize = d->header.tagSize();
  long long paddingSize = originalSize - (tagData.size() - Header::size());

  if (paddingSize <= 0) {
    paddingSize = MinPaddingSize;
  }
  else {
    // Padding won't increase beyond 1% of the file size or 1MB.

    long long threshold = d->file ? d->file->length() / 100 : 0;
    threshold = std::max(threshold, MinPaddingSize);
    threshold = std::min(threshold, MaxPaddingSize);

    if (paddingSize > threshold)
      paddingSize = MinPaddingSize;
  }

  tagData.resize(static_cast<unsigned int>(tagData.size() + paddingSize), '\0');

  // Set the version and data size.
  d->header.setMajorVersion(version);
  d->header.setTagSize(tagData.size() - Header::size());

  // TODO: This should eventually include d->footer->render().
  const ByteVector headerData = d->header.render();
  std::copy(headerData.begin(), headerData.end(), tagData.begin());

  return tagData;

}

Strawberry_TagLib::TagLib::StringHandler const *ID3v2::Tag::latin1StringHandler() {
  return stringHandler;
}

void ID3v2::Tag::setLatin1StringHandler(const Strawberry_TagLib::TagLib::StringHandler *handler) {
  if (handler)
    stringHandler = handler;
  else
    stringHandler = &defaultStringHandler;
}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

void ID3v2::Tag::read() {

  if (!d->file)
    return;

  if (!d->file->isOpen())
    return;

  d->file->seek(d->tagOffset);
  d->header.setData(d->file->readBlock(Header::size()));

  // If the tag size is 0, then this is an invalid tag (tags must contain at
  // least one frame)

  if (d->header.tagSize() != 0)
    parse(d->file->readBlock(d->header.tagSize()));

  // Look for duplicate ID3v2 tags and treat them as an extra blank of this one.
  // It leads to overwriting them with zero when saving the tag.

  // This is a workaround for some faulty files that have duplicate ID3v2 tags.
  // Unfortunately, TagLib itself may write such duplicate tags until v1.10.

  unsigned int extraSize = 0;

  while (true) {

    d->file->seek(d->tagOffset + d->header.completeTagSize() + extraSize);

    const ByteVector data = d->file->readBlock(Header::size());
    if (data.size() < Header::size() || !data.startsWith(Header::fileIdentifier()))
      break;

    extraSize += Header(data).completeTagSize();
  }

  if (extraSize != 0) {
    debug("ID3v2::Tag::read() - Duplicate ID3v2 tags found.");
    d->header.setTagSize(d->header.tagSize() + extraSize);
  }

}

void ID3v2::Tag::parse(const ByteVector &origData) {

  ByteVector data = origData;

  if (d->header.unsynchronisation() && d->header.majorVersion() <= 3)
    data = SynchData::decode(data);

  size_t frameDataPosition = 0;
  size_t frameDataLength = data.size();

  // check for extended header

  if (d->header.extendedHeader()) {
    if (!d->extendedHeader)
      d->extendedHeader = new ExtendedHeader();
    d->extendedHeader->setData(data);
    if (d->extendedHeader->size() <= data.size()) {
      frameDataPosition += d->extendedHeader->size();
      frameDataLength -= d->extendedHeader->size();
    }
  }

  // check for footer -- we don't actually need to parse it, as it *must*
  // contain the same data as the header, but we do need to account for its
  // size.

  if (d->header.footerPresent() && Footer::size() <= frameDataLength)
    frameDataLength -= Footer::size();

  // parse frames

  // Make sure that there is at least enough room in the remaining frame data for
  // a frame header.

  while (frameDataPosition < frameDataLength - Frame::headerSize(d->header.majorVersion())) {

    // If the next data is position is 0, assume that we've hit the padding
    // portion of the frame data.

    if (data.at(frameDataPosition) == 0) {
      if (d->header.footerPresent()) {
        debug("Padding *and* a footer found.  This is not allowed by the spec.");
      }

      break;
    }

    Frame *frame = d->factory->createFrame(data.mid(frameDataPosition),
      &d->header);

    if (!frame)
      return;

    // Checks to make sure that frame parsed correctly.

    if (frame->size() <= 0) {
      delete frame;
      return;
    }

    frameDataPosition += frame->size() + Frame::headerSize(d->header.majorVersion());
    addFrame(frame);
  }

  d->factory->rebuildAggregateFrames(this);

}

void ID3v2::Tag::setTextFrame(const ByteVector &id, const String &value) {

  if (value.isEmpty()) {
    removeFrames(id);
    return;
  }

  if (!d->frameListMap[id].isEmpty())
    d->frameListMap[id].front()->setText(value);
  else {
    const String::Type encoding = d->factory->defaultTextEncoding();
    TextIdentificationFrame *f = new TextIdentificationFrame(id, encoding);
    addFrame(f);
    f->setText(value);
  }

}
