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

#include "tfile.h"
#include "tfilestream.h"
#include "tstring.h"
#include "tdebug.h"
#include "tpropertymap.h"
#include "audioproperties.h"

using namespace Strawberry_TagLib::TagLib;

class File::FilePrivate {
 public:
  explicit FilePrivate(IOStream *_stream, bool _owner) : stream(_stream), streamOwner(_owner), valid(true) {}

  ~FilePrivate() {
    if (streamOwner)
      delete stream;
  }

  IOStream *stream;
  bool streamOwner;
  bool valid;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

File::~File() {
  delete d;
}

FileName File::name() const {
  return d->stream->name();
}

PropertyMap File::properties() const {
  return tag()->properties();
}

void File::removeUnsupportedProperties(const StringList &properties) {
  tag()->removeUnsupportedProperties(properties);

}

PropertyMap File::setProperties(const PropertyMap &properties) {
  return tag()->setProperties(properties);
}

ByteVector File::readBlock(size_t length) {
  return d->stream->readBlock(length);
}

void File::writeBlock(const ByteVector &data) {
  d->stream->writeBlock(data);
}

long long File::find(const ByteVector &pattern, long long fromOffset, const ByteVector &before) {

  if (!d->stream || pattern.size() > bufferSize())
    return -1;

  // The position in the file that the current buffer starts at.

  long long bufferOffset = fromOffset;

  // These variables are used to keep track of a partial match that happens at
  // the end of a buffer.

  size_t previousPartialMatch = ByteVector::npos();
  size_t beforePreviousPartialMatch = ByteVector::npos();

  // Save the location of the current read pointer.  We will restore the
  // position using seek() before all returns.

  long long originalPosition = tell();

  // Start the search at the offset.

  seek(fromOffset);

  // This loop is the crux of the find method.  There are three cases that we
  // want to account for:
  //
  // (1) The previously searched buffer contained a partial match of the search
  // pattern and we want to see if the next one starts with the remainder of
  // that pattern.
  //
  // (2) The search pattern is wholly contained within the current buffer.
  //
  // (3) The current buffer ends with a partial match of the pattern.  We will
  // note this for use in the next iteration, where we will check for the rest
  // of the pattern.
  //
  // All three of these are done in two steps.  First we check for the pattern
  // and do things appropriately if a match (or partial match) is found.  We
  // then check for "before".  The order is important because it gives priority
  // to "real" matches.

  for (ByteVector buffer = readBlock(bufferSize()); buffer.size() > 0; buffer = readBlock(bufferSize())) {

    // (1) previous partial match

    if (previousPartialMatch != ByteVector::npos() && bufferSize() > previousPartialMatch) {
      const size_t patternOffset = (bufferSize() - previousPartialMatch);
      if (buffer.containsAt(pattern, 0, patternOffset)) {
        seek(originalPosition);
        return bufferOffset - bufferSize() + previousPartialMatch;
      }
    }

    if (!before.isEmpty() && beforePreviousPartialMatch != ByteVector::npos() && bufferSize() > beforePreviousPartialMatch) {
      const size_t beforeOffset = (bufferSize() - beforePreviousPartialMatch);
      if (buffer.containsAt(before, 0, beforeOffset)) {
        seek(originalPosition);
        return -1;
      }
    }

    // (2) pattern contained in current buffer

    size_t location = buffer.find(pattern);
    if (location != ByteVector::npos()) {
      seek(originalPosition);
      return bufferOffset + location;
    }

    if (!before.isEmpty() && buffer.find(before) != ByteVector::npos()) {
      seek(originalPosition);
      return -1;
    }

    // (3) partial match

    previousPartialMatch = buffer.endsWithPartialMatch(pattern);

    if (!before.isEmpty())
      beforePreviousPartialMatch = buffer.endsWithPartialMatch(before);

    bufferOffset += bufferSize();
  }

  // Since we hit the end of the file, reset the status before continuing.

  clear();

  seek(originalPosition);

  return -1;

}


long long File::rfind(const ByteVector &pattern, long long fromOffset, const ByteVector &before) {

  if (!d->stream || pattern.size() > bufferSize())
    return -1;

  // Save the location of the current read pointer.  We will restore the
  // position using seek() before all returns.

  long long originalPosition = tell();

  // Start the search at the offset.

  if (fromOffset == 0)
    fromOffset = length();

  long long bufferLength = bufferSize();
  long long bufferOffset = fromOffset + pattern.size();

  // See the notes in find() for an explanation of this algorithm.

  while (true) {

    if (bufferOffset > bufferLength) {
      bufferOffset -= bufferLength;
    }
    else {
      bufferLength = bufferOffset;
      bufferOffset = 0;
    }
    seek(bufferOffset);

    const ByteVector buffer = readBlock(static_cast<size_t>(bufferLength));
    if (buffer.isEmpty())
      break;

    // TODO: (1) previous partial match

    // (2) pattern contained in current buffer

    const size_t location = buffer.rfind(pattern);
    if (location != ByteVector::npos()) {
      seek(originalPosition);
      return bufferOffset + location;
    }

    if (!before.isEmpty() && buffer.find(before) != ByteVector::npos()) {
      seek(originalPosition);
      return -1;
    }

    // TODO: (3) partial match
  }

  // Since we hit the end of the file, reset the status before continuing.

  clear();

  seek(originalPosition);

  return -1;

}

void File::insert(const ByteVector &data, long long start, size_t replace) {
  d->stream->insert(data, start, replace);
}

void File::removeBlock(long long start, size_t length) {
  d->stream->removeBlock(start, length);
}

bool File::readOnly() const {
  return d->stream->readOnly();
}

bool File::isOpen() const {
  return d->stream->isOpen();
}

bool File::isValid() const {
  return isOpen() && d->valid;
}

void File::seek(long long offset, Position p) {
  d->stream->seek(offset, IOStream::Position(p));
}

void File::truncate(long long length) {
  d->stream->truncate(length);
}

void File::clear() {
  d->stream->clear();
}

long long File::tell() const {
  return d->stream->tell();
}

long long File::length() {
  return d->stream->length();
}

String File::toString() const {

  StringList desc;
  AudioProperties *properties = audioProperties();
  if (properties) {
    desc.append(properties->toString());
  }
  Tag *t = tag();
  if (t) {
    desc.append(t->toString());
  }
  return desc.toString("\n");

}


////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

File::File(const FileName &fileName) : d(new FilePrivate(new FileStream(fileName), true)) {}

File::File(IOStream *stream) : d(new FilePrivate(stream, false)) {}

size_t File::bufferSize() {
  return FileStream::bufferSize();
}

void File::setValid(bool valid) {
  d->valid = valid;
}
