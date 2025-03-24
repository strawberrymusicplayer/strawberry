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

#include "tagreadergme.h"

#include <taglib/apefile.h>

#include <QByteArray>
#include <QString>
#include <QChar>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

#include "constants/timeconstants.h"
#include "core/logging.h"
#include "tagreaderbase.h"
#include "tagreadertaglib.h"

using namespace Qt::Literals::StringLiterals;

#undef TStringToQString
#undef QStringToTString

bool GME::IsSupportedFormat(const QFileInfo &fileinfo) {
  return fileinfo.exists() && (fileinfo.completeSuffix().endsWith("spc"_L1, Qt::CaseInsensitive) || fileinfo.completeSuffix().endsWith("vgm"_L1, Qt::CaseInsensitive));
}

TagReaderResult GME::ReadFile(const QFileInfo &fileinfo, Song *song) {

  if (fileinfo.completeSuffix().endsWith("spc"_L1, Qt::CaseInsensitive)) {
    return SPC::Read(fileinfo, song);
  }
  if (fileinfo.completeSuffix().endsWith("vgm"_L1, Qt::CaseInsensitive)) {
    return VGM::Read(fileinfo, song);
  }

  return TagReaderResult::ErrorCode::Unsupported;

}

quint32 GME::UnpackBytes32(const char *const bytes, size_t length) {

  Q_ASSERT(length <= 4 && length > 0);

  quint32 value = 0;
  for (size_t i = 0; i < length; i++) {
    value |= static_cast<unsigned char>(bytes[i]) << (8 * i);
  }

  return value;

}

TagReaderResult GME::SPC::Read(const QFileInfo &fileinfo, Song *song) {

  QFile file(fileinfo.filePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return TagReaderResult(TagReaderResult::ErrorCode::FileOpenError, file.errorString());
  }

  qLog(Debug) << "Reading tags from SPC file" << fileinfo.fileName();

  // Check for header -- more reliable than file name alone.
  if (!file.read(33).startsWith("SNES-SPC700")) {
    return TagReaderResult::ErrorCode::Unsupported;
  }

  // First order of business -- get any tag values that exist within the core file information.
  // These only allow for a certain number of bytes per field,
  // so they will likely be overwritten either by the id666 standard or the APETAG format (as used by other players, such as foobar and winamp)
  //
  //  Make sure to check id6 documentation before changing the read values!

  file.seek(HAS_ID6_OFFSET);
  const QByteArray id6_status = file.read(1);
  const bool has_id6 = id6_status.length() >= 1 && id6_status[0] == static_cast<char>(xID6_STATUS::ON);

  file.seek(SONG_TITLE_OFFSET);
  song->set_title(QString::fromLatin1(file.read(32)).toStdString());

  file.seek(GAME_TITLE_OFFSET);
  song->set_album(QString::fromLatin1(file.read(32)).toStdString());

  file.seek(ARTIST_OFFSET);
  song->set_artist(QString::fromLatin1(file.read(32)).toStdString());

  file.seek(INTRO_LENGTH_OFFSET);
  QByteArray length_bytes = file.read(INTRO_LENGTH_SIZE);
  if (length_bytes.size() >= INTRO_LENGTH_SIZE) {
    quint64 length_in_sec = ConvertSPCStringToNum(length_bytes);

    if (length_in_sec <= 0 || length_in_sec >= 0x1FFF) {
      // This means that parsing the length as a string failed, so get value LE.
      length_in_sec = static_cast<quint64>(length_bytes[0] | (length_bytes[1] << 8) | (length_bytes[2] << 16));
    }

    if (length_in_sec < 0x1FFF) {
      song->set_length_nanosec(static_cast<qint64>(length_in_sec * kNsecPerSec));
    }
  }

  file.seek(FADE_LENGTH_OFFSET);
  QByteArray fade_bytes = file.read(FADE_LENGTH_SIZE);
  if (fade_bytes.size() >= FADE_LENGTH_SIZE) {
    quint64 fade_length_in_ms = ConvertSPCStringToNum(fade_bytes);

    if (fade_length_in_ms > 0x7FFF) {
      fade_length_in_ms = static_cast<quint64>(fade_bytes[0] | (fade_bytes[1] << 8) | (fade_bytes[2] << 16) | (fade_bytes[3] << 24));
    }
    Q_UNUSED(fade_length_in_ms)
  }

  // Check for XID6 data -- this is infrequently used, but being able to fill in data from this is ideal before trying to rely on APETAG values.
  // XID6 format follows EA's binary file format standard named "IFF"
  file.seek(XID6_OFFSET);
  if (has_id6 && file.read(4) == "xid6") {
    QByteArray xid6_head_data = file.read(4);
    if (xid6_head_data.size() >= 4) {
      qint64 xid6_size = xid6_head_data[0] | (xid6_head_data[1] << 8) | (xid6_head_data[2] << 16) | xid6_head_data[3];
      // This should be the size remaining for entire ID6 block, but it seems that most files treat this as the size of the remaining header space...

      qLog(Debug) << fileinfo.fileName() << "has ID6 tag.";

      while ((file.pos()) + 4 < XID6_OFFSET + xid6_size) {
        QByteArray arr = file.read(4);
        if (arr.size() < 4) break;

        qint8 id = arr[0];
        qint8 type = arr[1];
        Q_UNUSED(id);
        Q_UNUSED(type);
        qint16 length = static_cast<qint16>(arr[2] | (arr[3] << 8));

        file.read(GetNextMemAddressAlign32bit(length));
      }
    }
  }

  // Music Players that support SPC tend to support additional tagging data as
  // an APETAG entry at the bottom of the file instead of writing into the xid6 tagging space.
  // This is where a lot of the extra data for a file is stored, such as genre or replaygain data.
  // This data is currently supported by TagLib, so we will simply use that for the remaining values.
  TagLib::APE::File ape(fileinfo.filePath().toStdString().data());
  if (ape.hasAPETag()) {
    TagLib::Tag *tag = ape.tag();
    if (!tag) {
      return TagReaderResult::ErrorCode::FileParseError;
    }

    song->set_artist(tag->artist());
    song->set_album(tag->album());
    song->set_title(tag->title());
    song->set_genre(tag->genre());
    song->set_track(static_cast<std::int32_t>(tag->track()));
    song->set_year(static_cast<std::int32_t>(tag->year()));
  }

  song->set_valid(true);
  song->set_filetype(Song::FileType::SPC);

  return TagReaderResult::ErrorCode::Success;

}

qint16 GME::SPC::GetNextMemAddressAlign32bit(qint16 input) {
  return static_cast<qint16>((input + 0x3) & ~0x3);
  // Plus 0x3 for rounding up (not down), AND NOT to flatten out on a 32 bit level.
}

quint64 GME::SPC::ConvertSPCStringToNum(const QByteArray &arr) {

  quint64 result = 0;
  for (auto it = arr.begin(); it != arr.end(); it++) {
    const uint num = static_cast<uint>(*it - '0');
    if (num > 9) break;
    result = (result * 10) + num;  // Shift Left and add.
  }

  return result;

}

TagReaderResult GME::VGM::Read(const QFileInfo &fileinfo, Song *song) {

  QFile file(fileinfo.filePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return TagReaderResult(TagReaderResult::ErrorCode::FileOpenError, file.errorString());
  }

  qLog(Debug) << "Reading tags from VGM file" << fileinfo.filePath();

  if (!file.read(4).startsWith("Vgm ")) {
    return TagReaderResult::ErrorCode::Unsupported;
  }

  file.seek(GD3_TAG_PTR);
  QByteArray gd3_head = file.read(4);
  if (gd3_head.size() < 4) {
    return TagReaderResult::ErrorCode::FileParseError;
  }

  quint64 pt = GME::UnpackBytes32(gd3_head.constData(), static_cast<size_t>(gd3_head.size()));

  file.seek(SAMPLE_COUNT);
  QByteArray sample_count_bytes = file.read(4);
  file.seek(LOOP_SAMPLE_COUNT);
  QByteArray loop_count_bytes = file.read(4);
  quint64 length = 0;

  if (!GetPlaybackLength(sample_count_bytes, loop_count_bytes, length)) {
    return TagReaderResult::ErrorCode::FileParseError;
  }

  file.seek(static_cast<qint64>(GD3_TAG_PTR + pt));
  QByteArray gd3_version = file.read(4);
  Q_UNUSED(gd3_version)

  file.seek(file.pos() + 4);
  QByteArray gd3_length_bytes = file.read(4);
  quint32 gd3_length = GME::UnpackBytes32(gd3_length_bytes.constData(), static_cast<size_t>(gd3_length_bytes.size()));

  QByteArray gd3Data = file.read(gd3_length);
  QTextStream fileTagStream(gd3Data, QIODevice::ReadOnly);
  // Stored as 16 bit UTF string, two bytes per letter.
  fileTagStream.setEncoding(QStringConverter::Utf16);
  QStringList strings = fileTagStream.readLine(0).split(u'\0');
  if (strings.count() < 10) {
    return TagReaderResult::ErrorCode::FileParseError;
  }

  // VGM standard dictates string tag data exist in specific order.
  // Order alternates between English and Japanese version of data.
  // Read GD3 tag standard for more details.
  song->set_title(strings[0].toStdString());
  song->set_album(strings[2].toStdString());
  song->set_artist(strings[6].toStdString());
  song->set_year(strings[8].left(4).toInt());
  song->set_length_nanosec(static_cast<qint64>(length * kNsecPerMsec));
  song->set_valid(true);
  song->set_filetype(Song::FileType::VGM);

  return TagReaderResult::ErrorCode::Success;

}

bool GME::VGM::GetPlaybackLength(const QByteArray &sample_count_bytes, const QByteArray &loop_count_bytes, quint64 &out_length) {

  if (sample_count_bytes.size() != 4) return false;
  if (loop_count_bytes.size() != 4) return false;

  quint64 sample_count = GME::UnpackBytes32(sample_count_bytes.constData(), static_cast<size_t>(sample_count_bytes.size()));

  if (sample_count == 0) return false;

  quint64 loop_sample_count = GME::UnpackBytes32(loop_count_bytes.constData(), static_cast<size_t>(loop_count_bytes.size()));

  if (loop_sample_count == 0) {
    out_length = sample_count * 1000 / SAMPLE_TIMEBASE;
    return true;
  }

  quint64 intro_length_ms = (sample_count - loop_sample_count) * 1000 / SAMPLE_TIMEBASE;
  quint64 loop_length_ms = (loop_sample_count) * 1000 / SAMPLE_TIMEBASE;
  out_length = intro_length_ms + (loop_length_ms * 2) + GST_GME_LOOP_TIME_MS;

  return true;

}

TagReaderGME::TagReaderGME() = default;


TagReaderResult TagReaderGME::IsMediaFile(const QString &filename) const {

  QFileInfo fileinfo(filename);
  return GME::IsSupportedFormat(fileinfo) ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::Unsupported;

}

TagReaderResult TagReaderGME::ReadFile(const QString &filename, Song *song) const {

  QFileInfo fileinfo(filename);
  return GME::ReadFile(fileinfo, song);

}

#ifdef HAVE_STREAMTAGREADER
TagReaderResult TagReaderGME::ReadStream(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song) const {

  Q_UNUSED(url);
  Q_UNUSED(filename);
  Q_UNUSED(size);
  Q_UNUSED(mtime);
  Q_UNUSED(token_type);
  Q_UNUSED(access_token);
  Q_UNUSED(song);

  return TagReaderResult::ErrorCode::Unsupported;

}
#endif

TagReaderResult TagReaderGME::WriteFile(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) const {

  Q_UNUSED(filename);
  Q_UNUSED(song);
  Q_UNUSED(save_tags_options);
  Q_UNUSED(save_tag_cover_data);

  return TagReaderResult::ErrorCode::Unsupported;

}

TagReaderResult TagReaderGME::LoadEmbeddedCover(const QString &filename, QByteArray &data) const {

  Q_UNUSED(filename);
  Q_UNUSED(data);

  return TagReaderResult::ErrorCode::Unsupported;

}

TagReaderResult TagReaderGME::SaveEmbeddedCover(const QString &filename, const SaveTagCoverData &save_tag_cover_data) const {

  Q_UNUSED(filename);
  Q_UNUSED(save_tag_cover_data);

  return TagReaderResult::ErrorCode::Unsupported;

}

TagReaderResult TagReaderGME::SaveSongPlaycount(const QString &filename, const uint playcount) const {

  Q_UNUSED(filename);
  Q_UNUSED(playcount);

  return TagReaderResult::ErrorCode::Unsupported;

}

TagReaderResult TagReaderGME::SaveSongRating(const QString &filename, const float rating) const {

  Q_UNUSED(filename);
  Q_UNUSED(rating);

  return TagReaderResult::ErrorCode::Unsupported;

}
