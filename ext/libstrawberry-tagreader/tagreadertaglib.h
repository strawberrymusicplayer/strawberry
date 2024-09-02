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
#include <taglib/mp4tag.h>
#include <taglib/asftag.h>

#include "tagreaderbase.h"
#include "tagreadermessages.pb.h"

#undef TStringToQString
#undef QStringToTString

class FileRefFactory;

/*
 * This class holds all useful methods to read and write tags from/to files.
 * You should not use it directly in the main process but rather use a TagReaderWorker process (using TagReaderClient)
 */
class TagReaderTagLib : public TagReaderBase {
 public:
  explicit TagReaderTagLib();
  ~TagReaderTagLib() override;

  static inline TagLib::String StdStringToTagLibString(const std::string &s) {
    return TagLib::String(s.c_str(), TagLib::String::UTF8);
  }

  static inline std::string TagLibStringToStdString(const TagLib::String &s) {
    return std::string(s.toCString(true), s.length());
  }

  static inline TagLib::String QStringToTagLibString(const QString &s) {
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
  }

  static inline QString TagLibStringToQString(const TagLib::String &s) {
    return QString::fromUtf8((s).toCString(true));
  }

  static inline void AssignTagLibStringToStdString(const TagLib::String &tstr, std::string *output) {

    const QString qstr = TagLibStringToQString(tstr).trimmed();
    const QByteArray data = qstr.toUtf8();
    output->assign(data.constData(), data.size());

  }

  bool IsMediaFile(const QString &filename) const override;

  Result ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const override;
  Result WriteFile(const QString &filename, const spb::tagreader::WriteFileRequest &request) const override;

  Result LoadEmbeddedArt(const QString &filename, QByteArray &data) const override;
  Result SaveEmbeddedArt(const QString &filename, const spb::tagreader::SaveEmbeddedArtRequest &request) const override;

  Result SaveSongPlaycountToFile(const QString &filename, const uint playcount) const override;
  Result SaveSongRatingToFile(const QString &filename, const float rating) const override;

 private:
  spb::tagreader::SongMetadata_FileType GuessFileType(TagLib::FileRef *fileref) const;

  void ParseID3v2Tags(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseVorbisComments(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseAPETags(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseMP4Tags(TagLib::MP4::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseASFTags(TagLib::ASF::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const;
  void ParseASFAttribute(const TagLib::ASF::AttributeListMap &attributes_map, const char *attribute, std::string *str) const;

  void SetID3v2Tag(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const;
  void SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const;
  void SetUserTextFrame(const std::string &description, const std::string &value, TagLib::ID3v2::Tag *tag) const;
  void SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const;

  void SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const spb::tagreader::SongMetadata &song) const;
  void SetAPETag(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SetASFTag(TagLib::ASF::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const std::string &value) const;
  void SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const int value) const;

  QByteArray LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const;

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

  void SetEmbeddedArt(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mime_type) const;
  void SetEmbeddedArt(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mime_type) const;

  static TagLib::String TagLibStringListToSlashSeparatedString(const TagLib::StringList &taglib_string_list);

 private:
  FileRefFactory *factory_;

  Q_DISABLE_COPY(TagReaderTagLib)
};

#endif  // TAGREADERTAGLIB_H
