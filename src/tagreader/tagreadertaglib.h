/*
 * Strawberry Music Player
 * Copyright 2013, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERTAGLIB_H
#define TAGREADERTAGLIB_H

#include "config.h"

#include <QByteArray>
#include <QString>

#include <taglib/tstring.h>
#include <taglib/fileref.h>
#include <taglib/xiphcomment.h>
#include <taglib/flacfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>
#include <taglib/apetag.h>
#include <taglib/apefile.h>
#include <taglib/asffile.h>
#include <taglib/id3v2tag.h>
#include <taglib/popularimeterframe.h>
#include <taglib/mp4tag.h>
#include <taglib/asftag.h>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"

#include "tagreaderbase.h"
#include "savetagcoverdata.h"

#undef TStringToQString
#undef QStringToTString

class FileRefFactory;

class TagReaderTagLib : public TagReaderBase {
 public:
  explicit TagReaderTagLib();
  ~TagReaderTagLib() override;

  static inline TagLib::String QStringToTagLibString(const QString &s) {
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
  }

  static inline QString TagLibStringToQString(const TagLib::String &s) {
    return QString::fromUtf8((s).toCString(true));
  }

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

 private:
  static Song::FileType GuessFileType(TagLib::FileRef *fileref);
  TagReaderResult Read(SharedPtr<TagLib::FileRef> fileref, Song *song) const;

  void ParseID3v2Tags(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, Song *song) const;
  void ParseVorbisComments(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, Song *song) const;
  void ParseAPETags(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, Song *song) const;
  void ParseMP4Tags(TagLib::MP4::Tag *tag, QString *disc, QString *compilation, Song *song) const;
  void ParseASFTags(TagLib::ASF::Tag *tag, QString *disc, QString *compilation, Song *song) const;
  void ParseASFAttribute(const TagLib::ASF::AttributeListMap &attributes_map, const char *attribute, QString *str) const;

  void SetID3v2Tag(TagLib::ID3v2::Tag *tag, const Song &song) const;
  void SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetUnsyncLyricsFrame(const QString &value, TagLib::ID3v2::Tag *tag) const;

  void SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const Song &song) const;
  void SetAPETag(TagLib::APE::Tag *tag, const Song &song) const;
  void SetASFTag(TagLib::ASF::Tag *tag, const Song &song) const;
  void SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const QString &value) const;
  void SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const int value) const;

  QByteArray LoadEmbeddedCover(TagLib::ID3v2::Tag *tag) const;
  QByteArray LoadEmbeddedCover(TagLib::APE::Tag *tag) const;

  static TagLib::ID3v2::PopularimeterFrame *GetPOPMFrameFromTag(TagLib::ID3v2::Tag *tag);

  void SetPlaycount(TagLib::Ogg::XiphComment *vorbis_comment, const uint playcount) const;
  void SetPlaycount(TagLib::APE::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::ID3v2::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::MP4::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::ASF::Tag *tag, const uint playcount) const;

  void SetRating(TagLib::Ogg::XiphComment *vorbis_comment, const float rating) const;
  void SetRating(TagLib::APE::Tag *tag, const float rating) const;
  void SetRating(TagLib::ID3v2::Tag *tag, const float rating) const;
  void SetRating(TagLib::MP4::Tag *tag, const float rating) const;
  void SetRating(TagLib::ASF::Tag *tag, const float rating) const;

  void SetEmbeddedCover(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mimetype) const;
  void SetEmbeddedCover(TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mimetype) const;
  void SetEmbeddedCover(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mimetype) const;
  void SetEmbeddedCover(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mimetype) const;

  static TagLib::String TagLibStringListToSlashSeparatedString(const TagLib::StringList &taglib_string_list, const uint begin_index = 0);

 private:
  FileRefFactory *factory_;

  Q_DISABLE_COPY(TagReaderTagLib)
};

#endif  // TAGREADERTAGLIB_H
