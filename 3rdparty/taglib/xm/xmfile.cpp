/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
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

#include "tstringlist.h"
#include "tdebug.h"
#include "xmfile.h"
#include "modfileprivate.h"
#include "tpropertymap.h"

#include <cstring>
#include <algorithm>

using namespace Strawberry_TagLib::TagLib;
using namespace XM;

/*!
 * The Reader classes are helpers to make handling of the stripped XM
 * format more easy. In the stripped XM format certain header sizes might
 * be smaller than one would expect. The fields that are not included
 * are then just some predefined valued (e.g. 0).
 *
 * Using these classes this code:
 *
 *   if(headerSize >= 4) {
 *     if(!readU16L(value1)) ERROR();
 *     if(headerSize >= 8) {
 *       if(!readU16L(value2)) ERROR();
 *       if(headerSize >= 12) {
 *         if(!readString(value3, 22)) ERROR();
 *         ...
 *       }
 *     }
 *   }
 *
 * Becomes:
 *
 *   StructReader header;
 *   header.u16L(value1).u16L(value2).string(value3, 22). ...;
 *   if(header.read(*this, headerSize) < std::min(header.size(), headerSize))
 *     ERROR();
 *
 * Maybe if this is useful to other formats these classes can be moved to
 * their own public files.
 */

namespace {
class Reader {
 public:
  virtual ~Reader() {
  }

  /*!
   * Reads associated values from \a file, but never reads more then \a limit bytes.
   */
  virtual unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) = 0;

  /*!
   * Returns the number of bytes this reader would like to read.
   */
  virtual unsigned int size() const = 0;
};

class SkipReader : public Reader {
 public:
  explicit SkipReader(unsigned int size) : m_size(size) {}

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {
    unsigned int count = std::min(m_size, limit);
    file.seek(count, Strawberry_TagLib::TagLib::File::Current);
    return count;
  }

  unsigned int size() const override {
    return m_size;
  }

 private:
  unsigned int m_size;
};

template<typename T>
class ValueReader : public Reader {
 public:
  explicit ValueReader(T &_value) : value(_value) {}

 protected:
  T &value;
};

class StringReader : public ValueReader<String> {
 public:
  StringReader(String &string, unsigned int size) : ValueReader<String>(string), m_size(size) {}

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {

    ByteVector data = file.readBlock(std::min(m_size, limit));
    size_t count = data.size();
    size_t index = data.find('\0');
    if (index != ByteVector::npos()) {
      data.resize(index);
    }
    data.replace('\xff', ' ');
    value = data;
    return count;

  }

  unsigned int size() const override {
    return m_size;
  }

 private:
  unsigned int m_size;
};

class ByteReader : public ValueReader<unsigned char> {
 public:
  explicit ByteReader(unsigned char _byte) : ValueReader<unsigned char>(_byte) {}

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {
    ByteVector data = file.readBlock(std::min(1U, limit));
    if (data.size() > 0) {
      value = data[0];
    }
    return data.size();
  }

  unsigned int size() const override {
    return 1;
  }
};

template<typename T>
class NumberReader : public ValueReader<T> {
 public:
  NumberReader(T &_value, bool _bigEndian) : ValueReader<T>(_value), bigEndian(_bigEndian) {
  }

 protected:
  bool bigEndian;
};

class U16Reader : public NumberReader<unsigned short> {
 public:
  U16Reader(unsigned short &_value, bool _bigEndian)
      : NumberReader<unsigned short>(_value, _bigEndian) {}

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {
    ByteVector data = file.readBlock(std::min(2U, limit));

    if (bigEndian)
      value = data.toUInt16BE(0);
    else
      value = data.toUInt16LE(0);

    return static_cast<unsigned int>(data.size());
  }

  unsigned int size() const override {
    return 2;
  }
};

class U32Reader : public NumberReader<unsigned int> {
 public:
  explicit U32Reader(unsigned int _value, bool _bigEndian = true) : NumberReader<unsigned int>(_value, _bigEndian) {
  }

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {
    ByteVector data = file.readBlock(std::min(4U, limit));

    if (bigEndian)
      value = data.toUInt32BE(0);
    else
      value = data.toUInt32LE(0);

    return static_cast<unsigned int>(data.size());
  }

  unsigned int size() const override {
    return 4;
  }
};

class StructReader : public Reader {
 public:
  StructReader() {
    m_readers.setAutoDelete(true);
  }

  /*!
   * Add a nested reader. This reader takes ownership.
   */
  StructReader &reader(Reader *reader) {
    m_readers.append(reader);
    return *this;
  }

  /*!
   * Don't read anything but skip \a size bytes.
   */
  StructReader &skip(unsigned int size) {
    m_readers.append(new SkipReader(size));
    return *this;
  }

  /*!
   * Read a string of \a size characters (bytes) into \a string.
   */
  StructReader &string(String &string, unsigned int size) {
    m_readers.append(new StringReader(string, size));
    return *this;
  }

  /*!
   * Read a byte into \a byte.
   */
  StructReader &byte(unsigned char &byte) {
    m_readers.append(new ByteReader(byte));
    return *this;
  }

  /*!
   * Read a unsigned 16 Bit integer into \a number. The byte order
   * is controlled by \a bigEndian.
   */
  StructReader &u16(unsigned short &number, bool bigEndian) {
    m_readers.append(new U16Reader(number, bigEndian));
    return *this;
  }

  /*!
   * Read a unsigned 16 Bit little endian integer into \a number.
   */
  StructReader &u16L(unsigned short &number) {
    return u16(number, false);
  }

  /*!
   * Read a unsigned 16 Bit big endian integer into \a number.
   */
  StructReader &u16B(unsigned short &number) {
    return u16(number, true);
  }

  /*!
   * Read a unsigned 32 Bit integer into \a number. The byte order
   * is controlled by \a bigEndian.
   */
  StructReader &u32(unsigned int number, bool bigEndian) {
    m_readers.append(new U32Reader(number, bigEndian));
    return *this;
  }

  /*!
   * Read a unsigned 32 Bit little endian integer into \a number.
   */
  StructReader &u32L(unsigned int number) {
    return u32(number, false);
  }

  /*!
   * Read a unsigned 32 Bit big endian integer into \a number.
   */
  StructReader &u32B(unsigned int number) {
    return u32(number, true);
  }

  unsigned int size() const override {
    unsigned int size = 0;
    for (List<Reader *>::ConstIterator i = m_readers.begin();
         i != m_readers.end();
         ++i) {
      size += (*i)->size();
    }
    return size;
  }

  unsigned int read(Strawberry_TagLib::TagLib::File &file, unsigned int limit) override {
    unsigned int sumcount = 0;
    for (List<Reader *>::ConstIterator i = m_readers.begin();
         limit > 0 && i != m_readers.end();
         ++i) {
      unsigned int count = (*i)->read(file, limit);
      limit -= count;
      sumcount += count;
    }
    return sumcount;
  }

 private:
  List<Reader *> m_readers;
};
}  // namespace

class XM::File::FilePrivate {
 public:
  explicit FilePrivate(AudioProperties::ReadStyle propertiesStyle) : properties(propertiesStyle) {}

  Mod::Tag tag;
  XM::AudioProperties properties;
};

XM::File::File(FileName file, bool readProperties,
  AudioProperties::ReadStyle propertiesStyle) : Mod::FileBase(file),
                                                d(new FilePrivate(propertiesStyle)) {
  if (isOpen())
    read(readProperties);
}

XM::File::File(IOStream *stream, bool readProperties,
  AudioProperties::ReadStyle propertiesStyle) : Mod::FileBase(stream),
                                                d(new FilePrivate(propertiesStyle)) {
  if (isOpen())
    read(readProperties);
}

XM::File::~File() {
  delete d;
}

Mod::Tag *XM::File::tag() const {
  return &d->tag;
}

XM::AudioProperties *XM::File::audioProperties() const {
  return &d->properties;
}

bool XM::File::save() {

  if (readOnly()) {
    debug("XM::File::save() - Cannot save to a read only file.");
    return false;
  }

  seek(17);
  writeString(d->tag.title(), 20);

  seek(38);
  writeString(d->tag.trackerName(), 20);

  seek(60);
  unsigned int headerSize = 0;
  if (!readU32L(headerSize))
    return false;

  seek(70);
  unsigned short patternCount = 0;
  unsigned short instrumentCount = 0;
  if (!readU16L(patternCount) || !readU16L(instrumentCount))
    return false;

  long pos = 60 + headerSize;  // should be long long in taglib2.

  // need to read patterns again in order to seek to the instruments:
  for (unsigned short i = 0; i < patternCount; ++i) {
    seek(pos);
    unsigned int patternHeaderLength = 0;
    if (!readU32L(patternHeaderLength) || patternHeaderLength < 4)
      return false;

    seek(pos + 7);
    unsigned short dataSize = 0;
    if (!readU16L(dataSize))
      return false;

    pos += patternHeaderLength + dataSize;
  }

  const StringList lines = d->tag.comment().split("\n");
  unsigned int sampleNameIndex = instrumentCount;
  for (unsigned short i = 0; i < instrumentCount; ++i) {
    seek(pos);
    unsigned int instrumentHeaderSize = 0;
    if (!readU32L(instrumentHeaderSize) || instrumentHeaderSize < 4)
      return false;

    seek(pos + 4);
    const unsigned int len = std::min(22U, instrumentHeaderSize - 4U);
    if (i >= lines.size())
      writeString(String(), len);
    else
      writeString(lines[i], len);

    unsigned short sampleCount = 0;
    if (instrumentHeaderSize >= 29U) {
      seek(pos + 27);
      if (!readU16L(sampleCount))
        return false;
    }

    unsigned int sampleHeaderSize = 0;
    if (sampleCount > 0) {
      seek(pos + 29);
      if (instrumentHeaderSize < 33U || !readU32L(sampleHeaderSize))
        return false;
    }

    pos += instrumentHeaderSize;

    for (unsigned short j = 0; j < sampleCount; ++j) {
      if (sampleHeaderSize > 4U) {
        seek(pos);
        unsigned int sampleLength = 0;
        if (!readU32L(sampleLength))
          return false;

        if (sampleHeaderSize > 18U) {
          seek(pos + 18);
          const unsigned int len2 = std::min(sampleHeaderSize - 18U, 22U);
          if (sampleNameIndex >= lines.size())
            writeString(String(), len2);
          else
            writeString(lines[sampleNameIndex++], len2);
        }
      }
      pos += sampleHeaderSize;
    }
  }

  return true;

}

void XM::File::read(bool) {

  if (!isOpen())
    return;

  seek(0);
  ByteVector magic = readBlock(17);
  // it's all 0x00 for stripped XM files:
  READ_ASSERT(magic == "Extended Module: " || magic == ByteVector(17, 0));

  READ_STRING(d->tag.setTitle, 20);
  READ_BYTE_AS(escape);
  // in stripped XM files this is 0x00:
  READ_ASSERT(escape == 0x1A || escape == 0x00);

  READ_STRING(d->tag.setTrackerName, 20);
  READ_U16L(d->properties.setVersion);

  READ_U32L_AS(headerSize);
  READ_ASSERT(headerSize >= 4);

  unsigned short length = 0;
  unsigned short restartPosition = 0;
  unsigned short channels = 0;
  unsigned short patternCount = 0;
  unsigned short instrumentCount = 0;
  unsigned short flags = 0;
  unsigned short tempo = 0;
  unsigned short bpmSpeed = 0;

  StructReader header;
  header.u16L(length)
    .u16L(restartPosition)
    .u16L(channels)
    .u16L(patternCount)
    .u16L(instrumentCount)
    .u16L(flags)
    .u16L(tempo)
    .u16L(bpmSpeed);

  unsigned int count = header.read(*this, headerSize - 4U);
  unsigned int size = std::min(headerSize - 4U, header.size());

  READ_ASSERT(count == size);

  d->properties.setLengthInPatterns(length);
  d->properties.setRestartPosition(restartPosition);
  d->properties.setChannels(channels);
  d->properties.setPatternCount(patternCount);
  d->properties.setInstrumentCount(instrumentCount);
  d->properties.setFlags(flags);
  d->properties.setTempo(tempo);
  d->properties.setBpmSpeed(bpmSpeed);

  seek(60 + headerSize);

  // read patterns:
  for (unsigned short i = 0; i < patternCount; ++i) {
    READ_U32L_AS(patternHeaderLength);
    READ_ASSERT(patternHeaderLength >= 4);

    unsigned char packingType = 0;
    unsigned short rowCount = 0;
    unsigned short dataSize = 0;
    StructReader pattern;
    pattern.byte(packingType).u16L(rowCount).u16L(dataSize);

    unsigned int count2 = pattern.read(*this, patternHeaderLength - 4U);
    READ_ASSERT(count2 == std::min(patternHeaderLength - 4U, pattern.size()));

    seek(patternHeaderLength - (4 + count2) + dataSize, Current);
  }

  StringList intrumentNames;
  StringList sampleNames;
  unsigned int sumSampleCount = 0;

  // read instruments:
  for (unsigned short i = 0; i < instrumentCount; ++i) {
    READ_U32L_AS(instrumentHeaderSize);
    READ_ASSERT(instrumentHeaderSize >= 4);

    String instrumentName;
    unsigned char instrumentType = 0;
    unsigned short sampleCount = 0;

    StructReader instrument;
    instrument.string(instrumentName, 22).byte(instrumentType).u16L(sampleCount);

    // 4 for instrumentHeaderSize
    unsigned int count2 = 4 + instrument.read(*this, instrumentHeaderSize - 4U);
    READ_ASSERT(count2 == std::min(instrumentHeaderSize, instrument.size() + 4));

    long offset = 0;
    if (sampleCount > 0) {
      unsigned int sampleHeaderSize = 0;
      sumSampleCount += sampleCount;
      // wouldn't know which header size to assume otherwise:
      READ_ASSERT(instrumentHeaderSize >= count2 + 4 && readU32L(sampleHeaderSize));
      // skip unhandled header proportion:
      seek(instrumentHeaderSize - count2 - 4, Current);

      for (unsigned short j = 0; j < sampleCount; ++j) {
        unsigned int sampleLength = 0;
        unsigned int loopStart = 0;
        unsigned int loopLength = 0;
        unsigned char volume = 0;
        unsigned char finetune = 0;
        unsigned char sampleType = 0;
        unsigned char panning = 0;
        unsigned char noteNumber = 0;
        unsigned char compression = 0;
        String sampleName;
        StructReader sample;
        sample.u32L(sampleLength)
          .u32L(loopStart)
          .u32L(loopLength)
          .byte(volume)
          .byte(finetune)
          .byte(sampleType)
          .byte(panning)
          .byte(noteNumber)
          .byte(compression)
          .string(sampleName, 22);

        unsigned int count3 = sample.read(*this, sampleHeaderSize);
        READ_ASSERT(count3 == std::min(sampleHeaderSize, sample.size()));
        // skip unhandled header proportion:
        seek(sampleHeaderSize - count3, Current);

        offset += sampleLength;
        sampleNames.append(sampleName);
      }
    }
    else {
      offset = instrumentHeaderSize - count2;
    }
    intrumentNames.append(instrumentName);
    seek(offset, Current);
  }

  d->properties.setSampleCount(sumSampleCount);
  String comment(intrumentNames.toString("\n"));
  if (!sampleNames.isEmpty()) {
    comment += "\n";
    comment += sampleNames.toString("\n");
  }
  d->tag.setComment(comment);

}
