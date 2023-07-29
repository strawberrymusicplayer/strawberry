/* This file is part of Strawberry.
   Copyright 2013, David Sansome <me@davidsansome.com>
   Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>

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

#include "tagreadertaglib.h"

#include <string>
#include <memory>
#include <algorithm>
#include <sys/stat.h>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tbytevector.h>
#include <taglib/tfile.h>
#include <taglib/tlist.h>
#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/tpropertymap.h>
#include <taglib/audioproperties.h>
#include <taglib/xiphcomment.h>
#include <taglib/tag.h>
#include <taglib/apetag.h>
#include <taglib/apeitem.h>
#include <taglib/apeproperties.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/uniquefileidentifierframe.h>
#include <taglib/commentsframe.h>
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
#include <taglib/modfile.h>
#include <taglib/s3mfile.h>
#include <taglib/xmfile.h>
#include <taglib/itfile.h>
#ifdef HAVE_TAGLIB_DSFFILE
#  include <taglib/dsffile.h>
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
#  include <taglib/dsdifffile.h>
#endif

#include <QtGlobal>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"
#include "utilities/timeconstants.h"

class FileRefFactory {
 public:
  FileRefFactory() = default;
  virtual ~FileRefFactory() = default;
  virtual TagLib::FileRef *GetFileRef(const QString &filename) = 0;

 private:
  Q_DISABLE_COPY(FileRefFactory)
};

class TagLibFileRefFactory : public FileRefFactory {
 public:
  TagLibFileRefFactory() = default;
  TagLib::FileRef *GetFileRef(const QString &filename) override {
#ifdef Q_OS_WIN32
    return new TagLib::FileRef(filename.toStdWString().c_str());
#else
    return new TagLib::FileRef(QFile::encodeName(filename).constData());
#endif
  }

 private:
  Q_DISABLE_COPY(TagLibFileRefFactory)
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

constexpr char kID3v2_AcoustID_ID[] = "Acoustid Id";
constexpr char kID3v2_AcoustID_Fingerprint[] = "Acoustid Fingerprint";
constexpr char kID3v2_MusicBrainz_AlbumArtistID[] = "MusicBrainz Album Artist Id";
constexpr char kID3v2_MusicBrainz_ArtistID[] = "MusicBrainz Artist Id";
constexpr char kID3v2_MusicBrainz_OriginalArtistID[] = "MusicBrainz Original Artist Id";
constexpr char kID3v2_MusicBrainz_AlbumID[] = "MusicBrainz Album Id";
constexpr char kID3v2_MusicBrainz_OriginalAlbumID[] = "MusicBrainz Original Album Id";
constexpr char kID3v2_MusicBrainz_RecordingID[] = "MUSICBRAINZ_TRACKID";
constexpr char kID3v2_MusicBrainz_TrackID[] = "MusicBrainz Release Track Id";
constexpr char kID3v2_MusicBrainz_DiscID[] = "MusicBrainz Disc Id";
constexpr char kID3v2_MusicBrainz_ReleaseGroupID[] = "MusicBrainz Release Group Id";
constexpr char kID3v2_MusicBrainz_WorkID[] = "MusicBrainz Work Id";

constexpr char kMP4_OriginalYear_ID[] = "----:com.apple.iTunes:ORIGINAL YEAR";
constexpr char kMP4_FMPS_Playcount_ID[] = "----:com.apple.iTunes:FMPS_Playcount";
constexpr char kMP4_FMPS_Rating_ID[] = "----:com.apple.iTunes:FMPS_Rating";
constexpr char kMP4_AcoustID_ID[] = "----:com.apple.iTunes:Acoustid Id";
constexpr char kMP4_AcoustID_Fingerprint[] = "----:com.apple.iTunes:Acoustid Fingerprint";
constexpr char kMP4_MusicBrainz_AlbumArtistID[] = "----:com.apple.iTunes:MusicBrainz Album Artist Id";
constexpr char kMP4_MusicBrainz_ArtistID[] = "----:com.apple.iTunes:MusicBrainz Artist Id";
constexpr char kMP4_MusicBrainz_OriginalArtistID[] = "----:com.apple.iTunes:MusicBrainz Original Artist Id";
constexpr char kMP4_MusicBrainz_AlbumID[] = "----:com.apple.iTunes:MusicBrainz Album Id";
constexpr char kMP4_MusicBrainz_OriginalAlbumID[] = "----:com.apple.iTunes:MusicBrainz Original Album Id";
constexpr char kMP4_MusicBrainz_RecordingID[] = "----:com.apple.iTunes:MusicBrainz Track Id";
constexpr char kMP4_MusicBrainz_TrackID[] = "----:com.apple.iTunes:MusicBrainz Release Track Id";
constexpr char kMP4_MusicBrainz_DiscID[] = "----:com.apple.iTunes:MusicBrainz Disc Id";
constexpr char kMP4_MusicBrainz_ReleaseGroupID[] = "----:com.apple.iTunes:MusicBrainz Release Group Id";
constexpr char kMP4_MusicBrainz_WorkID[] = "----:com.apple.iTunes:MusicBrainz Work Id";

constexpr char kASF_OriginalDate_ID[] = "WM/OriginalReleaseTime";
constexpr char kASF_OriginalYear_ID[] = "WM/OriginalReleaseYear";
constexpr char kASF_AcoustID_ID[] = "Acoustid/Id";
constexpr char kASF_AcoustID_Fingerprint[] = "Acoustid/Fingerprint";
constexpr char kASF_MusicBrainz_AlbumArtistID[] = "MusicBrainz/Album Artist Id";
constexpr char kASF_MusicBrainz_ArtistID[] = "MusicBrainz/Artist Id";
constexpr char kASF_MusicBrainz_OriginalArtistID[] = "MusicBrainz/Original Artist Id";
constexpr char kASF_MusicBrainz_AlbumID[] = "MusicBrainz/Album Id";
constexpr char kASF_MusicBrainz_OriginalAlbumID[] = "MusicBrainz/Original Album Id";
constexpr char kASF_MusicBrainz_RecordingID[] = "MusicBrainz/Track Id";
constexpr char kASF_MusicBrainz_TrackID[] = "MusicBrainz/Release Track Id";
constexpr char kASF_MusicBrainz_DiscID[] = "MusicBrainz/Disc Id";
constexpr char kASF_MusicBrainz_ReleaseGroupID[] = "MusicBrainz/Release Group Id";
constexpr char kASF_MusicBrainz_WorkID[] = "MusicBrainz/Work Id";

}  // namespace


TagReaderTagLib::TagReaderTagLib() : factory_(new TagLibFileRefFactory) {}

TagReaderTagLib::~TagReaderTagLib() {
  delete factory_;
}

bool TagReaderTagLib::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  return fileref && !fileref->isNull() && fileref->file() && fileref->tag();

}

spb::tagreader::SongMetadata_FileType TagReaderTagLib::GuessFileType(TagLib::FileRef *fileref) const {

  if (dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_WAV;
  if (dynamic_cast<TagLib::FLAC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_FLAC;
  if (dynamic_cast<TagLib::WavPack::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_WAVPACK;
  if (dynamic_cast<TagLib::Ogg::FLAC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGFLAC;
  if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGVORBIS;
  if (dynamic_cast<TagLib::Ogg::Opus::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGOPUS;
  if (dynamic_cast<TagLib::Ogg::Speex::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGSPEEX;
  if (dynamic_cast<TagLib::MPEG::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MPEG;
  if (dynamic_cast<TagLib::MP4::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MP4;
  if (dynamic_cast<TagLib::ASF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_ASF;
  if (dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_AIFF;
  if (dynamic_cast<TagLib::MPC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MPC;
  if (dynamic_cast<TagLib::TrueAudio::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_TRUEAUDIO;
  if (dynamic_cast<TagLib::APE::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_APE;
  if (dynamic_cast<TagLib::Mod::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MOD;
  if (dynamic_cast<TagLib::S3M::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_S3M;
  if (dynamic_cast<TagLib::XM::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_XM;
  if (dynamic_cast<TagLib::IT::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_IT;
#ifdef HAVE_TAGLIB_DSFFILE
  if (dynamic_cast<TagLib::DSF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_DSF;
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
  if (dynamic_cast<TagLib::DSDIFF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_DSDIFF;
#endif

  return spb::tagreader::SongMetadata_FileType_UNKNOWN;

}

bool TagReaderTagLib::ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const {

  const QByteArray url(QUrl::fromLocalFile(filename).toEncoded());
  const QFileInfo fileinfo(filename);

  qLog(Debug) << "Reading tags from" << filename;

  const QByteArray basefilename = fileinfo.fileName().toUtf8();
  song->set_basefilename(basefilename.constData(), basefilename.length());
  song->set_url(url.constData(), url.size());
  song->set_filesize(fileinfo.size());

  song->set_mtime(fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  song->set_ctime(fileinfo.birthTime().isValid() ? std::max(fileinfo.birthTime().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);

  if (song->ctime() <= 0) {
    song->set_ctime(song->mtime());
  }

  song->set_lastseen(QDateTime::currentDateTime().toSecsSinceEpoch());

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (fileref->isNull()) {
    qLog(Info) << "TagLib hasn't been able to read" << filename << "file";
    return false;
  }

  song->set_filetype(GuessFileType(fileref.get()));

  if (fileref->audioProperties()) {
    song->set_bitrate(fileref->audioProperties()->bitrate());
    song->set_samplerate(fileref->audioProperties()->sampleRate());
    song->set_length_nanosec(fileref->audioProperties()->lengthInMilliseconds() * kNsecPerMsec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    TStringToStdString(tag->title(), song->mutable_title());
    TStringToStdString(tag->artist(), song->mutable_artist());  // TPE1
    TStringToStdString(tag->album(), song->mutable_album());
    TStringToStdString(tag->genre(), song->mutable_genre());
    song->set_year(static_cast<int>(tag->year()));
    song->set_track(static_cast<int>(tag->track()));
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseOggTag(xiph_comment->fieldListMap(), &disc, &compilation, song);
    TagLib::List<TagLib::FLAC::Picture*> pictures = xiph_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
          song->set_art_embedded(true);
          break;
        }
      }
    }
  }

  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    song->set_bitdepth(file_flac->audioProperties()->bitsPerSample());
    if (file_flac->xiphComment()) {
      ParseOggTag(file_flac->xiphComment()->fieldListMap(), &disc, &compilation, song);
      TagLib::List<TagLib::FLAC::Picture*> pictures = file_flac->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
            song->set_art_embedded(true);
            break;
          }
        }
      }
    }
    if (tag) TStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    song->set_bitdepth(file_wavpack->audioProperties()->bitsPerSample());
    if (file_wavpack->APETag()) {
      ParseAPETag(file_wavpack->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) TStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (file_ape->APETag()) {
      ParseAPETag(file_ape->APETag()->itemListMap(), &disc, &compilation, song);
    }
    song->set_bitdepth(file_ape->audioProperties()->bitsPerSample());
    if (tag) TStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {

    if (file_mpeg->ID3v2Tag()) {
      const TagLib::ID3v2::FrameListMap &map = file_mpeg->ID3v2Tag()->frameListMap();

      if (map.contains("TPOS")) disc = TStringToQString(map["TPOS"].front()->toString()).trimmed();
      if (map.contains("TCOM")) TStringToStdString(map["TCOM"].front()->toString(), song->mutable_composer());

      // content group
      if (map.contains("TIT1")) TStringToStdString(map["TIT1"].front()->toString(), song->mutable_grouping());

      // original artist/performer
      if (map.contains("TOPE")) TStringToStdString(map["TOPE"].front()->toString(), song->mutable_performer());

      // Skip TPE1 (which is the artist) here because we already fetched it

      // non-standard: Apple, Microsoft
      if (map.contains("TPE2")) TStringToStdString(map["TPE2"].front()->toString(), song->mutable_albumartist());

      if (map.contains("TCMP")) compilation = TStringToQString(map["TCMP"].front()->toString()).trimmed();

      if (map.contains("TDOR")) {
        song->set_originalyear(map["TDOR"].front()->toString().substr(0, 4).toInt());
      }
      else if (map.contains("TORY")) {
        song->set_originalyear(map["TORY"].front()->toString().substr(0, 4).toInt());
      }

      if (map.contains("USLT")) {
        TStringToStdString(map["USLT"].front()->toString(), song->mutable_lyrics());
      }
      else if (map.contains("SYLT")) {
        TStringToStdString(map["SYLT"].front()->toString(), song->mutable_lyrics());
      }

      if (map.contains("APIC")) song->set_art_embedded(true);

      // Find a suitable comment tag.  For now we ignore iTunNORM comments.
      for (uint i = 0; i < map["COMM"].size(); ++i) {
        const TagLib::ID3v2::CommentsFrame *frame = dynamic_cast<const TagLib::ID3v2::CommentsFrame*>(map["COMM"][i]);

        if (frame && TStringToQString(frame->description()) != "iTunNORM") {
          TStringToStdString(frame->text(), song->mutable_comment());
          break;
        }
      }

      if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_playcount = TagLib::ID3v2::UserTextIdentificationFrame::find(file_mpeg->ID3v2Tag(), "FMPS_Playcount")) {
        TagLib::StringList frame_field_list = frame_fmps_playcount->fieldList();
        if (frame_field_list.size() > 1) {
          int playcount = TStringToQString(frame_field_list[1]).toInt();
          if (song->playcount() <= 0 && playcount > 0) {
            song->set_playcount(playcount);
          }
        }
      }

      if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_rating = TagLib::ID3v2::UserTextIdentificationFrame::find(file_mpeg->ID3v2Tag(), "FMPS_Rating")) {
        TagLib::StringList frame_field_list = frame_fmps_rating->fieldList();
        if (frame_field_list.size() > 1) {
          float rating = TStringToQString(frame_field_list[1]).toFloat();
          if (song->rating() <= 0 && rating > 0 && rating <= 1.0) {
            song->set_rating(rating);
          }
        }
      }

      if (map.contains("POPM")) {
        const TagLib::ID3v2::PopularimeterFrame *frame = dynamic_cast<const TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
        if (frame) {
          if (song->playcount() <= 0 && frame->counter() > 0) {
            song->set_playcount(frame->counter());
          }
          if (song->rating() <= 0 && frame->rating() > 0) {
            song->set_rating(ConvertPOPMRating(frame->rating()));
          }
        }
      }

      if (map.contains("UFID")) {
        for (uint i = 0; i < map["UFID"].size(); ++i) {
          if (TagLib::ID3v2::UniqueFileIdentifierFrame *frame = dynamic_cast<TagLib::ID3v2::UniqueFileIdentifierFrame*>(map["UFID"][i])) {
            const TagLib::PropertyMap property_map = frame->asProperties();
            if (property_map.contains(kID3v2_MusicBrainz_RecordingID)) {
              TStringToStdString(property_map[kID3v2_MusicBrainz_RecordingID].toString(), song->mutable_musicbrainz_recording_id());
            }
          }
        }
      }

      if (map.contains("TXXX")) {
        for (uint i = 0; i < map["TXXX"].size(); ++i) {
          if (TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(map["TXXX"][i])) {
            const TagLib::StringList frame_field_list = frame->fieldList();
            if (frame_field_list.size() != 2) continue;
            if (frame->description() == kID3v2_AcoustID_ID) {
              TStringToStdString(frame_field_list.back(), song->mutable_acoustid_id());
            }
            if (frame->description() == kID3v2_AcoustID_Fingerprint) {
              TStringToStdString(frame_field_list.back(), song->mutable_acoustid_fingerprint());
            }
            if (frame->description() == kID3v2_MusicBrainz_AlbumArtistID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_artist_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_ArtistID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_artist_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_OriginalArtistID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_artist_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_AlbumID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_OriginalAlbumID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_album_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_TrackID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_track_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_DiscID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_disc_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_ReleaseGroupID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_release_group_id());
            }
            if (frame->description() == kID3v2_MusicBrainz_WorkID) {
              TStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_work_id());
            }
          }
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
          TStringToStdString(album_artists.front(), song->mutable_albumartist());
        }
      }

      // Find album cover art
      if (mp4_tag->item("covr").isValid()) {
        song->set_art_embedded(true);
      }

      if (mp4_tag->item("disk").isValid()) {
        disc = TStringToQString(TagLib::String::number(mp4_tag->item("disk").toIntPair().first));
      }

      if (mp4_tag->item("\251wrt").isValid()) {
        TStringToStdString(mp4_tag->item("\251wrt").toStringList().toString(", "), song->mutable_composer());
      }
      if (mp4_tag->item("\251grp").isValid()) {
        TStringToStdString(mp4_tag->item("\251grp").toStringList().toString(" "), song->mutable_grouping());
      }
      if (mp4_tag->item("\251lyr").isValid()) {
        TStringToStdString(mp4_tag->item("\251lyr").toStringList().toString(" "), song->mutable_lyrics());
      }

      if (mp4_tag->item(kMP4_OriginalYear_ID).isValid()) {
        song->set_originalyear(TStringToQString(mp4_tag->item(kMP4_OriginalYear_ID).toStringList().toString('\n')).left(4).toInt());
      }

      if (mp4_tag->item("cpil").isValid()) {
        song->set_compilation(mp4_tag->item("cpil").toBool());
      }

      {
        TagLib::MP4::Item item = mp4_tag->item(kMP4_FMPS_Playcount_ID);
        if (item.isValid()) {
          const int playcount = TStringToQString(item.toStringList().toString('\n')).toInt();
          if (song->playcount() <= 0 && playcount > 0) {
            song->set_playcount(static_cast<uint>(playcount));
          }
        }
      }

      {
        TagLib::MP4::Item item = mp4_tag->item(kMP4_FMPS_Rating_ID);
        if (item.isValid()) {
          const float rating = TStringToQString(item.toStringList().toString('\n')).toFloat();
          if (song->rating() <= 0 && rating > 0) {
            song->set_rating(rating);
          }
        }
      }

      TStringToStdString(mp4_tag->comment(), song->mutable_comment());

      if (mp4_tag->contains(kMP4_AcoustID_ID)) {
        TStringToStdString(mp4_tag->item(kMP4_AcoustID_ID).toStringList().toString(), song->mutable_acoustid_id());
      }
      if (mp4_tag->contains(kMP4_AcoustID_Fingerprint)) {
        TStringToStdString(mp4_tag->item(kMP4_AcoustID_Fingerprint).toStringList().toString(), song->mutable_acoustid_fingerprint());
      }

      if (mp4_tag->contains(kMP4_MusicBrainz_AlbumArtistID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_AlbumArtistID).toStringList().toString(), song->mutable_musicbrainz_album_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_ArtistID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_ArtistID).toStringList().toString(), song->mutable_musicbrainz_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_OriginalArtistID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_OriginalArtistID).toStringList().toString(), song->mutable_musicbrainz_original_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_AlbumID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_AlbumID).toStringList().toString(), song->mutable_musicbrainz_album_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_OriginalAlbumID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_OriginalAlbumID).toStringList().toString(), song->mutable_musicbrainz_original_album_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_RecordingID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_RecordingID).toStringList().toString(), song->mutable_musicbrainz_recording_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_TrackID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_TrackID).toStringList().toString(), song->mutable_musicbrainz_track_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_DiscID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_DiscID).toStringList().toString(), song->mutable_musicbrainz_disc_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_ReleaseGroupID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_ReleaseGroupID).toStringList().toString(), song->mutable_musicbrainz_release_group_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_WorkID)) {
        TStringToStdString(mp4_tag->item(kMP4_MusicBrainz_WorkID).toStringList().toString(), song->mutable_musicbrainz_work_id());
      }

    }
  }

  else if (TagLib::ASF::File *file_asf = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {

    song->set_bitdepth(file_asf->audioProperties()->bitsPerSample());

    if (file_asf->tag()) {
      TStringToStdString(file_asf->tag()->comment(), song->mutable_comment());

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

      if (attributes_map.contains("FMPS/Playcount")) {
        const TagLib::ASF::AttributeList &attributes = attributes_map["FMPS/Playcount"];
        if (!attributes.isEmpty()) {
          int playcount = TStringToQString(attributes.front().toString()).toInt();
          if (song->playcount() <= 0 && playcount > 0) {
            song->set_playcount(static_cast<uint>(playcount));
          }
        }
      }

      if (attributes_map.contains("FMPS/Rating")) {
        const TagLib::ASF::AttributeList &attributes = attributes_map["FMPS/Rating"];
        if (!attributes.isEmpty()) {
          float rating = TStringToQString(attributes.front().toString()).toFloat();
          if (song->rating() <= 0 && rating > 0) {
            song->set_rating(rating);
          }
        }
      }

      if (attributes_map.contains(kASF_AcoustID_ID)) {
        TStringToStdString(attributes_map[kASF_AcoustID_ID].front().toString(), song->mutable_acoustid_id());
      }
      if (attributes_map.contains(kASF_AcoustID_Fingerprint)) {
        TStringToStdString(attributes_map[kASF_AcoustID_Fingerprint].front().toString(), song->mutable_acoustid_fingerprint());
      }

      if (attributes_map.contains(kASF_MusicBrainz_AlbumArtistID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_AlbumArtistID].front().toString(), song->mutable_musicbrainz_album_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_ArtistID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_ArtistID].front().toString(), song->mutable_musicbrainz_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_OriginalArtistID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_OriginalArtistID].front().toString(), song->mutable_musicbrainz_original_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_AlbumID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_AlbumID].front().toString(), song->mutable_musicbrainz_album_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_OriginalAlbumID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_OriginalAlbumID].front().toString(), song->mutable_musicbrainz_original_album_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_RecordingID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_RecordingID].front().toString(), song->mutable_musicbrainz_recording_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_TrackID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_TrackID].front().toString(), song->mutable_musicbrainz_track_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_DiscID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_DiscID].front().toString(), song->mutable_musicbrainz_disc_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_ReleaseGroupID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_ReleaseGroupID].front().toString(), song->mutable_musicbrainz_release_group_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_WorkID)) {
        TStringToStdString(attributes_map[kASF_MusicBrainz_WorkID].front().toString(), song->mutable_musicbrainz_work_id());
      }

    }

  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      ParseAPETag(file_mpc->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) TStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (tag) {
    TStringToStdString(tag->comment(), song->mutable_comment());
  }

  if (!disc.isEmpty()) {
    const qint64 i = disc.indexOf('/');
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
    const QString albumartist = QString::fromUtf8(song->albumartist().data(), static_cast<qint64>(song->albumartist().size()));
    const QString artist = QString::fromUtf8(song->artist().data(), static_cast<qint64>(song->artist().size()));
    if (artist.compare("various artists") == 0 || albumartist.compare("various artists") == 0) {
      song->set_compilation(true);
    }
  }
  else {
    song->set_compilation(compilation.toInt() == 1);
  }

  if (!lyrics.isEmpty()) song->set_lyrics(lyrics.toStdString());

  // Set integer fields to -1 if they're not valid

  if (song->track() <= 0) { song->set_track(-1); }
  if (song->disc() <= 0) { song->set_disc(-1); }
  if (song->year() <= 0) { song->set_year(-1); }
  if (song->originalyear() <= 0) { song->set_originalyear(-1); }
  if (song->samplerate() <= 0) { song->set_samplerate(-1); }
  if (song->bitdepth() <= 0) { song->set_bitdepth(-1); }
  if (song->bitrate() <= 0) { song->set_bitrate(-1); }
  if (song->lastplayed() <= 0) { song->set_lastplayed(-1); }

  return song->filetype() != spb::tagreader::SongMetadata_FileType_UNKNOWN;

}

void TagReaderTagLib::TStringToStdString(const TagLib::String &tag, std::string *output) {

  const QString tmp = TStringToQString(tag).trimmed();
  const QByteArray data = tmp.toUtf8();
  output->assign(data.constData(), data.size());

}

void TagReaderTagLib::ParseOggTag(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  if (map.contains("COMPOSER")) TStringToStdString(map["COMPOSER"].front(), song->mutable_composer());
  if (map.contains("PERFORMER")) TStringToStdString(map["PERFORMER"].front(), song->mutable_performer());
  if (map.contains("CONTENT GROUP")) TStringToStdString(map["CONTENT GROUP"].front(), song->mutable_grouping());
  if (map.contains("GROUPING")) TStringToStdString(map["GROUPING"].front(), song->mutable_grouping());

  if (map.contains("ALBUMARTIST")) TStringToStdString(map["ALBUMARTIST"].front(), song->mutable_albumartist());
  else if (map.contains("ALBUM ARTIST")) TStringToStdString(map["ALBUM ARTIST"].front(), song->mutable_albumartist());

  if (map.contains("ORIGINALDATE")) song->set_originalyear(TStringToQString(map["ORIGINALDATE"].front()).left(4).toInt());
  else if (map.contains("ORIGINALYEAR")) song->set_originalyear(TStringToQString(map["ORIGINALYEAR"].front()).toInt());

  if (map.contains("DISCNUMBER")) *disc = TStringToQString(map["DISCNUMBER"].front()).trimmed();
  if (map.contains("COMPILATION")) *compilation = TStringToQString(map["COMPILATION"].front()).trimmed();
  if (map.contains("COVERART")) song->set_art_embedded(true);
  if (map.contains("METADATA_BLOCK_PICTURE")) song->set_art_embedded(true);

  if (map.contains("FMPS_PLAYCOUNT") && song->playcount() <= 0) {
    const int playcount = TStringToQString(map["FMPS_PLAYCOUNT"].front()).trimmed().toInt();
    song->set_playcount(static_cast<uint>(playcount));
  }
  if (map.contains("FMPS_RATING") && song->rating() <= 0) song->set_rating(TStringToQString(map["FMPS_RATING"].front()).trimmed().toFloat());

  if (map.contains("LYRICS")) TStringToStdString(map["LYRICS"].front(), song->mutable_lyrics());
  else if (map.contains("UNSYNCEDLYRICS")) TStringToStdString(map["UNSYNCEDLYRICS"].front(), song->mutable_lyrics());

  if (map.contains("ACOUSTID_ID")) TStringToStdString(map["ACOUSTID_ID"].front(), song->mutable_acoustid_id());
  if (map.contains("ACOUSTID_FINGERPRINT")) TStringToStdString(map["ACOUSTID_FINGERPRINT"].front(), song->mutable_acoustid_fingerprint());

  if (map.contains("MUSICBRAINZ_ALBUMARTISTID")) TStringToStdString(map["MUSICBRAINZ_ALBUMARTISTID"].front(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains("MUSICBRAINZ_ARTISTID")) TStringToStdString(map["MUSICBRAINZ_ARTISTID"].front(), song->mutable_musicbrainz_artist_id());
  if (map.contains("MUSICBRAINZ_ORIGINALARTISTID")) TStringToStdString(map["MUSICBRAINZ_ORIGINALARTISTID"].front(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains("MUSICBRAINZ_ALBUMID")) TStringToStdString(map["MUSICBRAINZ_ALBUMID"].front(), song->mutable_musicbrainz_album_id());
  if (map.contains("MUSICBRAINZ_ORIGINALALBUMID")) TStringToStdString(map["MUSICBRAINZ_ORIGINALALBUMID"].front(), song->mutable_musicbrainz_original_album_id());
  if (map.contains("MUSICBRAINZ_TRACKID")) TStringToStdString(map["MUSICBRAINZ_TRACKID"].front(), song->mutable_musicbrainz_recording_id());
  if (map.contains("MUSICBRAINZ_RELEASETRACKID")) TStringToStdString(map["MUSICBRAINZ_RELEASETRACKID"].front(), song->mutable_musicbrainz_track_id());
  if (map.contains("MUSICBRAINZ_DISCID")) TStringToStdString(map["MUSICBRAINZ_DISCID"].front(), song->mutable_musicbrainz_disc_id());
  if (map.contains("MUSICBRAINZ_RELEASEGROUPID")) TStringToStdString(map["MUSICBRAINZ_RELEASEGROUPID"].front(), song->mutable_musicbrainz_release_group_id());
  if (map.contains("MUSICBRAINZ_WORKID")) TStringToStdString(map["MUSICBRAINZ_WORKID"].front(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::ParseAPETag(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find("ALBUM ARTIST");
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.values();
    if (!album_artists.isEmpty()) {
      TStringToStdString(album_artists.front(), song->mutable_albumartist());
    }
  }

  if (map.find("COVER ART (FRONT)") != map.end()) song->set_art_embedded(true);
  if (map.contains("COMPILATION")) {
    *compilation = TStringToQString(TagLib::String::number(map["COMPILATION"].toString().toInt()));
  }

  if (map.contains("DISC")) {
    *disc = TStringToQString(TagLib::String::number(map["DISC"].toString().toInt()));
  }

  if (map.contains("PERFORMER")) {
    TStringToStdString(map["PERFORMER"].values().toString(", "), song->mutable_performer());
  }

  if (map.contains("COMPOSER")) {
    TStringToStdString(map["COMPOSER"].values().toString(", "), song->mutable_composer());
  }

  if (map.contains("GROUPING")) {
    TStringToStdString(map["GROUPING"].values().toString(" "), song->mutable_grouping());
  }

  if (map.contains("LYRICS")) {
    TStringToStdString(map["LYRICS"].toString(), song->mutable_lyrics());
  }

  if (map.contains("FMPS_PLAYCOUNT")) {
    const int playcount = TStringToQString(map["FMPS_PLAYCOUNT"].toString()).toInt();
    if (song->playcount() <= 0 && playcount > 0) {
      song->set_playcount(static_cast<uint>(playcount));
    }
  }

  if (map.contains("FMPS_RATING")) {
    const float rating = TStringToQString(map["FMPS_RATING"].toString()).toFloat();
    if (song->rating() <= 0 && rating > 0) {
      song->set_rating(rating);
    }
  }

  if (map.contains("ACOUSTID_ID")) TStringToStdString(map["ACOUSTID_ID"].toString(), song->mutable_acoustid_id());
  if (map.contains("ACOUSTID_FINGERPRINT")) TStringToStdString(map["ACOUSTID_FINGERPRINT"].toString(), song->mutable_acoustid_fingerprint());

  if (map.contains("MUSICBRAINZ_ALBUMARTISTID")) TStringToStdString(map["MUSICBRAINZ_ALBUMARTISTID"].toString(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains("MUSICBRAINZ_ARTISTID")) TStringToStdString(map["MUSICBRAINZ_ARTISTID"].toString(), song->mutable_musicbrainz_artist_id());
  if (map.contains("MUSICBRAINZ_ORIGINALARTISTID")) TStringToStdString(map["MUSICBRAINZ_ORIGINALARTISTID"].toString(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains("MUSICBRAINZ_ALBUMID")) TStringToStdString(map["MUSICBRAINZ_ALBUMID"].toString(), song->mutable_musicbrainz_album_id());
  if (map.contains("MUSICBRAINZ_ORIGINALALBUMID")) TStringToStdString(map["MUSICBRAINZ_ORIGINALALBUMID"].toString(), song->mutable_musicbrainz_original_album_id());
  if (map.contains("MUSICBRAINZ_TRACKID")) TStringToStdString(map["MUSICBRAINZ_TRACKID"].toString(), song->mutable_musicbrainz_recording_id());
  if (map.contains("MUSICBRAINZ_RELEASETRACKID")) TStringToStdString(map["MUSICBRAINZ_RELEASETRACKID"].toString(), song->mutable_musicbrainz_track_id());
  if (map.contains("MUSICBRAINZ_DISCID")) TStringToStdString(map["MUSICBRAINZ_DISCID"].toString(), song->mutable_musicbrainz_disc_id());
  if (map.contains("MUSICBRAINZ_RELEASEGROUPID")) TStringToStdString(map["MUSICBRAINZ_RELEASEGROUPID"].toString(), song->mutable_musicbrainz_release_group_id());
  if (map.contains("MUSICBRAINZ_WORKID")) TStringToStdString(map["MUSICBRAINZ_WORKID"].toString(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const spb::tagreader::SongMetadata &song) const {

  vorbis_comment->addField("COMPOSER", StdStringToTaglibString(song.composer()), true);
  vorbis_comment->addField("PERFORMER", StdStringToTaglibString(song.performer()), true);
  vorbis_comment->addField("GROUPING", StdStringToTaglibString(song.grouping()), true);
  vorbis_comment->addField("DISCNUMBER", QStringToTaglibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  vorbis_comment->addField("COMPILATION", QStringToTaglibString(song.compilation() ? "1" : QString()), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comment->addField("ALBUMARTIST", StdStringToTaglibString(song.albumartist()), true);
  vorbis_comment->removeFields("ALBUM ARTIST");

  vorbis_comment->addField("LYRICS", StdStringToTaglibString(song.lyrics()), true);
  vorbis_comment->removeFields("UNSYNCEDLYRICS");

}

bool TagReaderTagLib::SaveFile(const spb::tagreader::SaveFileRequest &request) const {

  if (request.filename().empty()) return false;

  const QString filename = QString::fromUtf8(request.filename().data(), static_cast<qint64>(request.filename().size()));
  const spb::tagreader::SongMetadata song = request.metadata();
  const bool save_tags = request.has_save_tags() && request.save_tags();
  const bool save_playcount = request.has_save_playcount() && request.save_playcount();
  const bool save_rating = request.has_save_rating() && request.save_rating();
  const bool save_cover = request.has_save_cover() && request.save_cover();

  QStringList save_tags_options;
  if (save_tags) {
    save_tags_options << "tags";
  }
  if (save_playcount) {
    save_tags_options << "playcount";
  }
  if (save_rating) {
    save_tags_options << "rating";
  }
  if (save_cover) {
    save_tags_options << "embedded cover";
  }

  qLog(Debug) << "Saving" << save_tags_options.join(", ") << "to" << filename;

  const Cover cover = LoadCoverFromRequest(request);

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) return false;

  if (save_tags) {
    fileref->tag()->setTitle(song.title().empty() ? TagLib::String() : StdStringToTaglibString(song.title()));
    fileref->tag()->setArtist(song.artist().empty() ? TagLib::String() : StdStringToTaglibString(song.artist()));
    fileref->tag()->setAlbum(song.album().empty() ? TagLib::String() : StdStringToTaglibString(song.album()));
    fileref->tag()->setGenre(song.genre().empty() ? TagLib::String() : StdStringToTaglibString(song.genre()));
    fileref->tag()->setComment(song.comment().empty() ? TagLib::String() : StdStringToTaglibString(song.comment()));
    fileref->tag()->setYear(song.year() <= 0 ? 0 : song.year());
    fileref->tag()->setTrack(song.track() <= 0 ? 0 : song.track());
  }

  bool is_flac = false;
  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    is_flac = true;
    TagLib::Ogg::XiphComment *xiph_comment = file_flac->xiphComment(true);
    if (!xiph_comment) return false;
    if (save_tags) {
      SetVorbisComments(xiph_comment, song);
    }
    if (save_playcount) {
      SetPlaycount(xiph_comment, song);
    }
    if (save_rating) {
      SetRating(xiph_comment, song);
    }
    if (save_cover) {
      SetEmbeddedArt(file_flac, xiph_comment, cover.data, cover.mime_type);
    }
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_wavpack->APETag(true);
    if (!tag) return false;
    if (save_tags) {
      SaveAPETag(tag, song);
    }
    if (save_playcount) {
      SetPlaycount(tag, song);
    }
    if (save_rating) {
      SetRating(tag, song);
    }
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_ape->APETag(true);
    if (!tag) return false;
    if (save_tags) {
      SaveAPETag(tag, song);
    }
    if (save_playcount) {
      SetPlaycount(tag, song);
    }
    if (save_rating) {
      SetRating(tag, song);
    }
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_mpc->APETag(true);
    if (!tag) return false;
    if (save_tags) {
      SaveAPETag(tag, song);
    }
    if (save_playcount) {
      SetPlaycount(tag, song);
    }
    if (save_rating) {
      SetRating(tag, song);
    }
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mpeg->ID3v2Tag(true);
    if (!tag) return false;
    if (save_tags) {
      SetTextFrame("TPOS", song.disc() <= 0 ? QString() : QString::number(song.disc()), tag);
      SetTextFrame("TCOM", song.composer().empty() ? std::string() : song.composer(), tag);
      SetTextFrame("TIT1", song.grouping().empty() ? std::string() : song.grouping(), tag);
      SetTextFrame("TOPE", song.performer().empty() ? std::string() : song.performer(), tag);
      // Skip TPE1 (which is the artist) here because we already set it
      SetTextFrame("TPE2", song.albumartist().empty() ? std::string() : song.albumartist(), tag);
      SetTextFrame("TCMP", song.compilation() ? QString::number(1) : QString(), tag);
      SetUnsyncLyricsFrame(song.lyrics().empty() ? std::string() : song.lyrics(), tag);
    }
    if (save_playcount) {
      SetPlaycount(tag, song);
    }
    if (save_rating) {
      SetRating(tag, song);
    }
    if (save_cover) {
      SetEmbeddedArt(file_mpeg, tag, cover.data, cover.mime_type);
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_mp4->tag();
    if (!tag) return false;
    if (save_tags) {
      tag->setItem("disk", TagLib::MP4::Item(song.disc() <= 0 - 1 ? 0 : song.disc(), 0));
      tag->setItem("\251wrt", TagLib::StringList(TagLib::String(song.composer(), TagLib::String::UTF8)));
      tag->setItem("\251grp", TagLib::StringList(TagLib::String(song.grouping(), TagLib::String::UTF8)));
      tag->setItem("\251lyr", TagLib::StringList(TagLib::String(song.lyrics(), TagLib::String::UTF8)));
      tag->setItem("aART", TagLib::StringList(TagLib::String(song.albumartist(), TagLib::String::UTF8)));
      tag->setItem("cpil", TagLib::MP4::Item(song.compilation()));
    }
    if (save_playcount) {
      SetPlaycount(tag, song);
    }
    if (save_rating) {
      SetRating(tag, song);
    }
    if (save_cover) {
      SetEmbeddedArt(file_mp4, tag, cover.data, cover.mime_type);
    }
  }

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block above.
  if (!is_flac) {
    if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
      if (save_tags) {
        SetVorbisComments(xiph_comment, song);
      }
      if (save_playcount) {
        SetPlaycount(xiph_comment, song);
      }
      if (save_rating) {
        SetRating(xiph_comment, song);
      }
      if (save_cover) {
        SetEmbeddedArt(xiph_comment, cover.data, cover.mime_type);
      }
    }
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success;

}

void TagReaderTagLib::SaveAPETag(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  tag->setItem("album artist", TagLib::APE::Item("album artist", TagLib::StringList(song.albumartist().c_str())));
  tag->addValue("disc", QStringToTaglibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  tag->setItem("composer", TagLib::APE::Item("composer", TagLib::StringList(song.composer().c_str())));
  tag->setItem("grouping", TagLib::APE::Item("grouping", TagLib::StringList(song.grouping().c_str())));
  tag->setItem("performer", TagLib::APE::Item("performer", TagLib::StringList(song.performer().c_str())));
  tag->setItem("lyrics", TagLib::APE::Item("lyrics", TagLib::String(song.lyrics())));
  tag->addValue("compilation", QStringToTaglibString(song.compilation() ? QString::number(1) : QString()), true);

}

void TagReaderTagLib::SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray utf8(value.toUtf8());
  SetTextFrame(id, std::string(utf8.constData(), utf8.length()), tag);

}

void TagReaderTagLib::SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const {

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
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

void TagReaderTagLib::SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray descr_utf8(description.toUtf8());
  const QByteArray value_utf8(value.toUtf8());
  SetUserTextFrame(std::string(descr_utf8.constData(), descr_utf8.length()), std::string(value_utf8.constData(), value_utf8.length()), tag);

}

void TagReaderTagLib::SetUserTextFrame(const std::string &description, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::String t_description = StdStringToTaglibString(description);
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

void TagReaderTagLib::SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const {

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
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTaglibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

QByteArray TagReaderTagLib::LoadEmbeddedArt(const QString &filename) const {

  if (filename.isEmpty()) return QByteArray();

  qLog(Debug) << "Loading art from" << filename;

#ifdef Q_OS_WIN32
  TagLib::FileRef fileref(filename.toStdWString().c_str());
#else
  TagLib::FileRef fileref(QFile::encodeName(filename).constData());
#endif

  if (fileref.isNull() || !fileref.file()) return QByteArray();

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref.file())) {
    if (flac_file->xiphComment()) {
      TagLib::List<TagLib::FLAC::Picture*> pictures = flac_file->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
            QByteArray data(picture->data().data(), picture->data().size());
            if (!data.isEmpty()) {
              return data;
            }
          }
        }
      }
    }
  }

  // WavPack
  if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref.file())) {
    if (wavpack_file->APETag()) {
      return LoadEmbeddedAPEArt(wavpack_file->APETag()->itemListMap());
    }
  }

  // APE
  if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref.file())) {
    if (ape_file->APETag()) {
      return LoadEmbeddedAPEArt(ape_file->APETag()->itemListMap());
    }
  }

  // MPC
  if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref.file())) {
    if (mpc_file->APETag()) {
      return LoadEmbeddedAPEArt(mpc_file->APETag()->itemListMap());
    }
  }

  // Ogg Vorbis / Opus / Speex
  if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref.file()->tag())) {
    TagLib::Ogg::FieldListMap map = xiph_comment->fieldListMap();

    TagLib::List<TagLib::FLAC::Picture*> pictures = xiph_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
          QByteArray data(picture->data().data(), picture->data().size());
          if (!data.isEmpty()) {
            return data;
          }
        }
      }
    }

    // Ogg lacks a definitive standard for embedding cover art, but it seems b64 encoding a field called COVERART is the general convention
    if (map.contains("COVERART")) {
      return QByteArray::fromBase64(map["COVERART"].toString().toCString());
    }

    return QByteArray();
  }

  // MP3
  if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref.file())) {
    if (file_mp3->ID3v2Tag()) {
      TagLib::ID3v2::FrameList apic_frames = file_mp3->ID3v2Tag()->frameListMap()["APIC"];
      if (apic_frames.isEmpty()) {
        return QByteArray();
      }

      TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(apic_frames.front());

      return QByteArray(reinterpret_cast<const char*>(picture->picture().data()), picture->picture().size());
    }
  }

  // MP4/AAC
  if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref.file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag && tag->item("covr").isValid()) {
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

QByteArray TagReaderTagLib::LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const {

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

void TagReaderTagLib::SetEmbeddedArt(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const {

  (void)xiph_comment;

  flac_file->removePictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    flac_file->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const {

  xiph_comment->removeAllPictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    xiph_comment->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::MPEG::File *file_mp3, TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  (void)file_mp3;

  // Remove existing covers
  TagLib::ID3v2::FrameList apiclist = tag->frameListMap()["APIC"];
  for (TagLib::ID3v2::FrameList::ConstIterator it = apiclist.begin(); it != apiclist.end(); ++it) {
    TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
    tag->removeFrame(frame, false);
  }

  if (!data.isEmpty()) {
    // Add new cover
    TagLib::ID3v2::AttachedPictureFrame *frontcover = nullptr;
    frontcover = new TagLib::ID3v2::AttachedPictureFrame("APIC");
    frontcover->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    frontcover->setMimeType(QStringToTString(mime_type));
    frontcover->setPicture(TagLib::ByteVector(data.constData(), data.size()));
    tag->addFrame(frontcover);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  (void)aac_file;

  TagLib::MP4::CoverArtList covers;
  if (data.isEmpty()) {
    if (tag->contains("covr")) tag->removeItem("covr");
  }
  else {
    TagLib::MP4::CoverArt::Format cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    if (mime_type == "image/jpeg") {
      cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    }
    else if (mime_type == "image/png") {
      cover_format = TagLib::MP4::CoverArt::Format::PNG;
    }
    else {
      return;
    }
    covers.append(TagLib::MP4::CoverArt(cover_format, TagLib::ByteVector(data.constData(), data.size())));
    tag->setItem("covr", covers);
  }

}

bool TagReaderTagLib::SaveEmbeddedArt(const spb::tagreader::SaveEmbeddedArtRequest &request) const {

  if (request.filename().empty()) return false;

  const QString filename = QString::fromUtf8(request.filename().data(), static_cast<qint64>(request.filename().size()));

  qLog(Debug) << "Saving art to" << filename;

  const Cover cover = LoadCoverFromRequest(request);

#ifdef Q_OS_WIN32
  TagLib::FileRef fileref(filename.toStdWString().c_str());
#else
  TagLib::FileRef fileref(QFile::encodeName(filename).constData());
#endif

  if (fileref.isNull() || !fileref.file()) return false;

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref.file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (!xiph_comment) return false;
    SetEmbeddedArt(flac_file, xiph_comment, cover.data, cover.mime_type);
  }

  // Ogg Vorbis / Opus / Speex
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref.file()->tag())) {
    SetEmbeddedArt(xiph_comment, cover.data, cover.mime_type);
  }

  // MP3
  else if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref.file())) {
    TagLib::ID3v2::Tag *tag = file_mp3->ID3v2Tag();
    if (!tag) return false;
    SetEmbeddedArt(file_mp3, tag, cover.data, cover.mime_type);
  }

  // MP4/AAC
  else if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref.file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (!tag) return false;
    SetEmbeddedArt(aac_file, tag, cover.data, cover.mime_type);
  }

  // Not supported.
  else return false;

  const bool success = fileref.file()->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success;

}

TagLib::ID3v2::PopularimeterFrame *TagReaderTagLib::GetPOPMFrameFromTag(TagLib::ID3v2::Tag *tag) {

  TagLib::ID3v2::PopularimeterFrame *frame = nullptr;

  const TagLib::ID3v2::FrameListMap &map = tag->frameListMap();
  if (map.contains("POPM")) {
    frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
  }

  if (!frame) {
    frame = new TagLib::ID3v2::PopularimeterFrame();
    tag->addFrame(frame);
  }

  return frame;

}

void TagReaderTagLib::SetPlaycount(TagLib::Ogg::XiphComment *xiph_comment, const spb::tagreader::SongMetadata &song) const {

  if (song.playcount() > 0) {
    xiph_comment->addField("FMPS_PLAYCOUNT", TagLib::String::number(static_cast<int>(song.playcount())), true);
  }
  else {
    xiph_comment->removeFields("FMPS_PLAYCOUNT");
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  if (song.playcount() > 0) {
    tag->setItem("FMPS_Playcount", TagLib::APE::Item("FMPS_Playcount", TagLib::String::number(static_cast<int>(song.playcount()))));
  }
  else {
    tag->removeItem("FMPS_Playcount");
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  SetUserTextFrame("FMPS_Playcount", QString::number(song.playcount()), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setCounter(song.playcount());
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::MP4::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  if (song.playcount() > 0) {
    tag->setItem(kMP4_FMPS_Playcount_ID, TagLib::MP4::Item(TagLib::String::number(static_cast<int>(song.playcount()))));
  }
  else {
    tag->removeItem(kMP4_FMPS_Playcount_ID);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::ASF::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  if (song.playcount() > 0) {
    tag->setAttribute("FMPS/Playcount", TagLib::ASF::Attribute(QStringToTaglibString(QString::number(song.playcount()))));
  }
  else {
    tag->removeItem("FMPS/Playcount");
  }

}

bool TagReaderTagLib::SaveSongPlaycountToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const {

  if (filename.isEmpty()) return false;

  qLog(Debug) << "Saving song playcount to" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) return false;

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (!xiph_comment) return false;
    SetPlaycount(xiph_comment, song);
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (!tag) return false;
    SetPlaycount(tag, song);
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (!tag) return false;
    SetPlaycount(tag, song);
  }
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetPlaycount(xiph_comment, song);
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (!tag) return false;
    SetPlaycount(tag, song);
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (!tag) return false;
    SetPlaycount(tag, song);
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (!tag) return false;
    SetPlaycount(tag, song);
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (!tag) return false;
    if (song.playcount() > 0) {
      tag->addAttribute("FMPS/Playcount", TagLib::ASF::Attribute(QStringToTaglibString(QString::number(song.playcount()))));
    }
  }
  else {
    return true;
  }

  bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success;

}

void TagReaderTagLib::SetRating(TagLib::Ogg::XiphComment *xiph_comment, const spb::tagreader::SongMetadata &song) const {

  if (song.rating() > 0.0F) {
    xiph_comment->addField("FMPS_RATING", QStringToTaglibString(QString::number(song.rating())), true);
  }
  else {
    xiph_comment->removeFields("FMPS_RATING");
  }

}

void TagReaderTagLib::SetRating(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  if (song.rating() > 0.0F) {
    tag->setItem("FMPS_Rating", TagLib::APE::Item("FMPS_Rating", TagLib::StringList(QStringToTaglibString(QString::number(song.rating())))));
  }
  else {
    tag->removeItem("FMPS_Rating");
  }

}

void TagReaderTagLib::SetRating(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  SetUserTextFrame("FMPS_Rating", QString::number(song.rating()), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setRating(ConvertToPOPMRating(song.rating()));
  }

}

void TagReaderTagLib::SetRating(TagLib::MP4::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  tag->setItem(kMP4_FMPS_Rating_ID, TagLib::StringList(QStringToTaglibString(QString::number(song.rating()))));

}

void TagReaderTagLib::SetRating(TagLib::ASF::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  tag->addAttribute("FMPS/Rating", TagLib::ASF::Attribute(QStringToTaglibString(QString::number(song.rating()))));

}

bool TagReaderTagLib::SaveSongRatingToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const {

  if (filename.isNull()) return false;

  qLog(Debug) << "Saving song rating to" << filename;

  if (song.rating() < 0) {
    return true;
  }

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));

  if (!fileref || fileref->isNull()) return false;

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (!xiph_comment) return false;
    SetRating(xiph_comment, song);
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (!tag) return false;
    SetRating(tag, song);
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (!tag) return false;
    SetRating(tag, song);
  }
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetRating(xiph_comment, song);
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (!tag) return false;
    SetRating(tag, song);
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (!tag) return false;
    SetRating(tag, song);
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (!tag) return false;
    SetRating(tag, song);
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (!tag) return false;
    SetRating(tag, song);
  }
  else {
    return true;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success;

}
