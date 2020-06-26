/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
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

#include "tdebug.h"
#include "tstring.h"
#include "mp4file.h"
#include "mp4atom.h"
#include "mp4properties.h"

using namespace Strawberry_TagLib::TagLib;

class MP4::AudioProperties::AudioPropertiesPrivate {
 public:
  explicit AudioPropertiesPrivate() : length(0),
                                      bitrate(0),
                                      sampleRate(0),
                                      channels(0),
                                      bitsPerSample(0),
                                      encrypted(false),
                                      codec(MP4::AudioProperties::Unknown) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;
  bool encrypted;
  Codec codec;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

MP4::AudioProperties::AudioProperties(File *file, MP4::Atoms *atoms, ReadStyle) : Strawberry_TagLib::TagLib::AudioProperties(), d(new AudioPropertiesPrivate()) {
  read(file, atoms);
}

MP4::AudioProperties::~AudioProperties() {
  delete d;
}

int MP4::AudioProperties::channels() const {
  return d->channels;
}

int MP4::AudioProperties::sampleRate() const {
  return d->sampleRate;
}

int MP4::AudioProperties::lengthInSeconds() const {
  return d->length / 1000;
}

int MP4::AudioProperties::lengthInMilliseconds() const {
  return d->length;
}

int MP4::AudioProperties::bitrate() const {
  return d->bitrate;
}

int MP4::AudioProperties::bitsPerSample() const {
  return d->bitsPerSample;
}

bool MP4::AudioProperties::isEncrypted() const {
  return d->encrypted;
}

MP4::AudioProperties::Codec
MP4::AudioProperties::codec() const {
  return d->codec;
}

String MP4::AudioProperties::toString() const {

  String format;
  if (d->codec == AAC) {
    format = "AAC";
  }
  else if (d->codec == ALAC) {
    format = "ALAC";
  }
  else {
    format = "Unknown";
  }
  StringList desc;
  desc.append("MPEG-4 audio (" + format + ")");
  desc.append(String::number(lengthInSeconds()) + " seconds");
  desc.append(String::number(bitrate()) + " kbps");
  return desc.toString(", ");

}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void MP4::AudioProperties::read(File *file, Atoms *atoms) {

  MP4::Atom *moov = atoms->find("moov");
  if (!moov) {
    debug("MP4: Atom 'moov' not found");
    return;
  }

  MP4::Atom *trak = nullptr;
  ByteVector data;

  const MP4::AtomList trakList = moov->findall("trak");
  for (MP4::AtomList::ConstIterator it = trakList.begin(); it != trakList.end(); ++it) {
    trak = *it;
    MP4::Atom *hdlr = trak->find("mdia", "hdlr");
    if (!hdlr) {
      debug("MP4: Atom 'trak.mdia.hdlr' not found");
      return;
    }
    file->seek(hdlr->offset);
    data = file->readBlock(hdlr->length);
    if (data.containsAt("soun", 16)) {
      break;
    }
    trak = nullptr;
  }
  if (!trak) {
    debug("MP4: No audio tracks");
    return;
  }

  MP4::Atom *mdhd = trak->find("mdia", "mdhd");
  if (!mdhd) {
    debug("MP4: Atom 'trak.mdia.mdhd' not found");
    return;
  }

  file->seek(mdhd->offset);
  data = file->readBlock(mdhd->length);

  const unsigned int version = data[8];
  long long unit;
  long long length;
  if (version == 1) {
    if (data.size() < 36 + 8) {
      debug("MP4: Atom 'trak.mdia.mdhd' is smaller than expected");
      return;
    }
    unit = data.toUInt32BE(28);
    length = data.toInt64BE(32);
  }
  else {
    if (data.size() < 24 + 8) {
      debug("MP4: Atom 'trak.mdia.mdhd' is smaller than expected");
      return;
    }
    unit = data.toUInt32BE(20);
    length = data.toUInt32BE(24);
  }
  if (unit > 0 && length > 0)
    d->length = static_cast<int>(length * 1000.0 / unit + 0.5);

  MP4::Atom *atom = trak->find("mdia", "minf", "stbl", "stsd");
  if (!atom) {
    return;
  }

  file->seek(atom->offset);
  data = file->readBlock(atom->length);
  if (data.containsAt("mp4a", 20)) {
    d->codec = AAC;
    d->channels = data.toUInt16BE(40);
    d->bitsPerSample = data.toUInt16BE(42);
    d->sampleRate = data.toUInt32BE(46);
    if (data.containsAt("esds", 56) && data[64] == 0x03) {
      unsigned int pos = 65;
      if (data.containsAt("\x80\x80\x80", pos)) {
        pos += 3;
      }
      pos += 4;
      if (data[pos] == 0x04) {
        pos += 1;
        if (data.containsAt("\x80\x80\x80", pos)) {
          pos += 3;
        }
        pos += 10;
        d->bitrate = static_cast<int>((data.toUInt32BE(pos) + 500) / 1000.0 + 0.5);
      }
    }
  }
  else if (data.containsAt("alac", 20)) {
    if (atom->length == 88 && data.containsAt("alac", 56)) {
      d->codec = ALAC;
      d->bitsPerSample = data.at(69);
      d->channels = data.at(73);
      d->bitrate = static_cast<int>(data.toUInt32BE(80) / 1000.0 + 0.5);
      d->sampleRate = data.toUInt32BE(84);
    }
  }

  MP4::Atom *drms = atom->find("drms");
  if (drms) {
    d->encrypted = true;
  }

}
