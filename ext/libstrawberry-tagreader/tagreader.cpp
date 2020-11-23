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
#include <sys/stat.h>

#include <taglib/taglib.h>
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
#include <taglib/apeitem.h>
#include <taglib/apeproperties.h>
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
#include <taglib/wavpackproperties.h>
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
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
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
#include <QVector>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"
#include "core/timeconstants.h"

class FileRefFactory {
 public:
  virtual ~FileRefFactory() {}
  virtual TagLib::FileRef *GetFileRef(const QString &filename) = 0;
};

class TagLibFileRefFactory : public FileRefFactory {
 public:
  TagLib::FileRef *GetFileRef(const QString &filename) override {
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

}  // namespace

namespace {
// Tags containing the year the album was originally released (in contrast to other tags that contain the release year of the current edition)
const char *kMP4_OriginalYear_ID = "----:com.apple.iTunes:ORIGINAL YEAR";
const char *kASF_OriginalDate_ID = "WM/OriginalReleaseTime";
const char *kASF_OriginalYear_ID = "WM/OriginalReleaseYear";
}  // namespace


TagReader::TagReader() :
  factory_(new TagLibFileRefFactory),
  kEmbeddedCover("(embedded)") {
}

TagReader::~TagReader() {
  delete factory_;
}

bool TagReader::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  return !fileref->isNull() && fileref->tag();

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
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
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
  song->set_mtime(info.lastModified().toSecsSinceEpoch());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
  song->set_ctime(info.birthTime().isValid() ? info.birthTime().toSecsSinceEpoch() : info.lastModified().toSecsSinceEpoch());
#else
  song->set_ctime(info.created().toSecsSinceEpoch());
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
    song->set_length_nanosec(fileref->audioProperties()->lengthInMilliseconds() * kNsecPerMsec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    Decode(tag->title(), song->mutable_title());
    Decode(tag->artist(), song->mutable_artist());  // TPE1
    Decode(tag->album(), song->mutable_album());
    Decode(tag->genre(), song->mutable_genre());
    song->set_year(tag->year());
    song->set_track(tag->track());
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *tag_ogg = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseOggTag(tag_ogg->fieldListMap(), &disc, &compilation, song);
    if (!tag_ogg->pictureList().isEmpty()) {
      song->set_art_automatic(kEmbeddedCover);
    }
  }

  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File *>(fileref->file())) {

    song->set_bitdepth(file_flac->audioProperties()->bitsPerSample());

    if (file_flac->xiphComment()) {
      ParseOggTag(file_flac->xiphComment()->fieldListMap(), &disc, &compilation, song);
      if (!file_flac->pictureList().isEmpty()) {
        song->set_art_automatic(kEmbeddedCover);
      }
    }
    if (tag) Decode(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File *>(fileref->file())) {
    song->set_bitdepth(file_wavpack->audioProperties()->bitsPerSample());
    if (file_wavpack->APETag()) {
      ParseAPETag(file_wavpack->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) Decode(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (file_ape->APETag()) {
      ParseAPETag(file_ape->APETag()->itemListMap(), &disc, &compilation, song);
    }
    song->set_bitdepth(file_ape->audioProperties()->bitsPerSample());
    if (tag) Decode(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {

    if (file_mpeg->ID3v2Tag()) {
      const TagLib::ID3v2::FrameListMap &map = file_mpeg->ID3v2Tag()->frameListMap();

      if (!map["TPOS"].isEmpty()) disc = TStringToQString(map["TPOS"].front()->toString()).trimmed();
      if (!map["TCOM"].isEmpty()) Decode(map["TCOM"].front()->toString(), song->mutable_composer());

      // content group
      if (!map["TIT1"].isEmpty()) Decode(map["TIT1"].front()->toString(), song->mutable_grouping());

      // ID3v2: lead performer/soloist
      if (!map["TPE1"].isEmpty()) Decode(map["TPE1"].front()->toString(), song->mutable_performer());

      // original artist/performer
      if (!map["TOPE"].isEmpty()) Decode(map["TOPE"].front()->toString(), song->mutable_performer());

      // Skip TPE1 (which is the artist) here because we already fetched it


      // non-standard: Apple, Microsoft
      if (!map["TPE2"].isEmpty()) Decode(map["TPE2"].front()->toString(), song->mutable_albumartist());

      if (!map["TCMP"].isEmpty()) compilation = TStringToQString(map["TCMP"].front()->toString()).trimmed();

      if (!map["TDOR"].isEmpty()) { song->set_originalyear(map["TDOR"].front()->toString().substr(0, 4).toInt()); }
      else if (!map["TORY"].isEmpty()) {
        song->set_originalyear(map["TORY"].front()->toString().substr(0, 4).toInt());
      }

      if (!map["USLT"].isEmpty()) {
        Decode(map["USLT"].front()->toString(), song->mutable_lyrics());
      }
      else if (!map["SYLT"].isEmpty()) {
        Decode(map["SYLT"].front()->toString(), song->mutable_lyrics());
      }

      if (!map["APIC"].isEmpty()) song->set_art_automatic(kEmbeddedCover);

      // Find a suitable comment tag.  For now we ignore iTunNORM comments.
      for (uint i = 0 ; i < map["COMM"].size() ; ++i) {
        const TagLib::ID3v2::CommentsFrame *frame = dynamic_cast<const TagLib::ID3v2::CommentsFrame*>(map["COMM"][i]);

        if (frame && TStringToQString(frame->description()) != "iTunNORM") {
          Decode(frame->text(), song->mutable_comment());
          break;
        }
      }

    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {

    song->set_bitdepth(file_mp4->audioProperties()->bitsPerSample());

    if (file_mp4->tag()) {
      TagLib::MP4::Tag *mp4_tag = file_mp4->tag();

      // Find album artists
      if (mp4_tag->item("aART").isValid()) {
        TagLib::StringList album_artists = mp4_tag->item("aART").toStringList();
        if (!album_artists.isEmpty()) {
          Decode(album_artists.front(), song->mutable_albumartist());
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
        Decode(mp4_tag->item("\251wrt").toStringList().toString(", "), song->mutable_composer());
      }
      if (mp4_tag->item("\251grp").isValid()) {
        Decode(mp4_tag->item("\251grp").toStringList().toString(" "), song->mutable_grouping());
      }
      if (mp4_tag->item("\251lyr").isValid()) {
        Decode(mp4_tag->item("\251lyr").toStringList().toString(" "), song->mutable_lyrics());
      }

      if (mp4_tag->item(kMP4_OriginalYear_ID).isValid()) {
        song->set_originalyear(TStringToQString(mp4_tag->item(kMP4_OriginalYear_ID).toStringList().toString('\n')).left(4).toInt());
      }

      Decode(mp4_tag->comment(), song->mutable_comment());
    }
  }

  else if (TagLib::ASF::File *file_asf = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {

    song->set_bitdepth(file_asf->audioProperties()->bitsPerSample());

    if (file_asf->tag()) {
      Decode(file_asf->tag()->comment(), song->mutable_comment());
    }

    const TagLib::ASF::AttributeListMap &attributes_map = file_asf->tag()->attributeListMap();

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

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      ParseAPETag(file_mpc->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) Decode(tag->comment(), song->mutable_comment());
  }

  else if (tag) {
    Decode(tag->comment(), song->mutable_comment());
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
    if (QStringFromStdString(song->artist()).toLower() == "various artists" || QStringFromStdString(song->albumartist()).toLower() == "various artists") {
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

void TagReader::Decode(const TagLib::String &tag, std::string *output) {

  QString tmp = TStringToQString(tag).trimmed();
  output->assign(DataCommaSizeFromQString(tmp));

}

void TagReader::Decode(const QString &tag, std::string *output) {

  output->assign(DataCommaSizeFromQString(tag));

}

void TagReader::ParseOggTag(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, pb::tagreader::SongMetadata *song) const {

  if (!map["COMPOSER"].isEmpty()) Decode(map["COMPOSER"].front(), song->mutable_composer());
  if (!map["PERFORMER"].isEmpty()) Decode(map["PERFORMER"].front(), song->mutable_performer());
  if (!map["CONTENT GROUP"].isEmpty()) Decode(map["CONTENT GROUP"].front(), song->mutable_grouping());

  if (!map["ALBUMARTIST"].isEmpty()) Decode(map["ALBUMARTIST"].front(), song->mutable_albumartist());
  else if (!map["ALBUM ARTIST"].isEmpty()) Decode(map["ALBUM ARTIST"].front(), song->mutable_albumartist());

  if (!map["ORIGINALDATE"].isEmpty()) song->set_originalyear(TStringToQString(map["ORIGINALDATE"].front()).left(4).toInt());
  else if (!map["ORIGINALYEAR"].isEmpty()) song->set_originalyear(TStringToQString(map["ORIGINALYEAR"].front()).toInt());

  if (!map["DISCNUMBER"].isEmpty()) *disc = TStringToQString( map["DISCNUMBER"].front() ).trimmed();
  if (!map["COMPILATION"].isEmpty()) *compilation = TStringToQString( map["COMPILATION"].front() ).trimmed();
  if (!map["COVERART"].isEmpty()) song->set_art_automatic(kEmbeddedCover);
  if (!map["METADATA_BLOCK_PICTURE"].isEmpty()) song->set_art_automatic(kEmbeddedCover);

  if (!map["FMPS_PLAYCOUNT"].isEmpty() && song->playcount() <= 0) song->set_playcount(TStringToQString( map["FMPS_PLAYCOUNT"].front() ).trimmed().toFloat());

  if (!map["LYRICS"].isEmpty()) Decode(map["LYRICS"].front(), song->mutable_lyrics());
  else if (!map["UNSYNCEDLYRICS"].isEmpty()) Decode(map["UNSYNCEDLYRICS"].front(), song->mutable_lyrics());

}

void TagReader::ParseAPETag(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, pb::tagreader::SongMetadata *song) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find("ALBUM ARTIST");
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.values();
    if (!album_artists.isEmpty()) {
      Decode(album_artists.front(), song->mutable_albumartist());
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
    Decode(map["PERFORMER"].values().toString(", "), song->mutable_performer());
  }

  if (map.contains("COMPOSER")) {
    Decode(map["COMPOSER"].values().toString(", "), song->mutable_composer());
  }

  if (map.contains("GROUPING")) {
    Decode(map["GROUPING"].values().toString(" "), song->mutable_grouping());
  }

  if (map.contains("LYRICS")) {
    Decode(map["LYRICS"].toString(), song->mutable_lyrics());
  }

  if (map.contains("FMPS_PLAYCOUNT")) {
    int playcount = TStringToQString(map["FMPS_PLAYCOUNT"].toString()).toFloat();
    if (song->playcount() <= 0 && playcount > 0) {
      song->set_playcount(playcount);
    }
  }

}

void TagReader::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comments, const pb::tagreader::SongMetadata &song) const {

  vorbis_comments->addField("COMPOSER", StdStringToTaglibString(song.composer()), true);
  vorbis_comments->addField("PERFORMER", StdStringToTaglibString(song.performer()), true);
  vorbis_comments->addField("CONTENT GROUP", StdStringToTaglibString(song.grouping()), true);
  vorbis_comments->addField("DISCNUMBER", QStringToTaglibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  vorbis_comments->addField("COMPILATION", QStringToTaglibString(song.compilation() ? "1" : QString()), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comments->addField("ALBUMARTIST", StdStringToTaglibString(song.albumartist()), true);
  vorbis_comments->removeFields("ALBUM ARTIST");

  vorbis_comments->addField("LYRICS", StdStringToTaglibString(song.lyrics()), true);
  vorbis_comments->removeFields("UNSYNCEDLYRICS");

}

bool TagReader::SaveFile(const QString &filename, const pb::tagreader::SongMetadata &song) const {

  if (filename.isEmpty()) return false;

  qLog(Debug) << "Saving tags to" << filename;
  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));;
  if (!fileref || fileref->isNull()) return false;

  fileref->tag()->setTitle(song.title().empty() ? TagLib::String() : StdStringToTaglibString(song.title()));
  fileref->tag()->setArtist(song.artist().empty() ? TagLib::String() : StdStringToTaglibString(song.artist()));
  fileref->tag()->setAlbum(song.album().empty() ? TagLib::String() : StdStringToTaglibString(song.album()));
  fileref->tag()->setGenre(song.genre().empty() ? TagLib::String() : StdStringToTaglibString(song.genre()));
  fileref->tag()->setComment(song.comment().empty() ? TagLib::String() : StdStringToTaglibString(song.comment()));
  fileref->tag()->setYear(song.year() <= 0 ? 0 : song.year());
  fileref->tag()->setTrack(song.track() <= 0 ? 0 : song.track());

  bool result = false;

  if (TagLib::FLAC::File *file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *tag = file->xiphComment();
    SetVorbisComments(tag, song);
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_wavpack->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_ape->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_mpc->APETag(true);
    if (!tag) return false;
    SaveAPETag(tag, song);
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mpeg->ID3v2Tag(true);
    if (!tag) return false;
    SetTextFrame("TPOS", song.disc() <= 0 ? QString() : QString::number(song.disc()), tag);
    SetTextFrame("TCOM", song.composer().empty() ? std::string() : song.composer(), tag);
    SetTextFrame("TIT1", song.grouping().empty() ? std::string() : song.grouping(), tag);
    SetTextFrame("TOPE", song.performer().empty() ? std::string() : song.performer(), tag);
    // Skip TPE1 (which is the artist) here because we already set it
    SetTextFrame("TPE2", song.albumartist().empty() ? std::string() : song.albumartist(), tag);
    SetTextFrame("TCMP", song.compilation() ? QString::number(1) : QString(), tag);
    SetUnsyncLyricsFrame(song.lyrics().empty() ? std::string() : song.lyrics(), tag);
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_mp4->tag();
    tag->setItem("disk", TagLib::MP4::Item(song.disc() <= 0 -1 ? 0 : song.disc(), 0));
    tag->setItem("\251wrt", TagLib::StringList(song.composer().c_str()));
    tag->setItem("\251grp", TagLib::StringList(song.grouping().c_str()));
    tag->setItem("\251lyr", TagLib::StringList(song.lyrics().c_str()));
    tag->setItem("aART", TagLib::StringList(song.albumartist().c_str()));
    tag->setItem("cpil", TagLib::StringList(song.compilation() ? "1" : "0"));
  }

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block above.
  if (TagLib::Ogg::XiphComment *tag = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetVorbisComments(tag, song);
  }

  result = fileref->save();
#ifdef Q_OS_LINUX
  if (result) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return result;
}

void TagReader::SaveAPETag(TagLib::APE::Tag *tag, const pb::tagreader::SongMetadata &song) const {

  tag->setItem("album artist", TagLib::APE::Item("album artist", TagLib::StringList(song.albumartist().c_str())));
  tag->addValue("disc", QStringToTaglibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  tag->setItem("composer", TagLib::APE::Item("composer", TagLib::StringList(song.composer().c_str())));
  tag->setItem("grouping", TagLib::APE::Item("grouping", TagLib::StringList(song.grouping().c_str())));
  tag->setItem("performer", TagLib::APE::Item("performer", TagLib::StringList(song.performer().c_str())));
  tag->setItem("lyrics", TagLib::APE::Item("lyrics", TagLib::String(song.lyrics())));
  tag->addValue("compilation", QStringToTaglibString(song.compilation() ? QString::number(1) : QString()), true);

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

  if (value.empty()) return;

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::TextIdentificationFrame frame(id_vector, TagLib::String::UTF8);
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0 ; i < frames_buffer.size() ; ++i) {
    TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

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
      // Use the first picture in the file - this could be made cleverer and pick the front cover if it's present.

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

    return QByteArray(reinterpret_cast<const char*>(pic->picture().data()), pic->picture().size());
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
    if ((pos > 0) && (static_cast<uint>(pos) < data.size())) {
      ret = QByteArray(data.data() + pos, data.size() - pos);
    }
  }

  return ret;

}

void TagReader::SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector("USLT");
  QVector<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  if (value.empty()) return;

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame frame(TagLib::String::UTF8);
    frame.setDescription("Clementine editor");
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0 ; i < frames_buffer.size() ; ++i) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}
