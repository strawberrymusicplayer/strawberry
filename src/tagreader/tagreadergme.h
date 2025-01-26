/*
 * Strawberry Music Player
 * Copyright 2022, Eoin O'Neill <eoinoneill1991@gmail.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef TAGREADERGME_H
#define TAGREADERGME_H

#include <QByteArray>
#include <QString>
#include <QFileInfo>

#include "tagreaderbase.h"

namespace GME {
bool IsSupportedFormat(const QFileInfo &fileinfo);
TagReaderResult ReadFile(const QFileInfo &fileinfo, Song *song);

uint32_t UnpackBytes32(const char *const bytes, size_t length);

namespace SPC {
// SPC SPEC: http://vspcplay.raphnet.net/spc_file_format.txt

constexpr int HAS_ID6_OFFSET = 0x23;
constexpr int SONG_TITLE_OFFSET = 0x2E;
constexpr int GAME_TITLE_OFFSET = 0x4E;
constexpr int DUMPER_OFFSET = 0x6E;
constexpr int COMMENTS_OFFSET = 0x7E;
// It seems that intro length and fade length are inconsistent from file to file.
// It should be looked into within the GME source code to see how GStreamer gets its values for playback length.
constexpr int INTRO_LENGTH_OFFSET = 0xA9;
constexpr int INTRO_LENGTH_SIZE = 3;
constexpr int FADE_LENGTH_OFFSET = 0xAC;
constexpr int FADE_LENGTH_SIZE = 4;
constexpr int ARTIST_OFFSET = 0xB1;
constexpr int XID6_OFFSET = (0x101C0 + 64);

constexpr int NANO_PER_MS = 1000000;

enum class xID6_STATUS {
  ON = 0x26,
  OFF = 0x27
};

enum class xID6_ID {
  SongName = 0x01,
  GameName = 0x02,
  ArtistName = 0x03
};

enum class xID6_TYPE {
  Length = 0x0,
  String = 0x1,
  Integer = 0x4
};

TagReaderResult Read(const QFileInfo &fileinfo, Song *song);
qint16 GetNextMemAddressAlign32bit(qint16 input);
quint64 ConvertSPCStringToNum(const QByteArray &arr);
}  // namespace SPC

namespace VGM {
// VGM SPEC:
// http://www.smspower.org/uploads/Music/vgmspec170.txt?sid=17c810c54633b6dd4982f92f718361c1
// GD3 TAG SPEC:
// http://www.smspower.org/uploads/Music/gd3spec100.txt
constexpr int GD3_TAG_PTR = 0x14;
constexpr int SAMPLE_COUNT = 0x18;
constexpr int LOOP_SAMPLE_COUNT = 0x20;
constexpr int SAMPLE_TIMEBASE = 44100;
constexpr int GST_GME_LOOP_TIME_MS = 8000;

TagReaderResult Read(const QFileInfo &fileinfo, Song *song);
// Takes in two QByteArrays, expected to be 4 bytes long. Desired length is returned via output parameter out_length. Returns false on error.
bool GetPlaybackLength(const QByteArray &sample_count_bytes, const QByteArray &loop_count_bytes, quint64 &out_length);

}  // namespace VGM

}  // namespace GME

// TagReaderGME
// Fulfills Strawberry's Intended interface for tag reading.
class TagReaderGME : public TagReaderBase {

 public:
  explicit TagReaderGME();

  TagReaderResult IsMediaFile(const QString &filename) const override;

  TagReaderResult ReadFile(const QString &filename, Song *song) const override;

#ifdef HAVE_STREAMTAGREADER
  TagReaderResult ReadStream(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song) const override;
#endif

  TagReaderResult WriteFile(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) const override;

  TagReaderResult LoadEmbeddedCover(const QString &filename, QByteArray &data) const override;
  TagReaderResult SaveEmbeddedCover(const QString &filename, const SaveTagCoverData &save_tag_cover_data) const override;

  TagReaderResult SaveSongPlaycount(const QString &filename, const uint playcount) const override;
  TagReaderResult SaveSongRating(const QString &filename, const float rating) const override;
};

#endif  // TAGREADERGME_H
