/* This file is part of Strawberry.
   Copyright 2013, David Sansome <me@davidsansome.com>
   Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>

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

#include "config.h"

#include "tagreader.h"

#include <memory>
#include <list>
#include <map>
#include <utility>
#include <sys/stat.h>

#include <taglib/taglib.h>
#include <taglib/taglib_config.h>
#include <taglib/fileref.h>
#include <taglib/tbytevector.h>
#include <taglib/tfile.h>
#include <taglib/tlist.h>
#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/audioproperties.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/xiphcomment.h>
#include <taglib/commentsframe.h>
#include <taglib/tag.h>
#include <taglib/apetag.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/flacfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/flacproperties.h>
#include <taglib/flacpicture.h>
#include <taglib/vorbisfile.h>
#include <taglib/speexfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#include <taglib/aifffile.h>
#include <taglib/asffile.h>
#include <taglib/asftag.h>
#include <taglib/asfattribute.h>
#include <taglib/asfproperties.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4properties.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/apefile.h>
#ifdef HAVE_TAGLIB_DSFFILE
#  include <taglib/dsffile.h>
#  include <taglib/dsdifffile.h>
#endif

#include <QtGlobal>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QByteArray>
#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QTextCodec>
#include <QVector>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"

#include "fmpsparser.h"
#include "core/timeconstants.h"

#ifndef USE_SYSTEM_TAGLIB
using namespace Strawberry_TagLib;
#endif

class FileRefFactory {
 public:
  virtual ~FileRefFactory() {}
  virtual TagLib::FileRef *GetFileRef(const QString &filename) = 0;
};

class TagLibFileRefFactory : public FileRefFactory {
 public:
  virtual TagLib::FileRef *GetFileRef(const QString &filename) {
#ifdef Q_OS_WIN32
    return new TagLib::FileRef(filename.toStdWString().c_str());
#else
    return new TagLib::FileRef(QFile::encodeName(filename).constData());
#endif
  }
};

namespace {

TagLib::String StdStringToTaglibString(const std::string &s) {
  return TagLib::String(s.c_str(), TagLib::String::UTF8);
}

TagLib::String QStringToTaglibString(const QString &s) {
  return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
}

}

namespace {
// Tags containing the year the album was originally released (in contrast to other tags that contain the release year of the current edition)
const char *kMP4_OriginalYear_ID = "----:com.apple.iTunes:ORIGINAL YEAR";
const char *kASF_OriginalDate_ID = "WM/OriginalReleaseTime";
const char *kASF_OriginalYear_ID = "WM/OriginalReleaseYear";
}


TagReader::TagReader() :
  factory_(new TagLibFileRefFactory),
  kEmbeddedCover("(embedded)") {
}

TagReader::~TagReader() {
  delete factory_;
}

pb::tagreader::SongMetadata_FileType TagReader::GuessFileType(TagLib::FileRef *fileref) const {

  if (dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_WAV;
  if (dynamic_cast<TagLib::FLAC::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_FLAC;
  if (dynamic_cast<TagLib::WavPack::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_WAVPACK;
  if (dynamic_cast<TagLib::Ogg::FLAC::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_OGGFLAC;
  if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_OGGVORBIS;
  if (dynamic_cast<TagLib::Ogg::Opus::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_OGGOPUS;
  if (dynamic_cast<TagLib::Ogg::Speex::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_OGGSPEEX;
  if (dynamic_cast<TagLib::MPEG::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_MPEG;
  if (dynamic_cast<TagLib::MP4::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_MP4;
  if (dynamic_cast<TagLib::ASF::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_ASF;
  if (dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_AIFF;
  if (dynamic_cast<TagLib::MPC::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_MPC;
  if (dynamic_cast<TagLib::TrueAudio::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_TRUEAUDIO;
  if (dynamic_cast<TagLib::APE::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_APE;
#ifdef HAVE_TAGLIB_DSFFILE
  if (dynamic_cast<TagLib::DSF::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_DSF;
  if (dynamic_cast<TagLib::DSDIFF::File*>(fileref->file())) return pb::tagreader::SongMetadata_FileType_DSDIFF;
#endif

  return pb::tagreader::SongMetadata_FileType_UNKNOWN;

}

void TagReader::ReadFile(const QString &filename, pb::tagreader::SongMetadata *song) const {

  const QByteArray url(QUrl::fromLocalFile(filename).toEncoded());
  const QFileInfo info(filename);

  qLog(Debug) << "Reading tags from" << filename;

  song->set_basefilename(DataCommaSizeFromQString(info.fileName()));
  song->set_url(url.constData(), url.size());
  song->set_filesize(info.size());
  song->set_mtime(info.lastModified().toTime_t());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
  song->set_ctime(info.birthTime().isValid() ? info.birthTime().toTime_t() : info.lastModified().toTime_t());
#else
  song->set_ctime(info.created().toTime_t());
#endif

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (fileref->isNull()) {
    qLog(Info) << "TagLib hasn't been able to read" << filename << "file";
    return;
  }

  song->set_filetype(GuessFileType(fileref.get()));

  if (fileref->audioProperties()) {
    song->set_bitrate(fileref->audioProperties()->bitrate());
    song->set_samplerate(fileref->audioProperties()->sampleRate());
    song->set_length_nanosec(fileref->audioProperties()->length() * kNsecPerSec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    Decode(tag->title(), nullptr, song->mutable_title());
    Decode(tag->artist(), nullptr, song->mutable_artist());  // TPE1
    Decode(tag->album(), nullptr, song->mutable_album());
    Decode(tag->genre(), nullptr, song->mutable_genre());
    song->set_year(tag->year());
    song->set_track(tag->track());
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *tag = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseOggTag(tag->fieldListMap(), nullptr, &disc, &compilation, song);
    if (!tag->pictureList().isEmpty()) {
      song->set_art_automatic(kEmbeddedCover);
    }
  }

  if (TagLib::FLAC::File *file = dynamic_cast<TagLib::FLAC::File *>(fileref->file())) {

    song->set_bitdepth(file->audioProperties()->bitsPerSample());

    if (file->xiphComment()) {
      ParseOggTag(file->xiphComment()->fieldListMap(), nullptr, &disc, &compilation, song);
      if (!file->pictureList().isEmpty()) {
        song->set_art_automatic(kEmbeddedCover);
      }
    }
    if (tag) Decode(tag->comment(), nullptr, song->mutable_comment());
  }

  else if (TagLib::WavPack::File *file = dynamic_cast<TagLib::WavPack::File *>(fileref->file())) {
    song->set_bitdepth(file->audioProperties()->bitsPerSample());
    if (file->tag()) {
      ParseAPETag(file->APETag()->itemListMap(), nullptr, &disc, &compilation, song);
    }
    if (tag) Decode(tag->comment(), nullptr, song->mutable_comment());
  }

  else if (TagLib::APE::File *file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (file->tag()) {
      ParseAPETag(file->APETag()->itemListMap(), nullptr, &disc, &compilation, song);
    }
    song->set_bitdepth(file->audioProperties()->bitsPerSample());
    if (tag) Decode(tag->comment(), nullptr, song->mutable_comment());
  }

  else if (TagLib::MPEG::File *file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {

    if (file->ID3v2Tag()) {
      const TagLib::ID3v2::FrameListMap &map = file->ID3v2Tag()->frameListMap();

      if (!map["TPOS"].isEmpty()) disc = TStringToQString(map["TPOS"].front()->toString()).trimmed();
      if (!map["TCOM"].isEmpty()) Decode(map["TCOM"].front()->toString(), nullptr, song->mutable_composer());

      // content group
      if (!map["TIT1"].isEmpty()) Decode(map["TIT1"].front()->toString(), nullptr, song->mutable_grouping());

      // ID3v2: lead performer/soloist
      if (!map["TPE1"].isEmpty()) Decode(map["TPE1"].front()->toString(), nullptr, song->mutable_performer());

      // original artist/performer
      if (!map["TOPE"].isEmpty()) Decode(map["TOPE"].front()->toString(), nullptr, song->mutable_performer());

      // Skip TPE1 (which is the artist) here because we already fetched it


      // non-standard: Apple, Microsoft
      if (!map["TPE2"].isEmpty()) Decode(map["TPE2"].front()->toString(), nullptr, song->mutable_albumartist());

      if (!map["TCMP"].isEmpty()) compilation = TStringToQString(map["TCMP"].front()->toString()).trimmed();

      if (!map["TDOR"].isEmpty()) { song->set_originalyear(map["TDOR"].front()->toString().substr(0, 4).toInt()); }
      else if (!map["TORY"].isEmpty()) {
        song->set_originalyear(map["TORY"].front()->toString().substr(0, 4).toInt());
      }

      if (!map["USLT"].isEmpty()) {
        Decode(map["USLT"].front()->toString(), nullptr, song->mutable_lyrics());
      }
      else if (!map["SYLT"].isEmpty()) {
        Decode(map["SYLT"].front()->toString(), nullptr, song->mutable_lyrics());
      }

      if (!map["APIC"].isEmpty()) song->set_art_automatic(kEmbeddedCover);

      // Find a suitable comment tag.  For now we ignore iTunNORM comments.
      for (int i = 0; i < map["COMM"].size(); ++i) {
        const TagLib::ID3v2::CommentsFrame *frame = dynamic_cast<const TagLib::ID3v2::CommentsFrame*>(map["COMM"][i]);

        if (frame && TStringToQString(frame->description()) != "iTunNORM") {
          Decode(frame->text(), nullptr, song->mutable_comment());
          break;
        }
      }

      // Parse FMPS frames
      for (int i = 0; i < map["TXXX"].size(); ++i) {
        const TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<const TagLib::ID3v2::UserTextIdentificationFrame*>(map["TXXX"][i]);

        if (frame && frame->description().startsWith("FMPS_")) {
          ParseFMPSFrame(TStringToQString(frame->description()), TStringToQString(frame->fieldList()[1]), song);
        }
      }

    }
  }

  else if (TagLib::MP4::File *file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {

    song->set_bitdepth(file->audioProperties()->bitsPerSample());

    if (file->tag()) {
      TagLib::MP4::Tag *mp4_tag = file->tag();

      // Find album artists
      if (mp4_tag->item("aART").isValid()) {
        TagLib::StringList album_artists = mp4_tag->item("aART").toStringList();
        if (!album_artists.isEmpty()) {
          Decode(album_artists.front(), nullptr, song->mutable_albumartist());
        }
      }

      // Find album cover art
      if (mp4_tag->item("covr").isValid()) {
        song->set_art_automatic(kEmbeddedCover);
      }

      if (mp4_tag->item("disk").isValid()) {
        disc = TStringToQString(TagLib::String::number(mp4_tag->item("disk").toIntPair().first));
      }

      if (mp4_tag->item("\251wrt").isValid()) {
        Decode(mp4_tag->item("\251wrt").toStringList().toString(", "), nullptr, song->mutable_composer());
      }
      if (mp4_tag->item("\251grp").isValid()) {
        Decode(mp4_tag->item("\251grp").toStringList().toString(" "), nullptr, song->mutable_grouping());
      }

      if (mp4_tag->item(kMP4_OriginalYear_ID).isValid()) {
        song->set_originalyear(TStringToQString(mp4_tag->item(kMP4_OriginalYear_ID).toStringList().toString('\n')).left(4).toInt());
      }

      Decode(mp4_tag->comment(), nullptr, song->mutable_comment());
    }
  }

  else if (TagLib::ASF::File *file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {

    song->set_bitdepth(file->audioProperties()->bitsPerSample());

    const TagLib::ASF::AttributeListMap &attributes_map = file->tag()->attributeListMap();

    if (attributes_map.contains(kASF_OriginalDate_ID)) {
      const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalDate_ID];
      if (!attributes.isEmpty()) {
        song->set_originalyear(TStringToQString(attributes.front().toString()).left(4).toInt());
      }
    }
    else if (attributes_map.contains(kASF_OriginalYear_ID)) {
      const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalYear_ID];
      if (!attributes.isEmpty()) {
        song->set_originalyear(TStringToQString(attributes.front().toString()).left(4).toInt());
      }
    }
  }

  else if (TagLib::MPC::File* file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file->tag()) {
      ParseAPETag(file->APETag()->itemListMap(), nullptr, &disc, &compilation, song);
    }
    if (tag) Decode(tag->comment(), nullptr, song->mutable_comment());
  }

  else if (tag) {
    Decode(tag->comment(), nullptr, song->mutable_comment());
  }

  if (!disc.isEmpty()) {
    const int i = disc.indexOf('/');
    if (i != -1) {
      // disc.right( i ).toInt() is total number of discs, we don't use this at the moment
      song->set_disc(disc.left(i).toInt());
    }
    else {
      song->set_disc(disc.toInt());
    }
  }

  if (compilation.isEmpty()) {
    // well, it wasn't set, but if the artist is VA assume it's a compilation
    if (QStringFromStdString(song->artist()).toLower() == "various artists") {
      song->set_compilation(true);
    }
  }
  else {
    song->set_compilation(compilation.toInt() == 1);
  }

  if (!lyrics.isEmpty()) song->set_lyrics(lyrics.toStdString());

  // Set integer fields to -1 if they're not valid
  #define SetDefault(field) if (song->field() <= 0) { song->set_##field(-1); }
  SetDefault(track);
  SetDefault(disc);
  SetDefault(year);
  SetDefault(bitrate);
  SetDefault(samplerate);
  SetDefault(bitdepth);
  SetDefault(lastplayed);
  #undef SetDefault

}

void TagReader::Decode(const TagLib::String &tag, const QTextCodec *codec, std::string *output) {

  QString tmp;

  if (codec && tag.isLatin1()) {  // Never override UTF-8.
    const std::string fixed = QString::fromUtf8(tag.toCString(true)).toStdString();
    tmp = codec->toUnicode(fixed.c_str()).trimmed();
  }
  else {
    tmp = TStringToQString(tag).trimmed();
  }

  output->assign(DataCommaSizeFromQString(tmp));

}

void TagReader::Decode(const QString &tag, const QTextCodec *codec, std::string *output) {

  if (!codec) {
    output->assign(DataCommaSizeFromQString(tag));
  }
  else {
    const QString decoded(codec->toUnicode(tag.toUtf8()));
    output->assign(DataCommaSizeFromQString(decoded));
  }

}

void TagReader::ParseOggTag(const TagLib::Ogg::FieldListMap &map, const QTextCodec *codec, QString *disc, QString *compilation, pb::tagreader::SongMetadata *song) const {

  if (!map["COMPOSER"].isEmpty()) Decode(map["COMPOSER"].front(), codec, song->mutable_composer());
  if (!map["PERFORMER"].isEmpty()) Decode(map["PERFORMER"].front(), codec, song->mutable_performer());
  if (!map["CONTENT GROUP"].isEmpty()) Decode(map["CONTENT GROUP"].front(), codec, song->mutable_grouping());

  if (!map["ALBUMARTIST"].isEmpty()) Decode(map["ALBUMARTIST"].front(), codec, song->mutable_albumartist());
  else if (!map["ALBUM ARTIST"].isEmpty()) Decode(map["ALBUM ARTIST"].front(), codec, song->mutable_albumartist());

  if (!map["ORIGINALDATE"].isEmpty()) song->set_originalyear(TStringToQString(map["ORIGINALDATE"].front()).left(4).toInt());
  else if (!map["ORIGINALYEAR"].isEmpty()) song->set_originalyear(TStringToQString(map["ORIGINALYEAR"].front()).toInt());

  if (!map["DISCNUMBER"].isEmpty()) *disc = TStringToQString( map["DISCNUMBER"].front() ).trimmed();
  if (!map["COMPILATION"].isEmpty()) *compilation = TStringToQString( map["COMPILATION"].front() ).trimmed();
  if (!map["COVERART"].isEmpty()) song->set_art_automatic(kEmbeddedCover);
  if (!map["METADATA_BLOCK_PICTURE"].isEmpty()) song->set_art_automatic(kEmbeddedCover);

  if (!map["FMPS_PLAYCOUNT"].isEmpty() && song->playcount() <= 0) song->set_playcount(TStringToQString( map["FMPS_PLAYCOUNT"].front() ).trimmed().toFloat());

  if (!map["LYRICS"].isEmpty()) Decode(map["LYRICS"].front(), codec, song->mutable_lyrics());
  else if (!map["UNSYNCEDLYRICS"].isEmpty()) Decode(map["UNSYNCEDLYRICS"].front(), codec, song->mutable_lyrics());

}

void TagReader::ParseAPETag(const TagLib::APE::ItemListMap &map, const QTextCodec *codec, QString *disc, QString *compilation, pb::tagreader::SongMetadata *song) const {

  Q_UNUSED(codec);

  TagLib::APE::ItemListMap::ConstIterator it = map.find("ALBUM ARTIST");
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.toStringList();
    if (!album_artists.isEmpty()) {
      Decode(album_artists.front(), nullptr, song->mutable_albumartist());
    }
  }

  if (map.find("COVER ART (FRONT)") != map.end()) song->set_art_automatic(kEmbeddedCover);
  if (map.contains("COMPILATION")) {
    *compilation = TStringToQString(TagLib::String::number(map["COMPILATION"].toString().toInt()));
  }

  if (map.contains("DISC")) {
    *disc = TStringToQString(TagLib::String::number(map["DISC"].toString().toInt()));
  }

  if (map.contains("PERFORMER")) {
    Decode(map["PERFORMER"].toStringList().toString(", "), nullptr, song->mutable_performer());
  }

  if (map.contains("COMPOSER")) {
    Decode(map["COMPOSER"].toStringList().toString(", "), nullptr, song->mutable_composer());
  }

  if (map.contains("GROUPING")) {
    Decode(map["GROUPING"].toStringList().toString(" "), nullptr, song->mutable_grouping());
  }

  if (map.contains("LYRICS")) {
    Decode(map["LYRICS"].toString(), nullptr, song->mutable_lyrics());
  }

  if (map.contains("FMPS_PLAYCOUNT")) {
    int playcount = TStringToQString(map["FMPS_PLAYCOUNT"].toString()).toFloat();
    if (song->playcount() <= 0 && playcount > 0) {
      song->set_playcount(playcount);
    }
  }

}

void TagReader::ParseFMPSFrame(const QString &name, const QString &value, pb::tagreader::SongMetadata *song) const {

  qLog(Debug) << "Parsing FMPSFrame" << name << ", " << value;
  FMPSParser parser;

  if (!parser.Parse(value) || parser.is_empty()) return;

  QVariant var;

  if (name == "FMPS_PlayCount") {
    var = parser.result()[0][0];
    if (var.type() == QVariant::Double) {
      song->set_playcount(var.toDouble());
    }
  }
  else if (name == "FMPS_PlayCount_User") {
    // Take a user playcount only if there's no playcount already set
    if (song->playcount() == 0 && parser.result()[0].count() >= 2) {
      var = parser.result()[0][1];
      if (var.type() == QVariant::Double) {
        song->set_playcount(var.toDouble());
      }
    }
  }

}

void TagReader::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comments, const pb::tagreader::SongMetadata &song) const {

  vorbis_comments->addField("COMPOSER", StdStringToTaglibString(song.composer()), true);
  vorbis_comments->addField("PERFORMER", StdStringToTaglibString(song.performer()), true);
  vorbis_comments->addField("CONTENT GROUP", StdStringToTaglibString(song.grouping()), true);
  vorbis_comments->addField("DISCNUMBER", QStringToTaglibString(song.disc() <= 0 -1 ? QString() : QString::number(song.disc())), true);
  vorbis_comments->addField("COMPILATION", StdStringToTaglibString(song.compilation() ? "1" : "0"), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comments->addField("ALBUMARTIST", StdStringToTaglibString(song.albumartist()), true);
  vorbis_comments->removeFields("ALBUM ARTIST");

  vorbis_comments->addField("LYRICS", StdStringToTaglibString(song.lyrics()), true);
  vorbis_comments->removeFields("UNSYNCEDLYRICS");

}

bool TagReader::SaveFile(const QString &filename, const pb::tagreader::SongMetadata &song) const {

  if (filename.isNull() || filename.isEmpty()) return false;
  qLog(Debug) << "Saving tags to" << filename;
  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));;
  if (!fileref || fileref->isNull()) return false;

  fileref->tag()->setTitle(StdStringToTaglibString(song.title()));
  fileref->tag()->setArtist(StdStringToTaglibString(song.artist()));
  fileref->tag()->setAlbum(StdStringToTaglibString(song.album()));
  fileref->tag()->setGenre(StdStringToTaglibString(song.genre()));
  fileref->tag()->setComment(StdStringToTaglibString(song.comment()));
  fileref->tag()->setYear(song.year());
  fileref->tag()->setTrack(song.track());

  if (TagLib::FLAC::File *file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *tag = file->xiphComment();
    SetVorbisComments(tag, song);
  }

  else if (TagLib::WavPack::File *file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::APE::File *file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::MPC::File *file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::MPEG::File *file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file->ID3v2Tag(true);
    if (!tag) return false;
    SetTextFrame("TPOS", song.disc() <= 0 -1 ? QString() : QString::number(song.disc()), tag);
    SetTextFrame("TCOM", song.composer(), tag);
    SetTextFrame("TIT1", song.grouping(), tag);
    SetTextFrame("TOPE", song.performer(), tag);
    // Skip TPE1 (which is the artist) here because we already set it
    SetTextFrame("TPE2", song.albumartist(), tag);
    SetTextFrame("TCMP", std::string(song.compilation() ? "1" : "0"), tag);
    SetUnsyncLyricsFrame(song.lyrics(), tag);
  }

  else if (TagLib::MP4::File *file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file->tag();
    tag->setItem("disk", TagLib::MP4::Item(song.disc() <= 0 -1 ? 0 : song.disc(), 0));
    tag->setItem("\251wrt", TagLib::StringList(song.composer().c_str()));
    tag->setItem("\251grp", TagLib::StringList(song.grouping().c_str()));
    tag->setItem("aART", TagLib::StringList(song.albumartist().c_str()));
    tag->setItem("cpil", TagLib::StringList(song.compilation() ? "1" : "0"));
  }

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block above.
  if (TagLib::Ogg::XiphComment *tag = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetVorbisComments(tag, song);
  }

  bool ret = fileref->save();
#ifdef Q_OS_LINUX
  if (ret) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return ret;
}

void TagReader::SaveAPETag(TagLib::APE::Tag *tag, const pb::tagreader::SongMetadata &song) const {

  tag->setItem("album artist", TagLib::APE::Item("album artist", TagLib::StringList(song.albumartist().c_str())));
  tag->setItem("disc", TagLib::APE::Item("disc", TagLib::String::number(song.disc() <= 0 - 1 ? 0 : song.disc())));
  tag->setItem("composer", TagLib::APE::Item("composer", TagLib::StringList(song.composer().c_str())));
  tag->setItem("grouping", TagLib::APE::Item("grouping", TagLib::StringList(song.grouping().c_str())));
  tag->setItem("performer", TagLib::APE::Item("performer", TagLib::StringList(song.performer().c_str())));
  tag->setItem("lyrics", TagLib::APE::Item("lyrics", TagLib::String(song.lyrics())));
  tag->setItem("compilation", TagLib::APE::Item("compilation", TagLib::StringList(song.compilation() ? "1" : "0")));

}

void TagReader::SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray descr_utf8(description.toUtf8());
  const QByteArray value_utf8(value.toUtf8());
  qLog(Debug) << "Setting FMPSFrame:" << description << ", " << value;
  SetUserTextFrame(std::string(descr_utf8.constData(), descr_utf8.length()), std::string(value_utf8.constData(), value_utf8.length()), tag);

}

void TagReader::SetUserTextFrame(const std::string &description, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::String t_description = StdStringToTaglibString(description);
  // Remove the frame if it already exists
  TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, t_description);
  if (frame) {
    tag->removeFrame(frame);
  }

  // Create and add a new frame
  frame = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);

  frame->setDescription(t_description);
  frame->setText(StdStringToTaglibString(value));
  tag->addFrame(frame);

}

void TagReader::SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray utf8(value.toUtf8());
  SetTextFrame(id, std::string(utf8.constData(), utf8.length()), tag);
}

void TagReader::SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector(id);
  QVector<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::TextIdentificationFrame frame(id_vector, TagLib::String::UTF8);
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int lyrics_index = 0; lyrics_index < frames_buffer.size(); lyrics_index++) {
    TagLib::ID3v2::TextIdentificationFrame* frame = new TagLib::ID3v2::TextIdentificationFrame(frames_buffer.at(lyrics_index));
    if (lyrics_index == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

bool TagReader::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  return !fileref->isNull() && fileref->tag();

}

QByteArray TagReader::LoadEmbeddedArt(const QString &filename) const {

  if (filename.isEmpty()) return QByteArray();

  qLog(Debug) << "Loading art from" << filename;

#ifdef Q_OS_WIN32
  TagLib::FileRef ref(filename.toStdWString().c_str());
#else
  TagLib::FileRef ref(QFile::encodeName(filename).constData());
#endif

  if (ref.isNull() || !ref.file()) return QByteArray();

  // FLAC
  TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(ref.file());
  if (flac_file && flac_file->xiphComment()) {
    TagLib::List<TagLib::FLAC::Picture*> pics = flac_file->pictureList();
    if (!pics.isEmpty()) {
      // Use the first picture in the file - this could be made cleverer and
      // pick the front cover if it's present.

      std::list<TagLib::FLAC::Picture*>::iterator it = pics.begin();
      TagLib::FLAC::Picture *picture = *it;

      return QByteArray(picture->data().data(), picture->data().size());
    }
  }

  // WavPack

  TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(ref.file());
  if (wavpack_file) {
    return LoadEmbeddedAPEArt(wavpack_file->APETag()->itemListMap());
  }

  // APE

  TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(ref.file());
  if (ape_file) {
    return LoadEmbeddedAPEArt(ape_file->APETag()->itemListMap());
  }

  // MPC

  TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(ref.file());
  if (mpc_file) {
    return LoadEmbeddedAPEArt(mpc_file->APETag()->itemListMap());
  }

  // Ogg Vorbis / Speex
  TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(ref.file()->tag());
  if (xiph_comment) {
    TagLib::Ogg::FieldListMap map = xiph_comment->fieldListMap();

    TagLib::List<TagLib::FLAC::Picture*> pics = xiph_comment->pictureList();
    if (!pics.isEmpty()) {
      for (auto p : pics) {
        if (p->type() == TagLib::FLAC::Picture::FrontCover)
          return QByteArray(p->data().data(), p->data().size());
      }
      // If there was no specific front cover, just take the first picture
      std::list<TagLib::FLAC::Picture*>::iterator it = pics.begin();
      TagLib::FLAC::Picture *picture = *it;

      return QByteArray(picture->data().data(), picture->data().size());
    }

    // Ogg lacks a definitive standard for embedding cover art, but it seems
    // b64 encoding a field called COVERART is the general convention
    if (map.contains("COVERART"))
      return QByteArray::fromBase64(map["COVERART"].toString().toCString());

    return QByteArray();
  }

  // MP3
  TagLib::MPEG::File *file = dynamic_cast<TagLib::MPEG::File*>(ref.file());
  if (file && file->ID3v2Tag()) {
    TagLib::ID3v2::FrameList apic_frames = file->ID3v2Tag()->frameListMap()["APIC"];
    if (apic_frames.isEmpty())
      return QByteArray();

    TagLib::ID3v2::AttachedPictureFrame *pic = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(apic_frames.front());

    return QByteArray((const char*) pic->picture().data(), pic->picture().size());
  }

  // MP4/AAC
  TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(ref.file());
  if (aac_file) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag->item("covr").isValid()) {
      const TagLib::MP4::CoverArtList &art_list = tag->item("covr").toCoverArtList();

      if (!art_list.isEmpty()) {
        // Just take the first one for now
        const TagLib::MP4::CoverArt &art = art_list.front();
        return QByteArray(art.data().data(), art.data().size());
      }
    }
  }

  return QByteArray();

}

QByteArray TagReader::LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const {

  QByteArray ret;

  TagLib::APE::ItemListMap::ConstIterator it = map.find("COVER ART (FRONT)");
  if (it != map.end()) {
    TagLib::ByteVector data = it->second.binaryData();

    int pos = data.find('\0') + 1;
    if ((pos > 0) && (pos < data.size())) {
      ret = QByteArray(data.data() + pos, data.size() - pos);
    }
  }

  return ret;

}

void TagReader::SetUnsyncLyricsFrame(const std::string& value, TagLib::ID3v2::Tag* tag) const {

  TagLib::ByteVector id_vector("USLT");
  QVector<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame frame(TagLib::String::UTF8);
    frame.setDescription("Clementine editor");
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int lyrics_index = 0; lyrics_index < frames_buffer.size(); lyrics_index++) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame* frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(frames_buffer.at(lyrics_index));
    if (lyrics_index == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}
