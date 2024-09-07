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

using namespace Qt::StringLiterals;

#undef TStringToQString
#undef QStringToTString

namespace {

constexpr char kID3v2_AlbumArtist[] = "TPE2";
constexpr char kID3v2_Disc[] = "TPOS";
constexpr char kID3v2_Composer[] = "TCOM";
constexpr char kID3v2_Performer[] = "TOPE";
constexpr char kID3v2_Grouping[] = "TIT1";
constexpr char kID3v2_Compilation[] = "TCMP";
constexpr char kID3v2_OriginalReleaseTime[] = "TDOR";
constexpr char kID3v2_OriginalReleaseYear[] = "TORY";
constexpr char kID3v2_UnsychronizedLyrics[] = "USLT";
constexpr char kID3v2_SynchronizedLyrics[] = "SYLT";
constexpr char kID3v2_CoverArt[] = "APIC";
constexpr char kID3v2_CommercialFrame[] = "COMM";
constexpr char kID3v2_FMPS_Playcount[] = "FMPS_Playcount";
constexpr char kID3v2_FMPS_Rating[] = "FMPS_Rating";
constexpr char kID3v2_Unique_File_Identifier[] = "UFID";
constexpr char kID3v2_UserDefinedTextInformationFrame[] = "TXXX";
constexpr char kID3v2_Popularimeter[] = "POPM";
constexpr char kID3v2_AcoustId[] = "Acoustid Id";
constexpr char kID3v2_AcoustId_Fingerprint[] = "Acoustid Fingerprint";
constexpr char kID3v2_MusicBrainz_AlbumArtistId[] = "MusicBrainz Album Artist Id";
constexpr char kID3v2_MusicBrainz_ArtistId[] = "MusicBrainz Artist Id";
constexpr char kID3v2_MusicBrainz_OriginalArtistId[] = "MusicBrainz Original Artist Id";
constexpr char kID3v2_MusicBrainz_AlbumId[] = "MusicBrainz Album Id";
constexpr char kID3v2_MusicBrainz_OriginalAlbumId[] = "MusicBrainz Original Album Id";
constexpr char kID3v2_MusicBrainz_RecordingId[] = "MUSICBRAINZ_TRACKID";
constexpr char kID3v2_MusicBrainz_TrackId[] = "MusicBrainz Release Track Id";
constexpr char kID3v2_MusicBrainz_DiscId[] = "MusicBrainz Disc Id";
constexpr char kID3v2_MusicBrainz_ReleaseGroupId[] = "MusicBrainz Release Group Id";
constexpr char kID3v2_MusicBrainz_WorkId[] = "MusicBrainz Work Id";

constexpr char kVorbisComment_AlbumArtist1[] = "ALBUMARTIST";
constexpr char kVorbisComment_AlbumArtist2[] = "ALBUM ARTIST";
constexpr char kVorbisComment_Composer[] = "COMPOSER";
constexpr char kVorbisComment_Performer[] = "PERFORMER";
constexpr char kVorbisComment_Grouping1[] = "GROUPING";
constexpr char kVorbisComment_Grouping2[] = "CONTENT GROUP";
constexpr char kVorbisComment_OriginalYear1[] = "ORIGINALDATE";
constexpr char kVorbisComment_OriginalYear2[] = "ORIGINALYEAR";
constexpr char kVorbisComment_Disc[] = "DISCNUMBER";
constexpr char kVorbisComment_Compilation[] = "COMPILATION";
constexpr char kVorbisComment_CoverArt[] = "COVERART";
constexpr char kVorbisComment_MetadataBlockPicture[] = "METADATA_BLOCK_PICTURE";
constexpr char kVorbisComment_FMPS_Playcount[] = "FMPS_PLAYCOUNT";
constexpr char kVorbisComment_FMPS_Rating[] = "FMPS_RATING";
constexpr char kVorbisComment_Lyrics[] = "LYRICS";
constexpr char kVorbisComment_UnsyncedLyrics[] = "UNSYNCEDLYRICS";
constexpr char kVorbisComment_AcoustId[] = "ACOUSTID_ID";
constexpr char kVorbisComment_AcoustId_Fingerprint[] = "ACOUSTID_FINGERPRINT";
constexpr char kVorbisComment_MusicBrainz_AlbumArtistId[] = "MUSICBRAINZ_ALBUMARTISTID";
constexpr char kVorbisComment_MusicBrainz_ArtistId[] = "MUSICBRAINZ_ARTISTID";
constexpr char kVorbisComment_MusicBrainz_OriginalArtistId[] = "MUSICBRAINZ_ORIGINALARTISTID";
constexpr char kVorbisComment_MusicBrainz_AlbumId[] = "MUSICBRAINZ_ALBUMID";
constexpr char kVorbisComment_MusicBrainz_OriginalAlbumId[] = "MUSICBRAINZ_ORIGINALALBUMID";
constexpr char kVorbisComment_MusicBrainz_TackId[] = "MUSICBRAINZ_TRACKID";
constexpr char kVorbisComment_MusicBrainz_ReleaseTrackId[] = "MUSICBRAINZ_RELEASETRACKID";
constexpr char kVorbisComment_MusicBrainz_DiscId[] = "MUSICBRAINZ_DISCID";
constexpr char kVorbisComment_MusicBrainz_ReleaseGroupId[] = "MUSICBRAINZ_RELEASEGROUPID";
constexpr char kVorbisComment_MusicBrainz_WorkId[] = "MUSICBRAINZ_WORKID";

constexpr char kMP4_AlbumArtist[] = "aART";
constexpr char kMP4_Composer[] = "\251wrt";
constexpr char kMP4_Grouping[] = "\251grp";
constexpr char kMP4_Lyrics[] = "\251lyr";
constexpr char kMP4_Disc[] = "disk";
constexpr char kMP4_Compilation[] = "cpil";
constexpr char kMP4_CoverArt[] = "covr";
constexpr char kMP4_OriginalYear[] = "----:com.apple.iTunes:ORIGINAL YEAR";
constexpr char kMP4_FMPS_Playcount[] = "----:com.apple.iTunes:FMPS_Playcount";
constexpr char kMP4_FMPS_Rating[] = "----:com.apple.iTunes:FMPS_Rating";
constexpr char kMP4_AcoustId[] = "----:com.apple.iTunes:Acoustid Id";
constexpr char kMP4_AcoustId_Fingerprint[] = "----:com.apple.iTunes:Acoustid Fingerprint";
constexpr char kMP4_MusicBrainz_AlbumArtistId[] = "----:com.apple.iTunes:MusicBrainz Album Artist Id";
constexpr char kMP4_MusicBrainz_ArtistId[] = "----:com.apple.iTunes:MusicBrainz Artist Id";
constexpr char kMP4_MusicBrainz_OriginalArtistId[] = "----:com.apple.iTunes:MusicBrainz Original Artist Id";
constexpr char kMP4_MusicBrainz_AlbumId[] = "----:com.apple.iTunes:MusicBrainz Album Id";
constexpr char kMP4_MusicBrainz_OriginalAlbumId[] = "----:com.apple.iTunes:MusicBrainz Original Album Id";
constexpr char kMP4_MusicBrainz_RecordingId[] = "----:com.apple.iTunes:MusicBrainz Track Id";
constexpr char kMP4_MusicBrainz_TrackId[] = "----:com.apple.iTunes:MusicBrainz Release Track Id";
constexpr char kMP4_MusicBrainz_DiscId[] = "----:com.apple.iTunes:MusicBrainz Disc Id";
constexpr char kMP4_MusicBrainz_ReleaseGroupId[] = "----:com.apple.iTunes:MusicBrainz Release Group Id";
constexpr char kMP4_MusicBrainz_WorkId[] = "----:com.apple.iTunes:MusicBrainz Work Id";

constexpr char kAPE_AlbumArtist[] = "ALBUM ARTIST";
constexpr char kAPE_Composer[] = "COMPOSER";
constexpr char kAPE_Performer[] = "PERFORMER";
constexpr char kAPE_Grouping[] = "GROUPING";
constexpr char kAPE_Disc[] = "DISC";
constexpr char kAPE_Compilation[] = "COMPILATION";
constexpr char kAPE_CoverArt[] = "COVER ART (FRONT)";
constexpr char kAPE_FMPS_Playcount[] = "FMPS_PLAYCOUNT";
constexpr char kAPE_FMPS_Rating[] = "FMPS_RATING";
constexpr char kAPE_Lyrics[] = "LYRICS";
constexpr char kAPE_AcoustId[] = "ACOUSTID_ID";
constexpr char kAPE_AcoustId_Fingerprint[] = "ACOUSTID_FINGERPRINT";
constexpr char kAPE_MusicBrainz_AlbumArtistId[] = "MUSICBRAINZ_ALBUMARTISTID";
constexpr char kAPE_MusicBrainz_ArtistId[] = "MUSICBRAINZ_ARTISTID";
constexpr char kAPE_MusicBrainz_OriginalArtistId[] = "MUSICBRAINZ_ORIGINALARTISTID";
constexpr char kAPE_MusicBrainz_AlbumId[] = "MUSICBRAINZ_ALBUMID";
constexpr char kAPE_MusicBrainz_OriginalAlbumId[] = "MUSICBRAINZ_ORIGINALALBUMID";
constexpr char kAPE_MusicBrainz_TackId[] = "MUSICBRAINZ_TRACKID";
constexpr char kAPE_MusicBrainz_ReleaseTrackId[] = "MUSICBRAINZ_RELEASETRACKID";
constexpr char kAPE_MusicBrainz_DiscId[] = "MUSICBRAINZ_DISCID";
constexpr char kAPE_MusicBrainz_ReleaseGroupId[] = "MUSICBRAINZ_RELEASEGROUPID";
constexpr char kAPE_MusicBrainz_WorkId[] = "MUSICBRAINZ_WORKID";

constexpr char kASF_AlbumArtist[] = "WM/AlbumArtist";
constexpr char kASF_Composer[] = "WM/Composer";
constexpr char kASF_Lyrics[] = "WM/Lyrics";
constexpr char kASF_Disc[] = "WM/PartOfSet";
constexpr char kASF_OriginalDate[] = "WM/OriginalReleaseTime";
constexpr char kASF_OriginalYear[] = "WM/OriginalReleaseYear";
constexpr char kASF_FMPS_Playcount[] = "FMPS/Playcount";
constexpr char kASF_FMPS_Rating[] = "FMPS/Rating";
constexpr char kASF_AcoustId[] = "Acoustid/Id";
constexpr char kASF_AcoustId_Fingerprint[] = "Acoustid/Fingerprint";
constexpr char kASF_MusicBrainz_AlbumArtistId[] = "MusicBrainz/Album Artist Id";
constexpr char kASF_MusicBrainz_ArtistId[] = "MusicBrainz/Artist Id";
constexpr char kASF_MusicBrainz_OriginalArtistId[] = "MusicBrainz/Original Artist Id";
constexpr char kASF_MusicBrainz_AlbumId[] = "MusicBrainz/Album Id";
constexpr char kASF_MusicBrainz_OriginalAlbumId[] = "MusicBrainz/Original Album Id";
constexpr char kASF_MusicBrainz_RecordingId[] = "MusicBrainz/Track Id";
constexpr char kASF_MusicBrainz_TrackId[] = "MusicBrainz/Release Track Id";
constexpr char kASF_MusicBrainz_DiscId[] = "MusicBrainz/Disc Id";
constexpr char kASF_MusicBrainz_ReleaseGroupId[] = "MusicBrainz/Release Group Id";
constexpr char kASF_MusicBrainz_WorkId[] = "MusicBrainz/Work Id";

}  // namespace

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

TagReaderBase::Result TagReaderTagLib::ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Reading tags from" << filename;

  const QFileInfo fileinfo(filename);
  if (!fileinfo.exists()) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const QByteArray url = QUrl::fromLocalFile(filename).toEncoded();
  const QByteArray basefilename = fileinfo.fileName().toUtf8();
  song->set_basefilename(basefilename.constData(), basefilename.length());
  song->set_url(url.constData(), url.size());
  song->set_filesize(fileinfo.size());
  song->set_mtime(fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  song->set_ctime(fileinfo.birthTime().isValid() ? std::max(fileinfo.birthTime().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  if (song->ctime() <= 0) {
    song->set_ctime(song->mtime());
  }
  song->set_lastseen(QDateTime::currentSecsSinceEpoch());

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  song->set_filetype(GuessFileType(fileref.get()));

  if (fileref->audioProperties()) {
    song->set_bitrate(fileref->audioProperties()->bitrate());
    song->set_samplerate(fileref->audioProperties()->sampleRate());
    song->set_length_nanosec(fileref->audioProperties()->lengthInMilliseconds() * kNsecPerMsec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    AssignTagLibStringToStdString(tag->title(), song->mutable_title());
    AssignTagLibStringToStdString(tag->artist(), song->mutable_artist());  // TPE1
    AssignTagLibStringToStdString(tag->album(), song->mutable_album());
    AssignTagLibStringToStdString(tag->genre(), song->mutable_genre());
    song->set_year(static_cast<int>(tag->year()));
    song->set_track(static_cast<int>(tag->track()));
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseVorbisComments(vorbis_comment->fieldListMap(), &disc, &compilation, song);
    TagLib::List<TagLib::FLAC::Picture*> pictures = vorbis_comment->pictureList();
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
      ParseVorbisComments(file_flac->xiphComment()->fieldListMap(), &disc, &compilation, song);
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
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    song->set_bitdepth(file_wavpack->audioProperties()->bitsPerSample());
    if (file_wavpack->APETag()) {
      ParseAPETags(file_wavpack->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    song->set_bitdepth(file_ape->audioProperties()->bitsPerSample());
    if (file_ape->APETag()) {
      ParseAPETags(file_ape->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    if (file_mpeg->hasID3v2Tag()) {
      ParseID3v2Tags(file_mpeg->ID3v2Tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    song->set_bitdepth(file_mp4->audioProperties()->bitsPerSample());
    if (file_mp4->tag()) {
      ParseMP4Tags(file_mp4->tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::ASF::File *file_asf = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    song->set_bitdepth(file_asf->audioProperties()->bitsPerSample());
    if (file_asf->tag()) {
      AssignTagLibStringToStdString(file_asf->tag()->comment(), song->mutable_comment());
      ParseASFTags(file_asf->tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      ParseAPETags(file_mpc->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    if (file_wav->hasID3v2Tag()) {
      ParseID3v2Tags(file_wav->ID3v2Tag(), &disc, &compilation, song);
    }
  }

  else if (tag) {
    AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  if (!disc.isEmpty()) {
    const qint64 i = disc.indexOf(u'/');
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
    const QString albumartist = QString::fromStdString(song->albumartist());
    const QString artist = QString::fromStdString(song->artist());
    if (artist.compare("various artists"_L1) == 0 || albumartist.compare("various artists"_L1) == 0) {
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

  if (song->filetype() == spb::tagreader::SongMetadata_FileType_UNKNOWN) {
    qLog(Error) << "Unknown audio filetype reading" << filename;
    return Result::ErrorCode::Unsupported;
  }

  qLog(Debug) << "Got tags for" << filename;

  return Result::ErrorCode::Success;

}

void TagReaderTagLib::ParseID3v2Tags(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  TagLib::ID3v2::FrameListMap map = tag->frameListMap();

  if (map.contains(kID3v2_Disc)) *disc = TagLibStringToQString(map[kID3v2_Disc].front()->toString()).trimmed();
  if (map.contains(kID3v2_Composer)) AssignTagLibStringToStdString(map[kID3v2_Composer].front()->toString(), song->mutable_composer());

  // content group
  if (map.contains(kID3v2_Grouping)) AssignTagLibStringToStdString(map[kID3v2_Grouping].front()->toString(), song->mutable_grouping());

  // original artist/performer
  if (map.contains(kID3v2_Performer)) AssignTagLibStringToStdString(map[kID3v2_Performer].front()->toString(), song->mutable_performer());

  // Skip TPE1 (which is the artist) here because we already fetched it

  // non-standard: Apple, Microsoft
  if (map.contains(kID3v2_AlbumArtist)) AssignTagLibStringToStdString(map[kID3v2_AlbumArtist].front()->toString(), song->mutable_albumartist());

  if (map.contains(kID3v2_Compilation)) *compilation = TagLibStringToQString(map[kID3v2_Compilation].front()->toString()).trimmed();

  if (map.contains(kID3v2_OriginalReleaseTime)) {
    song->set_originalyear(map[kID3v2_OriginalReleaseTime].front()->toString().substr(0, 4).toInt());
  }
  else if (map.contains(kID3v2_OriginalReleaseYear)) {
    song->set_originalyear(map[kID3v2_OriginalReleaseYear].front()->toString().substr(0, 4).toInt());
  }

  if (map.contains(kID3v2_UnsychronizedLyrics)) {
    AssignTagLibStringToStdString(map[kID3v2_UnsychronizedLyrics].front()->toString(), song->mutable_lyrics());
  }
  else if (map.contains(kID3v2_SynchronizedLyrics)) {
    AssignTagLibStringToStdString(map[kID3v2_SynchronizedLyrics].front()->toString(), song->mutable_lyrics());
  }

  if (map.contains(kID3v2_CoverArt)) song->set_art_embedded(true);

  // Find a suitable comment tag.  For now we ignore iTunNORM comments.
  for (uint i = 0; i < map[kID3v2_CommercialFrame].size(); ++i) {
    const TagLib::ID3v2::CommentsFrame *frame = dynamic_cast<const TagLib::ID3v2::CommentsFrame*>(map[kID3v2_CommercialFrame][i]);

    if (frame && TagLibStringToQString(frame->description()) != "iTunNORM"_L1) {
      AssignTagLibStringToStdString(frame->text(), song->mutable_comment());
      break;
    }
  }

  if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_playcount = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, kID3v2_FMPS_Playcount)) {
    TagLib::StringList frame_field_list = frame_fmps_playcount->fieldList();
    if (frame_field_list.size() > 1) {
      const int playcount = TagLibStringToQString(frame_field_list[1]).toInt();
      if (song->playcount() <= 0 && playcount > 0) {
        song->set_playcount(playcount);
      }
    }
  }

  if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_rating = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, kID3v2_FMPS_Rating)) {
    TagLib::StringList frame_field_list = frame_fmps_rating->fieldList();
    if (frame_field_list.size() > 1) {
      const float rating = TagLibStringToQString(frame_field_list[1]).toFloat();
      if (song->rating() <= 0 && rating > 0 && rating <= 1.0) {
        song->set_rating(rating);
      }
    }
  }

  if (map.contains(kID3v2_Popularimeter)) {
    const TagLib::ID3v2::PopularimeterFrame *frame = dynamic_cast<const TagLib::ID3v2::PopularimeterFrame*>(map[kID3v2_Popularimeter].front());
    if (frame) {
      if (song->playcount() <= 0 && frame->counter() > 0) {
        song->set_playcount(frame->counter());
      }
      if (song->rating() <= 0 && frame->rating() > 0) {
        song->set_rating(ConvertPOPMRating(frame->rating()));
      }
    }
  }

  if (map.contains(kID3v2_Unique_File_Identifier)) {
    for (uint i = 0; i < map[kID3v2_Unique_File_Identifier].size(); ++i) {
      if (TagLib::ID3v2::UniqueFileIdentifierFrame *frame = dynamic_cast<TagLib::ID3v2::UniqueFileIdentifierFrame*>(map[kID3v2_Unique_File_Identifier][i])) {
        const TagLib::PropertyMap property_map = frame->asProperties();
        if (property_map.contains(kID3v2_MusicBrainz_RecordingId)) {
          AssignTagLibStringToStdString(property_map[kID3v2_MusicBrainz_RecordingId].toString(), song->mutable_musicbrainz_recording_id());
        }
      }
    }
  }

  if (map.contains(kID3v2_UserDefinedTextInformationFrame)) {
    for (uint i = 0; i < map[kID3v2_UserDefinedTextInformationFrame].size(); ++i) {
      if (TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(map[kID3v2_UserDefinedTextInformationFrame][i])) {
        const TagLib::StringList frame_field_list = frame->fieldList();
        if (frame_field_list.size() != 2) continue;
        if (frame->description() == kID3v2_AcoustId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_acoustid_id());
        }
        if (frame->description() == kID3v2_AcoustId_Fingerprint) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_acoustid_fingerprint());
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumArtistId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_ArtistId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalArtistId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalAlbumId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_album_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_TrackId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_track_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_DiscId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_disc_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_ReleaseGroupId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_release_group_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_WorkId) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_work_id());
        }
      }
    }
  }

}

void TagReaderTagLib::ParseVorbisComments(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  if (map.contains(kVorbisComment_Composer)) AssignTagLibStringToStdString(map[kVorbisComment_Composer].front(), song->mutable_composer());
  if (map.contains(kVorbisComment_Performer)) AssignTagLibStringToStdString(map[kVorbisComment_Performer].front(), song->mutable_performer());
  if (map.contains(kVorbisComment_Grouping2)) AssignTagLibStringToStdString(map[kVorbisComment_Grouping2].front(), song->mutable_grouping());
  if (map.contains(kVorbisComment_Grouping1)) AssignTagLibStringToStdString(map[kVorbisComment_Grouping1].front(), song->mutable_grouping());

  if (map.contains(kVorbisComment_AlbumArtist1)) AssignTagLibStringToStdString(map[kVorbisComment_AlbumArtist1].front(), song->mutable_albumartist());
  else if (map.contains(kVorbisComment_AlbumArtist2)) AssignTagLibStringToStdString(map[kVorbisComment_AlbumArtist2].front(), song->mutable_albumartist());

  if (map.contains(kVorbisComment_OriginalYear1)) song->set_originalyear(TagLibStringToQString(map[kVorbisComment_OriginalYear1].front()).left(4).toInt());
  else if (map.contains(kVorbisComment_OriginalYear2)) song->set_originalyear(TagLibStringToQString(map[kVorbisComment_OriginalYear2].front()).toInt());

  if (map.contains(kVorbisComment_Disc)) *disc = TagLibStringToQString(map[kVorbisComment_Disc].front()).trimmed();
  if (map.contains(kVorbisComment_Compilation)) *compilation = TagLibStringToQString(map[kVorbisComment_Compilation].front()).trimmed();
  if (map.contains(kVorbisComment_CoverArt) || map.contains(kVorbisComment_MetadataBlockPicture)) song->set_art_embedded(true);

  if (map.contains(kVorbisComment_FMPS_Playcount) && song->playcount() <= 0) {
    const int playcount = TagLibStringToQString(map[kVorbisComment_FMPS_Playcount].front()).trimmed().toInt();
    song->set_playcount(static_cast<uint>(playcount));
  }
  if (map.contains(kVorbisComment_FMPS_Rating) && song->rating() <= 0) song->set_rating(TagLibStringToQString(map[kVorbisComment_FMPS_Rating].front()).trimmed().toFloat());

  if (map.contains(kVorbisComment_Lyrics)) AssignTagLibStringToStdString(map[kVorbisComment_Lyrics].front(), song->mutable_lyrics());
  else if (map.contains(kVorbisComment_UnsyncedLyrics)) AssignTagLibStringToStdString(map[kVorbisComment_UnsyncedLyrics].front(), song->mutable_lyrics());

  if (map.contains(kVorbisComment_AcoustId)) AssignTagLibStringToStdString(map[kVorbisComment_AcoustId].front(), song->mutable_acoustid_id());
  if (map.contains(kVorbisComment_AcoustId_Fingerprint)) AssignTagLibStringToStdString(map[kVorbisComment_AcoustId_Fingerprint].front(), song->mutable_acoustid_fingerprint());

  if (map.contains(kVorbisComment_MusicBrainz_AlbumArtistId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_AlbumArtistId].front(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains(kVorbisComment_MusicBrainz_ArtistId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_ArtistId].front(), song->mutable_musicbrainz_artist_id());
  if (map.contains(kVorbisComment_MusicBrainz_OriginalArtistId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_OriginalArtistId].front(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains(kVorbisComment_MusicBrainz_AlbumId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_AlbumId].front(), song->mutable_musicbrainz_album_id());
  if (map.contains(kVorbisComment_MusicBrainz_OriginalAlbumId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_OriginalAlbumId].front(), song->mutable_musicbrainz_original_album_id());
  if (map.contains(kVorbisComment_MusicBrainz_TackId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_TackId].front(), song->mutable_musicbrainz_recording_id());
  if (map.contains(kVorbisComment_MusicBrainz_ReleaseTrackId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_ReleaseTrackId].front(), song->mutable_musicbrainz_track_id());
  if (map.contains(kVorbisComment_MusicBrainz_DiscId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_DiscId].front(), song->mutable_musicbrainz_disc_id());
  if (map.contains(kVorbisComment_MusicBrainz_ReleaseGroupId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_ReleaseGroupId].front(), song->mutable_musicbrainz_release_group_id());
  if (map.contains(kVorbisComment_MusicBrainz_WorkId)) AssignTagLibStringToStdString(map[kVorbisComment_MusicBrainz_WorkId].front(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::ParseAPETags(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find(kAPE_AlbumArtist);
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.values();
    if (!album_artists.isEmpty()) {
      AssignTagLibStringToStdString(album_artists.front(), song->mutable_albumartist());
    }
  }

  if (map.find(kAPE_CoverArt) != map.end()) song->set_art_embedded(true);
  if (map.contains(kAPE_Compilation)) {
    *compilation = TagLibStringToQString(TagLib::String::number(map[kAPE_Compilation].toString().toInt()));
  }

  if (map.contains(kAPE_Disc)) {
    *disc = TagLibStringToQString(TagLib::String::number(map[kAPE_Disc].toString().toInt()));
  }

  if (map.contains(kAPE_Performer)) {
    AssignTagLibStringToStdString(map[kAPE_Performer].values().toString(", "), song->mutable_performer());
  }

  if (map.contains(kAPE_Composer)) {
    AssignTagLibStringToStdString(map[kAPE_Composer].values().toString(", "), song->mutable_composer());
  }

  if (map.contains(kAPE_Grouping)) {
    AssignTagLibStringToStdString(map[kAPE_Grouping].values().toString(" "), song->mutable_grouping());
  }

  if (map.contains(kAPE_Lyrics)) {
    AssignTagLibStringToStdString(map[kAPE_Lyrics].toString(), song->mutable_lyrics());
  }

  if (map.contains(kAPE_FMPS_Playcount)) {
    const int playcount = TagLibStringToQString(map[kAPE_FMPS_Playcount].toString()).toInt();
    if (song->playcount() <= 0 && playcount > 0) {
      song->set_playcount(static_cast<uint>(playcount));
    }
  }

  if (map.contains(kAPE_FMPS_Rating)) {
    const float rating = TagLibStringToQString(map[kAPE_FMPS_Rating].toString()).toFloat();
    if (song->rating() <= 0 && rating > 0) {
      song->set_rating(rating);
    }
  }

  if (map.contains(kAPE_AcoustId)) AssignTagLibStringToStdString(map[kAPE_AcoustId].toString(), song->mutable_acoustid_id());
  if (map.contains(kAPE_AcoustId_Fingerprint)) AssignTagLibStringToStdString(map[kAPE_AcoustId_Fingerprint].toString(), song->mutable_acoustid_fingerprint());

  if (map.contains(kAPE_MusicBrainz_AlbumArtistId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_AlbumArtistId].toString(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains(kAPE_MusicBrainz_ArtistId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_ArtistId].toString(), song->mutable_musicbrainz_artist_id());
  if (map.contains(kAPE_MusicBrainz_OriginalArtistId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_OriginalArtistId].toString(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains(kAPE_MusicBrainz_AlbumId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_AlbumId].toString(), song->mutable_musicbrainz_album_id());
  if (map.contains(kAPE_MusicBrainz_OriginalAlbumId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_OriginalAlbumId].toString(), song->mutable_musicbrainz_original_album_id());
  if (map.contains(kAPE_MusicBrainz_TackId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_TackId].toString(), song->mutable_musicbrainz_recording_id());
  if (map.contains(kAPE_MusicBrainz_ReleaseTrackId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_ReleaseTrackId].toString(), song->mutable_musicbrainz_track_id());
  if (map.contains(kAPE_MusicBrainz_DiscId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_DiscId].toString(), song->mutable_musicbrainz_disc_id());
  if (map.contains(kAPE_MusicBrainz_ReleaseGroupId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_ReleaseGroupId].toString(), song->mutable_musicbrainz_release_group_id());
  if (map.contains(kAPE_MusicBrainz_WorkId)) AssignTagLibStringToStdString(map[kAPE_MusicBrainz_WorkId].toString(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::ParseMP4Tags(TagLib::MP4::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  Q_UNUSED(compilation);

  // Find album artists
  if (tag->item(kMP4_AlbumArtist).isValid()) {
    const TagLib::StringList album_artists = tag->item(kMP4_AlbumArtist).toStringList();
    if (!album_artists.isEmpty()) {
      AssignTagLibStringToStdString(album_artists.front(), song->mutable_albumartist());
    }
  }

  // Find album cover art
  if (tag->item(kMP4_CoverArt).isValid()) {
    song->set_art_embedded(true);
  }

  if (tag->item(kMP4_Disc).isValid()) {
    *disc = TagLibStringToQString(TagLib::String::number(tag->item(kMP4_Disc).toIntPair().first));
  }

  if (tag->item(kMP4_Composer).isValid()) {
    AssignTagLibStringToStdString(tag->item(kMP4_Composer).toStringList().toString(", "), song->mutable_composer());
  }
  if (tag->item(kMP4_Grouping).isValid()) {
    AssignTagLibStringToStdString(tag->item(kMP4_Grouping).toStringList().toString(" "), song->mutable_grouping());
  }
  if (tag->item(kMP4_Lyrics).isValid()) {
    AssignTagLibStringToStdString(tag->item(kMP4_Lyrics).toStringList().toString(" "), song->mutable_lyrics());
  }

  if (tag->item(kMP4_OriginalYear).isValid()) {
    song->set_originalyear(TagLibStringToQString(tag->item(kMP4_OriginalYear).toStringList().toString('\n')).left(4).toInt());
  }

  if (tag->item(kMP4_Compilation).isValid()) {
    song->set_compilation(tag->item(kMP4_Compilation).toBool());
  }

  {
    const TagLib::MP4::Item item = tag->item(kMP4_FMPS_Playcount);
    if (item.isValid()) {
      const int playcount = TagLibStringToQString(item.toStringList().toString('\n')).toInt();
      if (song->playcount() <= 0 && playcount > 0) {
        song->set_playcount(static_cast<uint>(playcount));
      }
    }
  }

  {
    const TagLib::MP4::Item item = tag->item(kMP4_FMPS_Rating);
    if (item.isValid()) {
      const float rating = TagLibStringToQString(item.toStringList().toString('\n')).toFloat();
      if (song->rating() <= 0 && rating > 0) {
        song->set_rating(rating);
      }
    }
  }

  AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());

  if (tag->contains(kMP4_AcoustId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_AcoustId).toStringList().toString(), song->mutable_acoustid_id());
  }
  if (tag->contains(kMP4_AcoustId_Fingerprint)) {
    AssignTagLibStringToStdString(tag->item(kMP4_AcoustId_Fingerprint).toStringList().toString(), song->mutable_acoustid_fingerprint());
  }

  if (tag->contains(kMP4_MusicBrainz_AlbumArtistId)) {
    AssignTagLibStringToStdString(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_AlbumArtistId).toStringList()), song->mutable_musicbrainz_album_artist_id());
  }
  if (tag->contains(kMP4_MusicBrainz_ArtistId)) {
    AssignTagLibStringToStdString(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_ArtistId).toStringList()), song->mutable_musicbrainz_artist_id());
  }
  if (tag->contains(kMP4_MusicBrainz_OriginalArtistId)) {
    AssignTagLibStringToStdString(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_OriginalArtistId).toStringList()), song->mutable_musicbrainz_original_artist_id());
  }
  if (tag->contains(kMP4_MusicBrainz_AlbumId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_AlbumId).toStringList().toString(), song->mutable_musicbrainz_album_id());
  }
  if (tag->contains(kMP4_MusicBrainz_OriginalAlbumId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_OriginalAlbumId).toStringList().toString(), song->mutable_musicbrainz_original_album_id());
  }
  if (tag->contains(kMP4_MusicBrainz_RecordingId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_RecordingId).toStringList().toString(), song->mutable_musicbrainz_recording_id());
  }
  if (tag->contains(kMP4_MusicBrainz_TrackId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_TrackId).toStringList().toString(), song->mutable_musicbrainz_track_id());
  }
  if (tag->contains(kMP4_MusicBrainz_DiscId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_DiscId).toStringList().toString(), song->mutable_musicbrainz_disc_id());
  }
  if (tag->contains(kMP4_MusicBrainz_ReleaseGroupId)) {
    AssignTagLibStringToStdString(tag->item(kMP4_MusicBrainz_ReleaseGroupId).toStringList().toString(), song->mutable_musicbrainz_release_group_id());
  }
  if (tag->contains(kMP4_MusicBrainz_WorkId)) {
    AssignTagLibStringToStdString(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_WorkId).toStringList()), song->mutable_musicbrainz_work_id());
  }

}
void TagReaderTagLib::ParseASFTags(TagLib::ASF::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  Q_UNUSED(disc);
  Q_UNUSED(compilation);

  const TagLib::ASF::AttributeListMap &attributes_map = tag->attributeListMap();
  if (attributes_map.isEmpty()) return;

  ParseASFAttribute(attributes_map, kASF_AlbumArtist, song->mutable_albumartist());
  ParseASFAttribute(attributes_map, kASF_Composer, song->mutable_composer());
  ParseASFAttribute(attributes_map, kASF_Lyrics, song->mutable_lyrics());
  ParseASFAttribute(attributes_map, kASF_AcoustId, song->mutable_acoustid_id());
  ParseASFAttribute(attributes_map, kASF_AcoustId_Fingerprint, song->mutable_acoustid_fingerprint());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_AlbumArtistId, song->mutable_musicbrainz_album_artist_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_ArtistId, song->mutable_musicbrainz_artist_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_OriginalArtistId, song->mutable_musicbrainz_original_artist_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_AlbumId, song->mutable_musicbrainz_album_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_OriginalAlbumId, song->mutable_musicbrainz_original_album_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_RecordingId, song->mutable_musicbrainz_recording_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_TrackId, song->mutable_musicbrainz_track_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_DiscId, song->mutable_musicbrainz_disc_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_ReleaseGroupId, song->mutable_musicbrainz_release_group_id());
  ParseASFAttribute(attributes_map, kASF_MusicBrainz_WorkId, song->mutable_musicbrainz_work_id());

  if (attributes_map.contains(kASF_Disc)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_Disc];
    if (!attributes.isEmpty()) {
      song->set_disc(TagLibStringToQString(attributes.front().toString()).toInt());
    }
  }

  if (attributes_map.contains(kASF_OriginalDate)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalDate];
    if (!attributes.isEmpty()) {
      song->set_originalyear(TagLibStringToQString(attributes.front().toString()).left(4).toInt());
    }
  }
  else if (attributes_map.contains(kASF_OriginalYear)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalYear];
    if (!attributes.isEmpty()) {
      song->set_originalyear(TagLibStringToQString(attributes.front().toString()).left(4).toInt());
    }
  }

  if (attributes_map.contains(kASF_FMPS_Playcount)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_FMPS_Playcount];
    if (!attributes.isEmpty()) {
      int playcount = TagLibStringToQString(attributes.front().toString()).toInt();
      if (song->playcount() <= 0 && playcount > 0) {
        song->set_playcount(static_cast<uint>(playcount));
      }
    }
  }

  if (attributes_map.contains(kASF_FMPS_Rating)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_FMPS_Rating];
    if (!attributes.isEmpty()) {
      float rating = TagLibStringToQString(attributes.front().toString()).toFloat();
      if (song->rating() <= 0 && rating > 0) {
        song->set_rating(rating);
      }
    }
  }

}

void TagReaderTagLib::ParseASFAttribute(const TagLib::ASF::AttributeListMap &attributes_map, const char *attribute, std::string *str) const {

  if (attributes_map.contains(attribute)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[attribute];
    if (!attributes.isEmpty()) {
      AssignTagLibStringToStdString(attributes.front().toString(), str);
    }
  }

}

TagReaderBase::Result TagReaderTagLib::WriteFile(const QString &filename, const spb::tagreader::WriteFileRequest &request) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const spb::tagreader::SongMetadata &song = request.metadata();
  const bool save_tags = request.has_save_tags() && request.save_tags();
  const bool save_playcount = request.has_save_playcount() && request.save_playcount();
  const bool save_rating = request.has_save_rating() && request.save_rating();
  const bool save_cover = request.has_save_cover() && request.save_cover();

  QStringList save_tags_options;
  if (save_tags) {
    save_tags_options << QStringLiteral("tags");
  }
  if (save_playcount) {
    save_tags_options << QStringLiteral("playcount");
  }
  if (save_rating) {
    save_tags_options << QStringLiteral("rating");
  }
  if (save_cover) {
    save_tags_options << QStringLiteral("embedded cover");
  }

  qLog(Debug) << "Saving" << save_tags_options.join(", "_L1) << "to" << filename;

  const Cover cover = LoadCoverFromRequest(filename, request);

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (save_tags) {
    fileref->tag()->setTitle(song.title().empty() ? TagLib::String() : StdStringToTagLibString(song.title()));
    fileref->tag()->setArtist(song.artist().empty() ? TagLib::String() : StdStringToTagLibString(song.artist()));
    fileref->tag()->setAlbum(song.album().empty() ? TagLib::String() : StdStringToTagLibString(song.album()));
    fileref->tag()->setGenre(song.genre().empty() ? TagLib::String() : StdStringToTagLibString(song.genre()));
    fileref->tag()->setComment(song.comment().empty() ? TagLib::String() : StdStringToTagLibString(song.comment()));
    fileref->tag()->setYear(song.year() <= 0 ? 0 : song.year());
    fileref->tag()->setTrack(song.track() <= 0 ? 0 : song.track());
  }

  bool is_flac = false;
  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    is_flac = true;
    TagLib::Ogg::XiphComment *vorbis_comment = file_flac->xiphComment(true);
    if (vorbis_comment) {
      if (save_tags) {
        SetVorbisComments(vorbis_comment, song);
      }
      if (save_playcount) {
        SetPlaycount(vorbis_comment, song.playcount());
      }
      if (save_rating) {
        SetRating(vorbis_comment, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(file_flac, vorbis_comment, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_wavpack->APETag(true);
    if (tag) {
      if (save_tags) {
        SetAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_ape->APETag(true);
    if (tag) {
      if (save_tags) {
        SetAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_mpc->APETag(true);
    if (tag) {
      if (save_tags) {
        SetAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mpeg->ID3v2Tag(true);
    if (tag) {
      if (save_tags) {
        SetID3v2Tag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(tag, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_mp4->tag();
    if (tag) {
      if (save_tags) {
        tag->setItem(kMP4_Disc, TagLib::MP4::Item(song.disc() <= 0 - 1 ? 0 : song.disc(), 0));
        tag->setItem(kMP4_Composer, TagLib::StringList(TagLib::String(song.composer(), TagLib::String::UTF8)));
        tag->setItem(kMP4_Grouping, TagLib::StringList(TagLib::String(song.grouping(), TagLib::String::UTF8)));
        tag->setItem(kMP4_Lyrics, TagLib::StringList(TagLib::String(song.lyrics(), TagLib::String::UTF8)));
        tag->setItem(kMP4_AlbumArtist, TagLib::StringList(TagLib::String(song.albumartist(), TagLib::String::UTF8)));
        tag->setItem(kMP4_Compilation, TagLib::MP4::Item(song.compilation()));
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(file_mp4, tag, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_wav->ID3v2Tag();
    if (tag) {
      if (save_tags) {
        SetID3v2Tag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(tag, cover.data, cover.mime_type);
      }
    }
  }
  else if (TagLib::ASF::File *file_asf = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = file_asf->tag();
    if (tag) {
      SetASFTag(tag, song);
    }
  }

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block above.
  if (!is_flac) {
    if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
      if (vorbis_comment) {
        if (save_tags) {
          SetVorbisComments(vorbis_comment, song);
        }
        if (save_playcount) {
          SetPlaycount(vorbis_comment, song.playcount());
        }
        if (save_rating) {
          SetRating(vorbis_comment, song.rating());
        }
        if (save_cover) {
          SetEmbeddedArt(vorbis_comment, cover.data, cover.mime_type);
        }
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

  return success ? Result(Result::ErrorCode::Success) : Result(Result::ErrorCode::FileSaveError);

}

void TagReaderTagLib::SetID3v2Tag(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  SetTextFrame(kID3v2_Disc, song.disc() <= 0 ? QString() : QString::number(song.disc()), tag);
  SetTextFrame(kID3v2_Composer, song.composer().empty() ? std::string() : song.composer(), tag);
  SetTextFrame(kID3v2_Grouping, song.grouping().empty() ? std::string() : song.grouping(), tag);
  SetTextFrame(kID3v2_Performer, song.performer().empty() ? std::string() : song.performer(), tag);
  // Skip TPE1 (which is the artist) here because we already set it
  SetTextFrame(kID3v2_AlbumArtist, song.albumartist().empty() ? std::string() : song.albumartist(), tag);
  SetTextFrame(kID3v2_Compilation, song.compilation() ? QString::number(1) : QString(), tag);
  SetUnsyncLyricsFrame(song.lyrics().empty() ? std::string() : song.lyrics(), tag);

}

void TagReaderTagLib::SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray utf8(value.toUtf8());
  SetTextFrame(id, std::string(utf8.constData(), utf8.length()), tag);

}

void TagReaderTagLib::SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::ByteVector id_vector(id);
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
      frame->setText(StdStringToTagLibString(value));
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

  const TagLib::String t_description = StdStringToTagLibString(description);
  TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, t_description);
  if (frame) {
    tag->removeFrame(frame);
  }

  // Create and add a new frame
  frame = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
  frame->setDescription(t_description);
  frame->setText(StdStringToTagLibString(value));
  tag->addFrame(frame);

}

void TagReaderTagLib::SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector(kID3v2_UnsychronizedLyrics);
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
    frame.setDescription("Strawberry editor");
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTagLibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

void TagReaderTagLib::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const spb::tagreader::SongMetadata &song) const {

  vorbis_comment->addField(kVorbisComment_Composer, StdStringToTagLibString(song.composer()), true);
  vorbis_comment->addField(kVorbisComment_Performer, StdStringToTagLibString(song.performer()), true);
  vorbis_comment->addField(kVorbisComment_Grouping1, StdStringToTagLibString(song.grouping()), true);
  vorbis_comment->addField(kVorbisComment_Disc, QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  vorbis_comment->addField(kVorbisComment_Compilation, QStringToTagLibString(song.compilation() ? QStringLiteral("1") : QString()), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comment->addField(kVorbisComment_AlbumArtist1, StdStringToTagLibString(song.albumartist()), true);
  vorbis_comment->removeFields(kVorbisComment_AlbumArtist2);

  vorbis_comment->addField(kVorbisComment_Lyrics, StdStringToTagLibString(song.lyrics()), true);
  vorbis_comment->removeFields(kVorbisComment_UnsyncedLyrics);

}

void TagReaderTagLib::SetAPETag(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  tag->setItem(kAPE_AlbumArtist, TagLib::APE::Item(kAPE_AlbumArtist, TagLib::StringList(song.albumartist().c_str())));
  tag->addValue(kAPE_Disc, QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  tag->setItem(kAPE_Composer, TagLib::APE::Item(kAPE_Composer, TagLib::StringList(song.composer().c_str())));
  tag->setItem(kAPE_Grouping, TagLib::APE::Item(kAPE_Grouping, TagLib::StringList(song.grouping().c_str())));
  tag->setItem(kAPE_Performer, TagLib::APE::Item(kAPE_Performer, TagLib::StringList(song.performer().c_str())));
  tag->setItem(kAPE_Lyrics, TagLib::APE::Item(kAPE_Lyrics, TagLib::String(song.lyrics())));
  tag->addValue(kAPE_Compilation, QStringToTagLibString(song.compilation() ? QString::number(1) : QString()), true);

}

void TagReaderTagLib::SetASFTag(TagLib::ASF::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  SetAsfAttribute(tag, kASF_AlbumArtist, song.albumartist());
  SetAsfAttribute(tag, kASF_Composer, song.composer());
  SetAsfAttribute(tag, kASF_Lyrics, song.lyrics());
  SetAsfAttribute(tag, kASF_Disc, song.disc());
  SetAsfAttribute(tag, kASF_OriginalDate, song.originalyear());
  SetAsfAttribute(tag, kASF_OriginalYear, song.originalyear());

}

void TagReaderTagLib::SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const std::string &value) const {

  if (value.empty()) {
    if (tag->contains(attribute)) {
      tag->removeItem(attribute);
    }
  }
  else {
    tag->addAttribute(attribute, StdStringToTagLibString(value));
  }

}

void TagReaderTagLib::SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const int value) const {

  if (value == -1) {
    if (tag->contains(attribute)) {
      tag->removeItem(attribute);
    }
  }
  else {
    tag->addAttribute(attribute, QStringToTagLibString(QString::number(value)));
  }

}

TagReaderBase::Result TagReaderTagLib::LoadEmbeddedArt(const QString &filename, QByteArray &data) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  qLog(Debug) << "Loading art from" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    if (flac_file->xiphComment()) {
      TagLib::List<TagLib::FLAC::Picture*> pictures = flac_file->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
            data = QByteArray(picture->data().data(), picture->data().size());
            if (!data.isEmpty()) {
              return Result::ErrorCode::Success;
            }
          }
        }
      }
    }
  }

  // WavPack
  if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    if (wavpack_file->APETag()) {
      data = LoadEmbeddedAPEArt(wavpack_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // APE
  if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (ape_file->APETag()) {
      data = LoadEmbeddedAPEArt(ape_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // MPC
  if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (mpc_file->APETag()) {
      data = LoadEmbeddedAPEArt(mpc_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // Ogg Vorbis / Opus / Speex
  if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    TagLib::Ogg::FieldListMap map = vorbis_comment->fieldListMap();

    TagLib::List<TagLib::FLAC::Picture*> pictures = vorbis_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
          data = QByteArray(picture->data().data(), picture->data().size());
          if (!data.isEmpty()) {
            return Result::ErrorCode::Success;
          }
        }
      }
    }

    // Ogg lacks a definitive standard for embedding cover art, but it seems b64 encoding a field called COVERART is the general convention
    if (map.contains(kVorbisComment_CoverArt)) {
      data = QByteArray::fromBase64(map[kVorbisComment_CoverArt].toString().toCString());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }

  }

  // MP3
  if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    if (file_mp3->ID3v2Tag()) {
      TagLib::ID3v2::FrameList apic_frames = file_mp3->ID3v2Tag()->frameListMap()[kID3v2_CoverArt];
      if (apic_frames.isEmpty()) {
        return Result::ErrorCode::Success;
      }

      TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(apic_frames.front());

      data = QByteArray(reinterpret_cast<const char*>(picture->picture().data()), picture->picture().size());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // MP4/AAC
  if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag && tag->item(kMP4_CoverArt).isValid()) {
      const TagLib::MP4::CoverArtList &art_list = tag->item(kMP4_CoverArt).toCoverArtList();

      if (!art_list.isEmpty()) {
        // Just take the first one for now
        const TagLib::MP4::CoverArt &art = art_list.front();
        data = QByteArray(art.data().data(), art.data().size());
        if (!data.isEmpty()) {
          return Result::ErrorCode::Success;
        }
      }
    }
  }

  return Result::ErrorCode::Success;

}

QByteArray TagReaderTagLib::LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find(kAPE_CoverArt);
  if (it != map.end()) {
    TagLib::ByteVector data = it->second.binaryData();

    int pos = data.find('\0') + 1;
    if ((pos > 0) && (static_cast<uint>(pos) < data.size())) {
      return QByteArray(data.data() + pos, data.size() - pos);
    }
  }

  return QByteArray();

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mime_type) const {

  (void)vorbis_comment;

  flac_file->removePictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    flac_file->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mime_type) const {

  vorbis_comment->removeAllPictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    vorbis_comment->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  // Remove existing covers
  TagLib::ID3v2::FrameList apiclist = tag->frameListMap()[kID3v2_CoverArt];
  for (TagLib::ID3v2::FrameList::ConstIterator it = apiclist.begin(); it != apiclist.end(); ++it) {
    TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
    tag->removeFrame(frame, false);
  }

  if (!data.isEmpty()) {
    // Add new cover
    TagLib::ID3v2::AttachedPictureFrame *frontcover = nullptr;
    frontcover = new TagLib::ID3v2::AttachedPictureFrame(kID3v2_CoverArt);
    frontcover->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    frontcover->setMimeType(QStringToTagLibString(mime_type));
    frontcover->setPicture(TagLib::ByteVector(data.constData(), data.size()));
    tag->addFrame(frontcover);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  (void)aac_file;

  TagLib::MP4::CoverArtList covers;
  if (data.isEmpty()) {
    if (tag->contains(kMP4_CoverArt)) tag->removeItem(kMP4_CoverArt);
  }
  else {
    TagLib::MP4::CoverArt::Format cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    if (mime_type == "image/jpeg"_L1) {
      cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    }
    else if (mime_type == "image/png"_L1) {
      cover_format = TagLib::MP4::CoverArt::Format::PNG;
    }
    else {
      return;
    }
    covers.append(TagLib::MP4::CoverArt(cover_format, TagLib::ByteVector(data.constData(), data.size())));
    tag->setItem(kMP4_CoverArt, covers);
  }

}

TagReaderBase::Result TagReaderTagLib::SaveEmbeddedArt(const QString &filename, const spb::tagreader::SaveEmbeddedArtRequest &request) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving art to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const Cover cover = LoadCoverFromRequest(filename, request);

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *vorbis_comment = flac_file->xiphComment(true);
    if (vorbis_comment) {
      SetEmbeddedArt(flac_file, vorbis_comment, cover.data, cover.mime_type);
    }
  }

  // Ogg Vorbis / Opus / Speex
  else if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetEmbeddedArt(vorbis_comment, cover.data, cover.mime_type);
  }

  // MP3
  else if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mp3->ID3v2Tag();
    if (tag) {
      SetEmbeddedArt(tag, cover.data, cover.mime_type);
    }
  }

  // MP4/AAC
  else if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag) {
      SetEmbeddedArt(aac_file, tag, cover.data, cover.mime_type);
    }
  }

  // Not supported.
  else {
    qLog(Error) << "Saving embedded art is not supported for %1" << filename;
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->file()->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}

TagLib::ID3v2::PopularimeterFrame *TagReaderTagLib::GetPOPMFrameFromTag(TagLib::ID3v2::Tag *tag) {

  TagLib::ID3v2::PopularimeterFrame *frame = nullptr;

  const TagLib::ID3v2::FrameListMap &map = tag->frameListMap();
  if (map.contains(kID3v2_Popularimeter)) {
    frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(map[kID3v2_Popularimeter].front());
  }

  if (!frame) {
    frame = new TagLib::ID3v2::PopularimeterFrame();
    tag->addFrame(frame);
  }

  return frame;

}

void TagReaderTagLib::SetPlaycount(TagLib::ID3v2::Tag *tag, const uint playcount) const {

  SetUserTextFrame(QLatin1String(kID3v2_FMPS_Playcount), QString::number(playcount), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setCounter(playcount);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::Ogg::XiphComment *vorbis_comment, const uint playcount) const {

  if (playcount > 0) {
    vorbis_comment->addField(kVorbisComment_FMPS_Playcount, TagLib::String::number(static_cast<int>(playcount)), true);
  }
  else {
    vorbis_comment->removeFields(kVorbisComment_FMPS_Playcount);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::APE::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setItem(kAPE_FMPS_Playcount, TagLib::APE::Item(kAPE_FMPS_Playcount, TagLib::String::number(static_cast<int>(playcount))));
  }
  else {
    tag->removeItem(kAPE_FMPS_Playcount);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::MP4::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setItem(kMP4_FMPS_Playcount, TagLib::MP4::Item(TagLib::String::number(static_cast<int>(playcount))));
  }
  else {
    tag->removeItem(kMP4_FMPS_Playcount);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::ASF::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setAttribute(kASF_FMPS_Playcount, TagLib::ASF::Attribute(QStringToTagLibString(QString::number(playcount))));
  }
  else {
    tag->removeItem(kASF_FMPS_Playcount);
  }

}

TagReaderBase::Result TagReaderTagLib::SaveSongPlaycountToFile(const QString &filename, const uint playcount) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song playcount to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *vorbis_comment = flac_file->xiphComment(true);
    if (vorbis_comment) {
      SetPlaycount(vorbis_comment, playcount);
    }
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    if (vorbis_comment) {
      SetPlaycount(vorbis_comment, playcount);
    }
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (tag && playcount > 0) {
      tag->addAttribute(kASF_FMPS_Playcount, TagLib::ASF::Attribute(QStringToTagLibString(QString::number(playcount))));
    }
  }
  else {
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}

void TagReaderTagLib::SetRating(TagLib::ID3v2::Tag *tag, const float rating) const {

  SetUserTextFrame(QLatin1String(kID3v2_FMPS_Rating), QString::number(rating), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setRating(ConvertToPOPMRating(rating));
  }

}

void TagReaderTagLib::SetRating(TagLib::Ogg::XiphComment *vorbis_comment, const float rating) const {

  if (rating > 0.0F) {
    vorbis_comment->addField(kVorbisComment_FMPS_Rating, QStringToTagLibString(QString::number(rating)), true);
  }
  else {
    vorbis_comment->removeFields(kVorbisComment_FMPS_Rating);
  }

}

void TagReaderTagLib::SetRating(TagLib::APE::Tag *tag, const float rating) const {

  if (rating > 0.0F) {
    tag->setItem(kAPE_FMPS_Rating, TagLib::APE::Item(kAPE_FMPS_Rating, TagLib::StringList(QStringToTagLibString(QString::number(rating)))));
  }
  else {
    tag->removeItem(kAPE_FMPS_Rating);
  }

}

void TagReaderTagLib::SetRating(TagLib::MP4::Tag *tag, const float rating) const {

  tag->setItem(kMP4_FMPS_Rating, TagLib::StringList(QStringToTagLibString(QString::number(rating))));

}

void TagReaderTagLib::SetRating(TagLib::ASF::Tag *tag, const float rating) const {

  tag->addAttribute(kASF_FMPS_Rating, TagLib::ASF::Attribute(QStringToTagLibString(QString::number(rating))));

}

TagReaderBase::Result TagReaderTagLib::SaveSongRatingToFile(const QString &filename, const float rating) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song rating to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  if (rating < 0) {
    return Result::ErrorCode::Success;
  }

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *vorbis_comment = flac_file->xiphComment(true);
    if (vorbis_comment) {
      SetRating(vorbis_comment, rating);
    }
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetRating(vorbis_comment, rating);
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else {
    qLog(Error) << "Unsupported file for saving rating for" << filename;
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  if (!success) {
    qLog(Error) << "TagLib hasn't been able to save file" << filename;
  }

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}

TagLib::String TagReaderTagLib::TagLibStringListToSlashSeparatedString(const TagLib::StringList &taglib_string_list) {

  TagLib::String result_string;
  for (const TagLib::String &taglib_string : taglib_string_list) {
    if (!result_string.isEmpty()) {
      result_string += '/';
    }
    result_string += taglib_string;
  }

  return result_string;

}
