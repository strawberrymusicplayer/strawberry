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

#include "tstring.h"
#include "tdebug.h"

#include "oggpage.h"
#include "oggpageheader.h"
#include "oggfile.h"

using namespace Strawberry_TagLib::TagLib;

namespace {
ByteVector checksum(const ByteVector &v) {
  const unsigned int crcTable[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
  };

  unsigned int sum = 0;
  for (ByteVector::ConstIterator it = v.begin(); it != v.end(); ++it)
    sum = (sum << 8) ^ crcTable[((sum >> 24) & 0xff) ^ static_cast<unsigned char>(*it)];

  return ByteVector::fromUInt32LE(sum);
}
}  // namespace

class Ogg::Page::PagePrivate {
 public:
  explicit PagePrivate(File *f = nullptr, long long pageOffset = -1) : file(f), fileOffset(pageOffset), header(f, pageOffset), firstPacketIndex(-1) {}

  File *file;
  long long fileOffset;
  PageHeader header;
  int firstPacketIndex;
  ByteVectorList packets;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Ogg::Page::Page(Ogg::File *file, long long pageOffset) : d(new PagePrivate(file, pageOffset)) {}

Ogg::Page::~Page() {
  delete d;
}

long long Ogg::Page::fileOffset() const {
  return d->fileOffset;
}

const Ogg::PageHeader *Ogg::Page::header() const {
  return &d->header;
}

int Ogg::Page::pageSequenceNumber() const {
  return d->header.pageSequenceNumber();
}

void Ogg::Page::setPageSequenceNumber(int sequenceNumber) {
  d->header.setPageSequenceNumber(sequenceNumber);
}

int Ogg::Page::firstPacketIndex() const {
  return d->firstPacketIndex;
}

void Ogg::Page::setFirstPacketIndex(int index) {
  d->firstPacketIndex = index;
}

Ogg::Page::ContainsPacketFlags Ogg::Page::containsPacket(int index) const {

  const int lastPacketIndex = d->firstPacketIndex + packetCount() - 1;
  if (index < d->firstPacketIndex || index > lastPacketIndex)
    return DoesNotContainPacket;

  ContainsPacketFlags flags = DoesNotContainPacket;

  if (index == d->firstPacketIndex)
    flags = ContainsPacketFlags(flags | BeginsWithPacket);

  if (index == lastPacketIndex)
    flags = ContainsPacketFlags(flags | EndsWithPacket);

  // If there's only one page and it's complete:

  if (packetCount() == 1 &&
    !d->header.firstPacketContinued() &&
    d->header.lastPacketCompleted()) {
    flags = ContainsPacketFlags(flags | CompletePacket);
  }

  // Or if there is more than one page and the page is
  // (a) the first page and it's complete or
  // (b) the last page and it's complete or
  // (c) a page in the middle.
  else if (packetCount() > 1 &&
    ((flags & BeginsWithPacket && !d->header.firstPacketContinued()) ||
      (flags & EndsWithPacket && d->header.lastPacketCompleted()) ||
      (!(flags & BeginsWithPacket) && !(flags & EndsWithPacket)))) {
    flags = ContainsPacketFlags(flags | CompletePacket);
  }

  return flags;

}

unsigned int Ogg::Page::packetCount() const {
  return d->header.packetSizes().size();
}

ByteVectorList Ogg::Page::packets() const {

  if (!d->packets.isEmpty())
    return d->packets;

  ByteVectorList l;

  if (d->file && d->header.isValid()) {

    d->file->seek(d->fileOffset + d->header.size());

    List<int> packetSizes = d->header.packetSizes();

    List<int>::ConstIterator it = packetSizes.begin();
    for (; it != packetSizes.end(); ++it)
      l.append(d->file->readBlock(*it));
  }
  else
    debug("Ogg::Page::packets() -- attempting to read packets from an invalid page.");

  return l;

}

int Ogg::Page::size() const {
  return d->header.size() + d->header.dataSize();
}

ByteVector Ogg::Page::render() const {

  ByteVector data;

  data.append(d->header.render());

  if (d->packets.isEmpty()) {
    if (d->file) {
      d->file->seek(d->fileOffset + d->header.size());
      data.append(d->file->readBlock(d->header.dataSize()));
    }
    else
      debug("Ogg::Page::render() -- this page is empty!");
  }
  else {
    ByteVectorList::ConstIterator it = d->packets.begin();
    for (; it != d->packets.end(); ++it)
      data.append(*it);
  }

  // Compute and set the checksum for the Ogg page.  The checksum is taken over
  // the entire page with the 4 bytes reserved for the checksum zeroed and then
  // inserted in bytes 22-25 of the page header.

  const ByteVector sum = checksum(data);
  std::copy(sum.begin(), sum.end(), data.begin() + 22);

  return data;

}

List<Ogg::Page *> Ogg::Page::paginate(const ByteVectorList &packets, PaginationStrategy strategy, unsigned int streamSerialNumber, int firstPage, bool firstPacketContinued, bool lastPacketCompleted, bool containsLastPacket) {

  // SplitSize must be a multiple of 255 in order to get the lacing values right
  // create pages of about 8KB each

  static const unsigned int SplitSize = 32 * 255;

  // Force repagination if the segment table will exceed the size limit.

  if (strategy != Repaginate) {

    size_t tableSize = 0;
    for (ByteVectorList::ConstIterator it = packets.begin(); it != packets.end(); ++it)
      tableSize += it->size() / 255 + 1;

    if (tableSize > 255)
      strategy = Repaginate;
  }

  List<Page *> l;

  // Handle creation of multiple pages with appropriate pagination.

  if (strategy == Repaginate) {

    int pageIndex = firstPage;

    for (ByteVectorList::ConstIterator it = packets.begin(); it != packets.end(); ++it) {

      const bool lastPacketInList = (it == --packets.end());

      // mark very first packet?

      bool continued = (firstPacketContinued && it == packets.begin());
      unsigned int pos = 0;

      while (pos < it->size()) {

        const bool lastSplit = (pos + SplitSize >= it->size());

        ByteVectorList packetList;
        packetList.append(it->mid(pos, SplitSize));

        l.append(new Page(packetList,
          streamSerialNumber,
          pageIndex,
          continued,
          lastSplit && (lastPacketInList ? lastPacketCompleted : true),
          lastSplit && (containsLastPacket && lastPacketInList)));
        pageIndex++;
        continued = true;

        pos += SplitSize;
      }
    }
  }
  else {
    l.append(new Page(packets,
      streamSerialNumber,
      firstPage,
      firstPacketContinued,
      lastPacketCompleted,
      containsLastPacket));
  }

  return l;

}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

Ogg::Page::Page(const ByteVectorList &packets, unsigned int streamSerialNumber, int pageNumber, bool firstPacketContinued, bool lastPacketCompleted, bool containsLastPacket) : d(new PagePrivate()) {

  d->header.setFirstPageOfStream(pageNumber == 0 && !firstPacketContinued);
  d->header.setLastPageOfStream(containsLastPacket);
  d->header.setFirstPacketContinued(firstPacketContinued);
  d->header.setLastPacketCompleted(lastPacketCompleted);
  d->header.setStreamSerialNumber(streamSerialNumber);
  d->header.setPageSequenceNumber(pageNumber);

  // Build a page from the list of packets.

  ByteVector data;
  List<int> packetSizes;

  for (ByteVectorList::ConstIterator it = packets.begin(); it != packets.end(); ++it) {
    packetSizes.append((*it).size());
    data.append(*it);
  }
  d->packets = packets;
  d->header.setPacketSizes(packetSizes);

}
