/* This file is part of Strawberry.
   Copyright 2013, David Sansome <me@davidsansome.com>
   Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TAGREADERTAGLIB_H
#define TAGREADERTAGLIB_H

#include "config.h"

#include <string>

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

#include "tagreaderbase.h"
#include "tagreadermessages.pb.h"

class FileRefFactory;

/*
 * This class holds all useful methods to read and write tags from/to files.
 * You should not use it directly in the main process but rather use a TagReaderWorker process (using TagReaderClient)
 */
class TagReaderTagLib : public TagReaderBase {
 public:
  explicit TagReaderTagLib();
  ~TagReaderTagLib();

  bool IsMediaFile(const QString &filename) const override;

  Result ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const override;
  Result WriteFile(const QString &filename, const spb::tagreader::WriteFileRequest &request) const override;

  Result LoadEmbeddedArt(const QString &filename, QByteArray &data) const;
  Result SaveEmbeddedArt(const QString &filename, const spb::tagreader::SaveEmbeddedArtRequest &request) const override;

  Result SaveSongPlaycountToFile(const QString &filename, const uint playcount) const override;
  Result SaveSongRatingToFile(const QString &filename, const float rating) const override;

  static void TStringToStdString(const TagLib::String &tag, std::string *output);

 private:
  spb::tagreader::SongMetadata_FileType GuessFileType(TagLib::FileRef *fileref) const;

  void ParseID3v2Tag(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseOggTag(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseAPETag(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;

  void SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const spb::tagreader::SongMetadata &song) const;
  void SaveID3v2Tag(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SaveAPETag(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const;

  void SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const;
  void SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetUserTextFrame(const std::string &description, const std::string &value, TagLib::ID3v2::Tag *tag) const;
  void SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const;

  QByteArray LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const;

  static TagLib::ID3v2::PopularimeterFrame *GetPOPMFrameFromTag(TagLib::ID3v2::Tag *tag);

  void SetPlaycount(TagLib::Ogg::XiphComment *xiph_comment, const uint playcount) const;
  void SetPlaycount(TagLib::APE::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::ID3v2::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::MP4::Tag *tag, const uint playcount) const;
  void SetPlaycount(TagLib::ASF::Tag *tag, const uint playcount) const;

  void SetRating(TagLib::Ogg::XiphComment *xiph_comment, const float rating) const;
  void SetRating(TagLib::APE::Tag *tag, const float rating) const;
  void SetRating(TagLib::ID3v2::Tag *tag, const float rating) const;
  void SetRating(TagLib::MP4::Tag *tag, const float rating) const;
  void SetRating(TagLib::ASF::Tag *tag, const float rating) const;

  void SetEmbeddedArt(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mime_type) const;

 private:
  FileRefFactory *factory_;

  Q_DISABLE_COPY(TagReaderTagLib)
};

#endif  // TAGREADERTAGLIB_H
