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

#include "config.h"

#include "tagreadertaglib.h"

#ifdef HAVE_STREAMTAGREADER
#  include "streamtagreader.h"
#endif

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
#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QtDebug>

#include "includes/scoped_ptr.h"
#include "core/logging.h"
#include "core/song.h"
#include "constants/timeconstants.h"

#include "albumcovertagdata.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

#undef TStringToQString
#undef QStringToTString

namespace {

constexpr char kID3v2_AlbumArtist[] = "TPE2";
constexpr char kID3v2_AlbumArtistSort[] = "TSO2";
constexpr char kID3v2_AlbumSort[] = "TSOA";
constexpr char kID3v2_ArtistSort[] = "TSOP";
constexpr char kID3v2_TitleSort[] = "TSOT";
constexpr char kID3v2_Disc[] = "TPOS";
constexpr char kID3v2_Composer[] = "TCOM";
constexpr char kID3v2_ComposerSort[] = "TSOC";
constexpr char kID3v2_Performer[] = "TOPE";
constexpr char kID3v2_Grouping[] = "TIT1";
constexpr char kID3v2_Compilation[] = "TCMP";
constexpr char kID3v2_OriginalReleaseTime[] = "TDOR";
constexpr char kID3v2_OriginalReleaseYear[] = "TORY";
constexpr char kID3v2_UnsychronizedLyrics[] = "USLT";
constexpr char kID3v2_CoverArt[] = "APIC";
constexpr char kID3v2_FMPS_Playcount[] = "FMPS_Playcount";
constexpr char kID3v2_FMPS_Rating[] = "FMPS_Rating";
constexpr char kID3v2_Unique_File_Identifier[] = "UFID";
constexpr char kID3v2_UserDefinedTextInformationFrame[] = "TXXX";
constexpr char kID3v2_Popularimeter[] = "POPM";
constexpr char kID3v2_BPM[] = "TBPM";
constexpr char kID3v2_Mood[] = "TMOO";
constexpr char kID3v2_Initial_Key[] = "TKEY";
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
constexpr char kVorbisComment_AlbumArtistSort[] = "ALBUMARTISTSORT";
constexpr char kVorbisComment_AlbumSort[] = "ALBUMSORT";
constexpr char kVorbisComment_ArtistSort[] = "ARTISTSORT";
constexpr char kVorbisComment_TitleSort[] = "TITLESORT";
constexpr char kVorbisComment_Composer[] = "COMPOSER";
constexpr char kVorbisComment_ComposerSort[] = "COMPOSERSORT";
constexpr char kVorbisComment_Performer[] = "PERFORMER";
constexpr char kVorbisComment_PerformerSort[] = "PERFORMERSORT";
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
constexpr char kVorbisComment_BPM[] = "BPM";
constexpr char kVorbisComment_Mood[] = "MOOD";
constexpr char kVorbisComment_Initial_Key[] = "INITIALKEY";
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
constexpr char kMP4_BPM[] = "tmpo";
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
constexpr char kAPE_BPM[] = "BPM";
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
  virtual TagLib::FileRef *GetFileRef(TagLib::IOStream *iostream) = 0;

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

  TagLib::FileRef *GetFileRef(TagLib::IOStream *iostream) override {
    return new TagLib::FileRef(iostream);
  }

 private:
  Q_DISABLE_COPY(TagLibFileRefFactory)
};

TagReaderTagLib::TagReaderTagLib() : factory_(new TagLibFileRefFactory) {}

TagReaderTagLib::~TagReaderTagLib() {
  delete factory_;
}

TagReaderResult TagReaderTagLib::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  return fileref &&
         !fileref->isNull() &&
         fileref->file() &&
         fileref->tag() ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::Unsupported;

}

Song::FileType TagReaderTagLib::GuessFileType(TagLib::FileRef *fileref) {

  if (dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) return Song::FileType::WAV;
  if (dynamic_cast<TagLib::FLAC::File*>(fileref->file())) return Song::FileType::FLAC;
  if (dynamic_cast<TagLib::WavPack::File*>(fileref->file())) return Song::FileType::WavPack;
  if (dynamic_cast<TagLib::Ogg::FLAC::File*>(fileref->file())) return Song::FileType::OggFlac;
  if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileref->file())) return Song::FileType::OggVorbis;
  if (dynamic_cast<TagLib::Ogg::Opus::File*>(fileref->file())) return Song::FileType::OggOpus;
  if (dynamic_cast<TagLib::Ogg::Speex::File*>(fileref->file())) return Song::FileType::OggSpeex;
  if (dynamic_cast<TagLib::MPEG::File*>(fileref->file())) return Song::FileType::MPEG;
  if (dynamic_cast<TagLib::MP4::File*>(fileref->file())) return Song::FileType::MP4;
  if (dynamic_cast<TagLib::ASF::File*>(fileref->file())) return Song::FileType::ASF;
  if (dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) return Song::FileType::AIFF;
  if (dynamic_cast<TagLib::MPC::File*>(fileref->file())) return Song::FileType::MPC;
  if (dynamic_cast<TagLib::TrueAudio::File*>(fileref->file())) return Song::FileType::TrueAudio;
  if (dynamic_cast<TagLib::APE::File*>(fileref->file())) return Song::FileType::APE;
  if (dynamic_cast<TagLib::Mod::File*>(fileref->file())) return Song::FileType::MOD;
  if (dynamic_cast<TagLib::S3M::File*>(fileref->file())) return Song::FileType::S3M;
  if (dynamic_cast<TagLib::XM::File*>(fileref->file())) return Song::FileType::XM;
  if (dynamic_cast<TagLib::IT::File*>(fileref->file())) return Song::FileType::IT;
#ifdef HAVE_TAGLIB_DSFFILE
  if (dynamic_cast<TagLib::DSF::File*>(fileref->file())) return Song::FileType::DSF;
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
  if (dynamic_cast<TagLib::DSDIFF::File*>(fileref->file())) return Song::FileType::DSDIFF;
#endif

  return Song::FileType::Unknown;

}

TagReaderResult TagReaderTagLib::Read(SharedPtr<TagLib::FileRef> fileref, Song *song) const {

  song->set_filetype(GuessFileType(&*fileref));

  if (fileref->audioProperties()) {
    song->set_bitrate(fileref->audioProperties()->bitrate());
    song->set_samplerate(fileref->audioProperties()->sampleRate());
    song->set_length_nanosec(fileref->audioProperties()->lengthInMilliseconds() * kNsecPerMsec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    song->set_title(tag->title());
    song->set_artist(tag->artist());  // TPE1
    song->set_album(tag->album());
    song->set_genre(tag->genre());
    song->set_year(static_cast<int>(tag->year()));
    song->set_track(static_cast<int>(tag->track()));
    song->set_comment(tag->comment());
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseVorbisComments(vorbis_comment->fieldListMap(), &disc, &compilation, song);
    if (song->url().isLocalFile()) {
      TagLib::List<TagLib::FLAC::Picture*> pictures = vorbis_comment->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if ((picture->type() == TagLib::FLAC::Picture::FrontCover || picture->type() == TagLib::FLAC::Picture::Other) && picture->data().size() > 0) {
            song->set_art_embedded(true);
            break;
          }
        }
      }
    }
  }

  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    song->set_bitdepth(file_flac->audioProperties()->bitsPerSample());
    if (file_flac->xiphComment()) {
      ParseVorbisComments(file_flac->xiphComment()->fieldListMap(), &disc, &compilation, song);
      if (song->url().isLocalFile()) {
        TagLib::List<TagLib::FLAC::Picture*> pictures = file_flac->pictureList();
        if (!pictures.isEmpty()) {
          for (TagLib::FLAC::Picture *picture : pictures) {
            if ((picture->type() == TagLib::FLAC::Picture::FrontCover || picture->type() == TagLib::FLAC::Picture::Other) && picture->data().size() > 0) {
              song->set_art_embedded(true);
              break;
            }
          }
        }
      }
    }
    if (tag) {
      song->set_comment(tag->comment());
    }
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    song->set_bitdepth(file_wavpack->audioProperties()->bitsPerSample());
    if (file_wavpack->APETag()) {
      ParseAPETags(file_wavpack->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) {
      song->set_comment(tag->comment());
    }
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    song->set_bitdepth(file_ape->audioProperties()->bitsPerSample());
    if (file_ape->APETag()) {
      ParseAPETags(file_ape->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) {
      song->set_comment(tag->comment());
    }
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
      song->set_comment(file_asf->tag()->comment());
      ParseASFTags(file_asf->tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      ParseAPETags(file_mpc->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) {
      song->set_comment(tag->comment());
    }
  }

  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    if (file_wav->hasID3v2Tag()) {
      ParseID3v2Tags(file_wav->ID3v2Tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::RIFF::AIFF::File *file_aiff = dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) {
    if (file_aiff->hasID3v2Tag()) {
      ParseID3v2Tags(file_aiff->tag(), &disc, &compilation, song);
    }
  }

  else if (tag) {
    song->set_comment(tag->comment());
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
    // Compilation wasn't set, but if the artist is VA assume it's a compilation
    const QString &albumartist = song->albumartist();
    const QString &artist = song->artist();
    if (artist.compare("various artists"_L1) == 0 || albumartist.compare("various artists"_L1) == 0) {
      song->set_compilation(true);
    }
  }
  else {
    song->set_compilation(compilation.toInt() == 1);
  }

  if (!lyrics.isEmpty()) song->set_lyrics(lyrics);

  // Set integer fields to -1 if they're not valid

  if (song->track() <= 0) { song->set_track(-1); }
  if (song->disc() <= 0) { song->set_disc(-1); }
  if (song->year() <= 0) { song->set_year(-1); }
  if (song->originalyear() <= 0) { song->set_originalyear(-1); }
  if (song->samplerate() <= 0) { song->set_samplerate(-1); }
  if (song->bitdepth() <= 0) { song->set_bitdepth(-1); }
  if (song->bitrate() <= 0) { song->set_bitrate(-1); }
  if (song->lastplayed() <= 0) { song->set_lastplayed(-1); }

  if (song->filetype() == Song::FileType::Unknown) {
    return TagReaderResult::ErrorCode::Unsupported;
  }

  return TagReaderResult::ErrorCode::Success;

}

TagReaderResult TagReaderTagLib::ReadFile(const QString &filename, Song *song) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Reading tags from file" << filename;

  const QFileInfo fileinfo(filename);
  if (!fileinfo.exists()) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  if (song->source() == Song::Source::Unknown) song->set_source(Song::Source::LocalFile);

  const QUrl url = QUrl::fromLocalFile(filename);
  song->set_basefilename(fileinfo.fileName());
  song->set_url(url);
  song->set_filesize(fileinfo.size());
  song->set_mtime(fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  song->set_ctime(fileinfo.birthTime().isValid() ? std::max(fileinfo.birthTime().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  if (song->ctime() <= 0) {
    song->set_ctime(song->mtime());
  }
  song->set_lastseen(QDateTime::currentSecsSinceEpoch());
  song->set_init_from_file(true);

  SharedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
  }

  const TagReaderResult result = Read(fileref, song);
  if (result.error_code == TagReaderResult::ErrorCode::Unsupported) {
    qLog(Error) << "Unknown audio filetype reading file" << filename;
    return TagReaderResult::ErrorCode::Unsupported;
  }

  qLog(Debug) << "Got tags for" << filename;

  return result;

}

#ifdef HAVE_STREAMTAGREADER

TagReaderResult TagReaderTagLib::ReadStream(const QUrl &url,
                                            const QString &filename,
                                            const quint64 size,
                                            const quint64 mtime,
                                            const QString &token_type,
                                            const QString &access_token,
                                            Song *song) const {

  qLog(Debug) << "Loading tags from stream" << url << filename;

  song->set_url(url);
  song->set_basefilename(QFileInfo(filename).baseName());
  song->set_filesize(static_cast<qint64>(size));
  song->set_ctime(static_cast<qint64>(mtime));
  song->set_mtime(static_cast<qint64>(mtime));

  ScopedPtr<StreamTagReader> stream = make_unique<StreamTagReader>(url, filename, size, token_type, access_token);
  stream->PreCache();

  if (stream->num_requests() > 2) {
    qLog(Warning) << "Total requests for file" << filename << stream->num_requests() << stream->cached_bytes();
  }

  SharedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(&*stream));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open stream" << filename << url;
    return TagReaderResult::ErrorCode::FileOpenError;
  }

  const TagReaderResult result = Read(fileref, song);
  if (result.error_code == TagReaderResult::ErrorCode::Unsupported) {
    qLog(Error) << "Unknown audio filetype reading stream" << filename << url;
    return result;
  }

  qLog(Debug) << "Got tags for stream" << filename << url;

  return result;

}

#endif  // HAVE_STREAMTAGREADER

void TagReaderTagLib::ParseID3v2Tags(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, Song *song) const {

  TagLib::ID3v2::FrameListMap map = tag->frameListMap();

  if (map.contains(kID3v2_Disc)) *disc = TagLibStringToQString(map[kID3v2_Disc].front()->toString()).trimmed();
  if (map.contains(kID3v2_Composer)) song->set_composer(map[kID3v2_Composer].front()->toString());
  if (map.contains(kID3v2_ComposerSort)) song->set_composersort(map[kID3v2_ComposerSort].front()->toString());

  // content group
  if (map.contains(kID3v2_Grouping)) song->set_grouping(map[kID3v2_Grouping].front()->toString());

  // original artist/performer
  if (map.contains(kID3v2_Performer)) song->set_performer(map[kID3v2_Performer].front()->toString());

  // Skip TPE1 (which is the artist) here because we already fetched it

  // non-standard: Apple, Microsoft
  if (map.contains(kID3v2_AlbumArtist)) song->set_albumartist(map[kID3v2_AlbumArtist].front()->toString());

  if (map.contains(kID3v2_AlbumArtistSort)) song->set_albumartistsort(map[kID3v2_AlbumArtistSort].front()->toString());
  if (map.contains(kID3v2_AlbumSort)) song->set_albumsort(map[kID3v2_AlbumSort].front()->toString());
  if (map.contains(kID3v2_ArtistSort)) song->set_artistsort(map[kID3v2_ArtistSort].front()->toString());
  if (map.contains(kID3v2_TitleSort)) song->set_titlesort(map[kID3v2_TitleSort].front()->toString());

  if (map.contains(kID3v2_Compilation)) *compilation = TagLibStringToQString(map[kID3v2_Compilation].front()->toString()).trimmed();

  if (map.contains(kID3v2_OriginalReleaseTime)) {
    song->set_originalyear(map[kID3v2_OriginalReleaseTime].front()->toString().substr(0, 4).toInt());
  }
  else if (map.contains(kID3v2_OriginalReleaseYear)) {
    song->set_originalyear(map[kID3v2_OriginalReleaseYear].front()->toString().substr(0, 4).toInt());
  }

  if (map.contains(kID3v2_UnsychronizedLyrics)) {
    song->set_lyrics(map[kID3v2_UnsychronizedLyrics].front()->toString());
  }

  if (map.contains(kID3v2_CoverArt) && song->url().isLocalFile()) song->set_art_embedded(true);

  if (map.contains(kID3v2_BPM)) song->set_bpm(TagLibStringToQString(map[kID3v2_BPM].front()->toString()).trimmed().toFloat());
  if (map.contains(kID3v2_Mood)) song->set_mood(map[kID3v2_Mood].front()->toString());
  if (map.contains(kID3v2_Initial_Key)) song->set_initial_key(map[kID3v2_Initial_Key].front()->toString());

  if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_playcount = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, kID3v2_FMPS_Playcount)) {
    TagLib::StringList frame_field_list = frame_fmps_playcount->fieldList();
    if (frame_field_list.size() > 1) {
      const int playcount = TagLibStringToQString(frame_field_list[1]).toInt();
      if (song->playcount() <= 0 && playcount > 0) {
        song->set_playcount(static_cast<uint>(playcount));
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
          song->set_musicbrainz_recording_id(TagLibStringToQString(property_map[kID3v2_MusicBrainz_RecordingId].toString()));
        }
      }
    }
  }

  if (map.contains(kID3v2_UserDefinedTextInformationFrame)) {
    for (uint i = 0; i < map[kID3v2_UserDefinedTextInformationFrame].size(); ++i) {
      if (TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(map[kID3v2_UserDefinedTextInformationFrame][i])) {
        const TagLib::StringList frame_field_list = frame->fieldList();
        if (frame_field_list.size() < 2) continue;
        if (frame->description() == kID3v2_AcoustId) {
          song->set_acoustid_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_AcoustId_Fingerprint) {
          song->set_acoustid_fingerprint(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumArtistId) {
          song->set_musicbrainz_album_artist_id(TagLibStringListToSlashSeparatedString(frame_field_list, 1));
        }
        if (frame->description() == kID3v2_MusicBrainz_ArtistId) {
          song->set_musicbrainz_artist_id(TagLibStringListToSlashSeparatedString(frame_field_list, 1));
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalArtistId) {
          song->set_musicbrainz_original_artist_id(TagLibStringListToSlashSeparatedString(frame_field_list, 1));
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumId) {
          song->set_musicbrainz_album_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalAlbumId) {
          song->set_musicbrainz_original_album_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_TrackId) {
          song->set_musicbrainz_track_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_DiscId) {
          song->set_musicbrainz_disc_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_ReleaseGroupId) {
          song->set_musicbrainz_release_group_id(frame_field_list.back());
        }
        if (frame->description() == kID3v2_MusicBrainz_WorkId) {
          song->set_musicbrainz_work_id(TagLibStringListToSlashSeparatedString(frame_field_list, 1));
        }
      }
    }
  }

}

void TagReaderTagLib::ParseVorbisComments(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, Song *song) const {

  if (map.contains(kVorbisComment_Composer)) song->set_composer(map[kVorbisComment_Composer].front());
  if (map.contains(kVorbisComment_ComposerSort)) song->set_composersort(map[kVorbisComment_ComposerSort].front());
  if (map.contains(kVorbisComment_Performer)) song->set_performer(map[kVorbisComment_Performer].front());
  if (map.contains(kVorbisComment_PerformerSort)) song->set_performersort(map[kVorbisComment_PerformerSort].front());
  if (map.contains(kVorbisComment_Grouping2)) song->set_grouping(map[kVorbisComment_Grouping2].front());
  if (map.contains(kVorbisComment_Grouping1)) song->set_grouping(map[kVorbisComment_Grouping1].front());

  if (map.contains(kVorbisComment_AlbumArtist1)) song->set_albumartist(map[kVorbisComment_AlbumArtist1].front());
  else if (map.contains(kVorbisComment_AlbumArtist2)) song->set_albumartist(map[kVorbisComment_AlbumArtist2].front());

  if (map.contains(kVorbisComment_AlbumArtistSort)) song->set_albumartistsort(map[kVorbisComment_AlbumArtistSort].front());
  if (map.contains(kVorbisComment_AlbumSort)) song->set_albumsort(map[kVorbisComment_AlbumSort].front());
  if (map.contains(kVorbisComment_ArtistSort)) song->set_artistsort(map[kVorbisComment_ArtistSort].front());
  if (map.contains(kVorbisComment_TitleSort)) song->set_titlesort(map[kVorbisComment_TitleSort].front());

  if (map.contains(kVorbisComment_OriginalYear1)) song->set_originalyear(TagLibStringToQString(map[kVorbisComment_OriginalYear1].front()).left(4).toInt());
  else if (map.contains(kVorbisComment_OriginalYear2)) song->set_originalyear(TagLibStringToQString(map[kVorbisComment_OriginalYear2].front()).toInt());

  if (map.contains(kVorbisComment_Disc)) *disc = TagLibStringToQString(map[kVorbisComment_Disc].front()).trimmed();
  if (map.contains(kVorbisComment_Compilation)) *compilation = TagLibStringToQString(map[kVorbisComment_Compilation].front()).trimmed();
  if ((map.contains(kVorbisComment_CoverArt) || map.contains(kVorbisComment_MetadataBlockPicture)) && song->url().isLocalFile()) song->set_art_embedded(true);

  if (map.contains(kVorbisComment_FMPS_Playcount) && song->playcount() <= 0) {
    const int playcount = TagLibStringToQString(map[kVorbisComment_FMPS_Playcount].front()).trimmed().toInt();
    song->set_playcount(static_cast<uint>(playcount));
  }
  if (map.contains(kVorbisComment_FMPS_Rating) && song->rating() <= 0) song->set_rating(TagLibStringToQString(map[kVorbisComment_FMPS_Rating].front()).trimmed().toFloat());

  if (map.contains(kVorbisComment_Lyrics)) song->set_lyrics(map[kVorbisComment_Lyrics].front());
  else if (map.contains(kVorbisComment_UnsyncedLyrics)) song->set_lyrics(map[kVorbisComment_UnsyncedLyrics].front());

  if (map.contains(kVorbisComment_BPM)) song->set_bpm(TagLibStringToQString(map[kVorbisComment_BPM].front()).toFloat());
  if (map.contains(kVorbisComment_Mood)) song->set_mood(map[kVorbisComment_Mood].front());
  if (map.contains(kVorbisComment_Initial_Key)) song->set_initial_key(map[kVorbisComment_Initial_Key].front());

  if (map.contains(kVorbisComment_AcoustId)) song->set_acoustid_id(map[kVorbisComment_AcoustId].front());
  if (map.contains(kVorbisComment_AcoustId_Fingerprint)) song->set_acoustid_fingerprint(map[kVorbisComment_AcoustId_Fingerprint].front());

  if (map.contains(kVorbisComment_MusicBrainz_AlbumArtistId)) song->set_musicbrainz_album_artist_id(map[kVorbisComment_MusicBrainz_AlbumArtistId].front());
  if (map.contains(kVorbisComment_MusicBrainz_ArtistId)) song->set_musicbrainz_artist_id(map[kVorbisComment_MusicBrainz_ArtistId].front());
  if (map.contains(kVorbisComment_MusicBrainz_OriginalArtistId)) song->set_musicbrainz_original_artist_id(map[kVorbisComment_MusicBrainz_OriginalArtistId].front());
  if (map.contains(kVorbisComment_MusicBrainz_AlbumId)) song->set_musicbrainz_album_id(map[kVorbisComment_MusicBrainz_AlbumId].front());
  if (map.contains(kVorbisComment_MusicBrainz_OriginalAlbumId)) song->set_musicbrainz_original_album_id(map[kVorbisComment_MusicBrainz_OriginalAlbumId].front());
  if (map.contains(kVorbisComment_MusicBrainz_TackId)) song->set_musicbrainz_recording_id(map[kVorbisComment_MusicBrainz_TackId].front());
  if (map.contains(kVorbisComment_MusicBrainz_ReleaseTrackId)) song->set_musicbrainz_track_id(map[kVorbisComment_MusicBrainz_ReleaseTrackId].front());
  if (map.contains(kVorbisComment_MusicBrainz_DiscId)) song->set_musicbrainz_disc_id(map[kVorbisComment_MusicBrainz_DiscId].front());
  if (map.contains(kVorbisComment_MusicBrainz_ReleaseGroupId)) song->set_musicbrainz_release_group_id(map[kVorbisComment_MusicBrainz_ReleaseGroupId].front());
  if (map.contains(kVorbisComment_MusicBrainz_WorkId)) song->set_musicbrainz_work_id(map[kVorbisComment_MusicBrainz_WorkId].front());

}

void TagReaderTagLib::ParseAPETags(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, Song *song) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find(kAPE_AlbumArtist);
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.values();
    if (!album_artists.isEmpty()) {
      song->set_albumartist(album_artists.front());
    }
  }

  if (map.find(kAPE_CoverArt) != map.end() && song->url().isLocalFile()) song->set_art_embedded(true);
  if (map.contains(kAPE_Compilation)) {
    *compilation = TagLibStringToQString(TagLib::String::number(map[kAPE_Compilation].toString().toInt()));
  }

  if (map.contains(kAPE_Disc)) {
    *disc = TagLibStringToQString(TagLib::String::number(map[kAPE_Disc].toString().toInt()));
  }

  if (map.contains(kAPE_Performer)) {
    song->set_performer(map[kAPE_Performer].values().toString(", "));
  }

  if (map.contains(kAPE_Composer)) {
    song->set_composer(map[kAPE_Composer].values().toString(", "));
  }

  if (map.contains(kAPE_Grouping)) {
    song->set_grouping(map[kAPE_Grouping].values().toString(" "));
  }

  if (map.contains(kAPE_Lyrics)) {
    song->set_lyrics(map[kAPE_Lyrics].toString());
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

  if (map.contains(kAPE_BPM)) {
    song->set_bpm(TagLibStringToQString(map[kAPE_BPM].toString()).toFloat());
  }

  if (map.contains(kAPE_AcoustId)) song->set_acoustid_id(map[kAPE_AcoustId].toString());
  if (map.contains(kAPE_AcoustId_Fingerprint)) song->set_acoustid_fingerprint(map[kAPE_AcoustId_Fingerprint].toString());

  if (map.contains(kAPE_MusicBrainz_AlbumArtistId)) song->set_musicbrainz_album_artist_id(map[kAPE_MusicBrainz_AlbumArtistId].toString());
  if (map.contains(kAPE_MusicBrainz_ArtistId)) song->set_musicbrainz_artist_id(map[kAPE_MusicBrainz_ArtistId].toString());
  if (map.contains(kAPE_MusicBrainz_OriginalArtistId)) song->set_musicbrainz_original_artist_id(map[kAPE_MusicBrainz_OriginalArtistId].toString());
  if (map.contains(kAPE_MusicBrainz_AlbumId)) song->set_musicbrainz_album_id(map[kAPE_MusicBrainz_AlbumId].toString());
  if (map.contains(kAPE_MusicBrainz_OriginalAlbumId)) song->set_musicbrainz_original_album_id(map[kAPE_MusicBrainz_OriginalAlbumId].toString());
  if (map.contains(kAPE_MusicBrainz_TackId)) song->set_musicbrainz_recording_id(map[kAPE_MusicBrainz_TackId].toString());
  if (map.contains(kAPE_MusicBrainz_ReleaseTrackId)) song->set_musicbrainz_track_id(map[kAPE_MusicBrainz_ReleaseTrackId].toString());
  if (map.contains(kAPE_MusicBrainz_DiscId)) song->set_musicbrainz_disc_id(map[kAPE_MusicBrainz_DiscId].toString());
  if (map.contains(kAPE_MusicBrainz_ReleaseGroupId)) song->set_musicbrainz_release_group_id(map[kAPE_MusicBrainz_ReleaseGroupId].toString());
  if (map.contains(kAPE_MusicBrainz_WorkId)) song->set_musicbrainz_work_id(map[kAPE_MusicBrainz_WorkId].toString());

}

void TagReaderTagLib::ParseMP4Tags(TagLib::MP4::Tag *tag, QString *disc, QString *compilation, Song *song) const {

  Q_UNUSED(compilation);

  // Find album artists
  if (tag->item(kMP4_AlbumArtist).isValid()) {
    const TagLib::StringList album_artists = tag->item(kMP4_AlbumArtist).toStringList();
    if (!album_artists.isEmpty()) {
      song->set_albumartist(album_artists.front());
    }
  }

  // Find album cover art
  if (tag->item(kMP4_CoverArt).isValid() && song->url().isLocalFile()) {
    song->set_art_embedded(true);
  }

  if (tag->item(kMP4_Disc).isValid()) {
    *disc = TagLibStringToQString(TagLib::String::number(tag->item(kMP4_Disc).toIntPair().first));
  }

  if (tag->item(kMP4_Composer).isValid()) {
    song->set_composer(tag->item(kMP4_Composer).toStringList().toString(", "));
  }
  if (tag->item(kMP4_Grouping).isValid()) {
    song->set_grouping(tag->item(kMP4_Grouping).toStringList().toString(" "));
  }
  if (tag->item(kMP4_Lyrics).isValid()) {
    song->set_lyrics(tag->item(kMP4_Lyrics).toStringList().toString(" "));
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

  song->set_comment(tag->comment());

  if (tag->item(kMP4_BPM).isValid()) {
    const TagLib::MP4::Item item = tag->item(kMP4_BPM);
    if (item.isValid()) {
      const float bpm = TagLibStringToQString(item.toStringList().toString('\n')).toFloat();
      if (bpm > 0) {
        song->set_bpm(bpm);
      }
    }
  }

  if (tag->contains(kMP4_AcoustId)) {
    song->set_acoustid_id(tag->item(kMP4_AcoustId).toStringList().toString());
  }
  if (tag->contains(kMP4_AcoustId_Fingerprint)) {
    song->set_acoustid_fingerprint(tag->item(kMP4_AcoustId_Fingerprint).toStringList().toString());
  }

  if (tag->contains(kMP4_MusicBrainz_AlbumArtistId)) {
    song->set_musicbrainz_album_artist_id(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_AlbumArtistId).toStringList()));
  }
  if (tag->contains(kMP4_MusicBrainz_ArtistId)) {
    song->set_musicbrainz_artist_id(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_ArtistId).toStringList()));
  }
  if (tag->contains(kMP4_MusicBrainz_OriginalArtistId)) {
    song->set_musicbrainz_original_artist_id(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_OriginalArtistId).toStringList()));
  }
  if (tag->contains(kMP4_MusicBrainz_AlbumId)) {
    song->set_musicbrainz_album_id(tag->item(kMP4_MusicBrainz_AlbumId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_OriginalAlbumId)) {
    song->set_musicbrainz_original_album_id(tag->item(kMP4_MusicBrainz_OriginalAlbumId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_RecordingId)) {
    song->set_musicbrainz_recording_id(tag->item(kMP4_MusicBrainz_RecordingId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_TrackId)) {
    song->set_musicbrainz_track_id(tag->item(kMP4_MusicBrainz_TrackId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_DiscId)) {
    song->set_musicbrainz_disc_id(tag->item(kMP4_MusicBrainz_DiscId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_ReleaseGroupId)) {
    song->set_musicbrainz_release_group_id(tag->item(kMP4_MusicBrainz_ReleaseGroupId).toStringList().toString());
  }
  if (tag->contains(kMP4_MusicBrainz_WorkId)) {
    song->set_musicbrainz_work_id(TagLibStringListToSlashSeparatedString(tag->item(kMP4_MusicBrainz_WorkId).toStringList()));
  }

}

void TagReaderTagLib::ParseASFTags(TagLib::ASF::Tag *tag, QString *disc, QString *compilation, Song *song) const {

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

void TagReaderTagLib::ParseASFAttribute(const TagLib::ASF::AttributeListMap &attributes_map, const char *attribute, QString *str) const {

  if (attributes_map.contains(attribute)) {
    const TagLib::ASF::AttributeList &attributes = attributes_map[attribute];
    if (!attributes.isEmpty()) {
      *str = TagLibStringToQString(attributes.front().toString());
    }
  }

}

TagReaderResult TagReaderTagLib::WriteFile(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  const bool save_tags = save_tags_options.testFlag(SaveTagsOption::Tags);
  const bool save_playcount = save_tags_options.testFlag(SaveTagsOption::Playcount);
  const bool save_rating = save_tags_options.testFlag(SaveTagsOption::Rating);
  const bool save_cover = save_tags_options.testFlag(SaveTagsOption::Cover);

  QStringList save_tags_options_list;
  if (save_tags) {
    save_tags_options_list << u"tags"_s;
  }
  if (save_playcount) {
    save_tags_options_list << u"playcount"_s;
  }
  if (save_rating) {
    save_tags_options_list << u"rating"_s;
  }
  if (save_cover) {
    save_tags_options_list << u"embedded cover"_s;
  }

  qLog(Debug) << "Saving" << save_tags_options_list.join(", "_L1) << "to" << filename;

  AlbumCoverTagData cover;
  if (save_cover) {
    cover = LoadAlbumCoverTagData(filename, save_tag_cover_data);
  }

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
  }

  if (save_tags) {
    fileref->tag()->setTitle(song.title().isEmpty() ? TagLib::String() : QStringToTagLibString(song.title()));
    fileref->tag()->setArtist(song.artist().isEmpty() ? TagLib::String() : QStringToTagLibString(song.artist()));
    fileref->tag()->setAlbum(song.album().isEmpty() ? TagLib::String() : QStringToTagLibString(song.album()));
    fileref->tag()->setGenre(song.genre().isEmpty() ? TagLib::String() : QStringToTagLibString(song.genre()));
    fileref->tag()->setComment(song.comment().isEmpty() ? TagLib::String() : QStringToTagLibString(song.comment()));
    fileref->tag()->setYear(song.year() <= 0 ? 0 : static_cast<uint>(song.year()));
    fileref->tag()->setTrack(song.track() <= 0 ? 0 : static_cast<uint>(song.track()));
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
        SetEmbeddedCover(file_flac, vorbis_comment, cover.data, cover.mimetype);
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
        SetEmbeddedCover(tag, cover.data, cover.mimetype);
      }
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_mp4->tag();
    if (tag) {
      if (save_tags) {
        tag->setItem(kMP4_Disc, TagLib::MP4::Item(song.disc() <= 0 - 1 ? 0 : song.disc(), 0));
        tag->setItem(kMP4_Composer, TagLib::StringList(QStringToTagLibString(song.composer())));
        tag->setItem(kMP4_Grouping, TagLib::StringList(QStringToTagLibString(song.grouping())));
        tag->setItem(kMP4_Lyrics, TagLib::StringList(QStringToTagLibString(song.lyrics())));
        tag->setItem(kMP4_AlbumArtist, TagLib::StringList(QStringToTagLibString(song.albumartist())));
        tag->setItem(kMP4_Compilation, TagLib::MP4::Item(song.compilation()));
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedCover(file_mp4, tag, cover.data, cover.mimetype);
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
        SetEmbeddedCover(tag, cover.data, cover.mimetype);
      }
    }
  }

  else if (TagLib::RIFF::AIFF::File *file_aiff = dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_aiff->tag();
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
        SetEmbeddedCover(tag, cover.data, cover.mimetype);
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
          SetEmbeddedCover(vorbis_comment, cover.data, cover.mimetype);
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

  return success ? TagReaderResult(TagReaderResult::ErrorCode::Success) : TagReaderResult(TagReaderResult::ErrorCode::FileSaveError);

}

void TagReaderTagLib::SetID3v2Tag(TagLib::ID3v2::Tag *tag, const Song &song) const {

  SetTextFrame(kID3v2_Disc, song.disc() <= 0 ? QString() : QString::number(song.disc()), tag);
  SetTextFrame(kID3v2_Composer, song.composer().isEmpty() ? QString() : song.composer(), tag);
  SetTextFrame(kID3v2_ComposerSort, song.composersort().isEmpty() ? QString() : song.composersort(), tag);
  SetTextFrame(kID3v2_Grouping, song.grouping().isEmpty() ? QString() : song.grouping(), tag);
  SetTextFrame(kID3v2_Performer, song.performer().isEmpty() ? QString() : song.performer(), tag);
  // Skip TPE1 (which is the artist) here because we already set it
  SetTextFrame(kID3v2_AlbumArtist, song.albumartist().isEmpty() ? QString() : song.albumartist(), tag);
  SetTextFrame(kID3v2_AlbumArtistSort, song.albumartistsort().isEmpty() ? QString() : song.albumartistsort(), tag);
  SetTextFrame(kID3v2_AlbumSort, song.albumsort().isEmpty() ? QString() : song.albumsort(), tag);
  SetTextFrame(kID3v2_ArtistSort, song.artistsort().isEmpty() ? QString() : song.artistsort(), tag);
  SetTextFrame(kID3v2_TitleSort, song.titlesort().isEmpty() ? QString() : song.titlesort(), tag);
  SetTextFrame(kID3v2_Compilation, song.compilation() ? QString::number(1) : QString(), tag);
  SetUnsyncLyricsFrame(song.lyrics().isEmpty() ? QString() : song.lyrics(), tag);

}

void TagReaderTagLib::SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::ByteVector id_vector(id);
  QList<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  if (value.isEmpty()) return;

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::TextIdentificationFrame frame(id_vector, TagLib::String::UTF8);
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(QStringToTagLibString(value));
    }
    // Add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

void TagReaderTagLib::SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::String t_description = QStringToTagLibString(description);
  TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, t_description);
  if (frame) {
    tag->removeFrame(frame);
  }

  // Create and add a new frame
  frame = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
  frame->setDescription(t_description);
  frame->setText(QStringToTagLibString(value));
  tag->addFrame(frame);

}

void TagReaderTagLib::SetUnsyncLyricsFrame(const QString &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector(kID3v2_UnsychronizedLyrics);
  QList<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  if (value.isEmpty()) return;

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
      frame->setText(QStringToTagLibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

void TagReaderTagLib::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const Song &song) const {

  vorbis_comment->addField(kVorbisComment_Composer, QStringToTagLibString(song.composer()), true);
  vorbis_comment->addField(kVorbisComment_ComposerSort, QStringToTagLibString(song.composersort()), true);
  vorbis_comment->addField(kVorbisComment_Performer, QStringToTagLibString(song.performer()), true);
  vorbis_comment->addField(kVorbisComment_PerformerSort, QStringToTagLibString(song.performersort()), true);
  vorbis_comment->addField(kVorbisComment_Grouping1, QStringToTagLibString(song.grouping()), true);
  vorbis_comment->addField(kVorbisComment_Disc, QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  vorbis_comment->addField(kVorbisComment_Compilation, QStringToTagLibString(song.compilation() ? u"1"_s : QString()), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comment->addField(kVorbisComment_AlbumArtist1, QStringToTagLibString(song.albumartist()), true);
  vorbis_comment->removeFields(kVorbisComment_AlbumArtist2);
  vorbis_comment->addField(kVorbisComment_AlbumArtistSort, QStringToTagLibString(song.albumartistsort()), true);
  vorbis_comment->addField(kVorbisComment_AlbumSort, QStringToTagLibString(song.albumsort()), true);
  vorbis_comment->addField(kVorbisComment_ArtistSort, QStringToTagLibString(song.artistsort()), true);
  vorbis_comment->addField(kVorbisComment_TitleSort, QStringToTagLibString(song.titlesort()), true);

  vorbis_comment->addField(kVorbisComment_Lyrics, QStringToTagLibString(song.lyrics()), true);
  vorbis_comment->removeFields(kVorbisComment_UnsyncedLyrics);

}

void TagReaderTagLib::SetAPETag(TagLib::APE::Tag *tag, const Song &song) const {

  tag->setItem(kAPE_AlbumArtist, TagLib::APE::Item(kAPE_AlbumArtist, TagLib::StringList(QStringToTagLibString(song.albumartist()))));
  tag->addValue(kAPE_Disc, QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  tag->setItem(kAPE_Composer, TagLib::APE::Item(kAPE_Composer, TagLib::StringList(QStringToTagLibString(song.composer()))));
  tag->setItem(kAPE_Grouping, TagLib::APE::Item(kAPE_Grouping, TagLib::StringList(QStringToTagLibString(song.grouping()))));
  tag->setItem(kAPE_Performer, TagLib::APE::Item(kAPE_Performer, TagLib::StringList(QStringToTagLibString(song.performer()))));
  tag->setItem(kAPE_Lyrics, TagLib::APE::Item(kAPE_Lyrics, QStringToTagLibString(song.lyrics())));
  tag->addValue(kAPE_Compilation, QStringToTagLibString(song.compilation() ? QString::number(1) : QString()), true);

}

void TagReaderTagLib::SetASFTag(TagLib::ASF::Tag *tag, const Song &song) const {

  SetAsfAttribute(tag, kASF_AlbumArtist, song.albumartist());
  SetAsfAttribute(tag, kASF_Composer, song.composer());
  SetAsfAttribute(tag, kASF_Lyrics, song.lyrics());
  SetAsfAttribute(tag, kASF_Disc, song.disc());
  SetAsfAttribute(tag, kASF_OriginalDate, song.originalyear());
  SetAsfAttribute(tag, kASF_OriginalYear, song.originalyear());

}

void TagReaderTagLib::SetAsfAttribute(TagLib::ASF::Tag *tag, const char *attribute, const QString &value) const {

  if (value.isEmpty()) {
    if (tag->contains(attribute)) {
      tag->removeItem(attribute);
    }
  }
  else {
    tag->addAttribute(attribute, QStringToTagLibString(value));
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

TagReaderResult TagReaderTagLib::LoadEmbeddedCover(const QString &filename, QByteArray &data) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  qLog(Debug) << "Loading cover from" << filename;

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
  }

  // FLAC
  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    if (file_flac->xiphComment()) {
      TagLib::List<TagLib::FLAC::Picture*> pictures = file_flac->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->data().size() <= 0) {
            continue;
          }
          if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
            data = QByteArray(picture->data().data(), picture->data().size());
            return TagReaderResult::ErrorCode::Success;
          }
          else if (picture->type() == TagLib::FLAC::Picture::Other) {
            data = QByteArray(picture->data().data(), picture->data().size());
          }
        }
        if (!data.isEmpty()) {
          return TagReaderResult::ErrorCode::Success;
        }
      }
    }
  }

  // WavPack
  if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    if (file_wavpack->APETag()) {
      data = LoadEmbeddedCover(file_wavpack->APETag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  // APE
  if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (file_ape->APETag()) {
      data = LoadEmbeddedCover(file_ape->APETag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  // MPC
  if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      data = LoadEmbeddedCover(file_mpc->APETag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  // Ogg Vorbis / Opus / Speex
  if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    TagLib::List<TagLib::FLAC::Picture*> pictures = vorbis_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->data().size() <= 0) {
          continue;
        }
        if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
          data = QByteArray(picture->data().data(), picture->data().size());
          return TagReaderResult::ErrorCode::Success;
        }
        else if (picture->type() == TagLib::FLAC::Picture::Other) {
          data = QByteArray(picture->data().data(), picture->data().size());
        }
      }
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }

    // Ogg lacks a definitive standard for embedding cover art, but it seems b64 encoding a field called COVERART is the general convention
    const TagLib::Ogg::FieldListMap map = vorbis_comment->fieldListMap();
    if (map.contains(kVorbisComment_CoverArt)) {
      data = QByteArray::fromBase64(map[kVorbisComment_CoverArt].toString().toCString());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }

  }

  // MP3
  if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    if (file_mp3->ID3v2Tag()) {
      data = LoadEmbeddedCover(file_mp3->ID3v2Tag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  // MP4/AAC
  if (TagLib::MP4::File *file_aac = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_aac->tag();
    if (tag && tag->item(kMP4_CoverArt).isValid()) {
      const TagLib::MP4::CoverArtList &art_list = tag->item(kMP4_CoverArt).toCoverArtList();

      if (!art_list.isEmpty()) {
        // Just take the first one for now
        const TagLib::MP4::CoverArt &art = art_list.front();
        data = QByteArray(art.data().data(), art.data().size());
        if (!data.isEmpty()) {
          return TagReaderResult::ErrorCode::Success;
        }
      }
    }
  }

  // WAV
  if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    if (file_wav->hasID3v2Tag()) {
      data = LoadEmbeddedCover(file_wav->ID3v2Tag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  // AIFF
  if (TagLib::RIFF::AIFF::File *file_aiff = dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) {
    if (file_aiff->hasID3v2Tag()) {
      data = LoadEmbeddedCover(file_aiff->tag());
      if (!data.isEmpty()) {
        return TagReaderResult::ErrorCode::Success;
      }
    }
  }

  return TagReaderResult::ErrorCode::Success;

}

QByteArray TagReaderTagLib::LoadEmbeddedCover(TagLib::ID3v2::Tag *tag) const {

  const TagLib::ID3v2::FrameList apic_frames = tag->frameListMap()[kID3v2_CoverArt];
  if (apic_frames.isEmpty()) {
    return QByteArray();
  }
  TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(apic_frames.front());
  return QByteArray(reinterpret_cast<const char*>(picture->picture().data()), picture->picture().size());

}

QByteArray TagReaderTagLib::LoadEmbeddedCover(TagLib::APE::Tag *tag) const {

  TagLib::APE::ItemListMap::ConstIterator it = tag->itemListMap().find(kAPE_CoverArt);
  if (it != tag->itemListMap().end()) {
    TagLib::ByteVector data = it->second.binaryData();
    const int pos = data.find('\0') + 1;
    if ((pos > 0) && (static_cast<uint>(pos) < data.size())) {
      return QByteArray(data.data() + pos, data.size() - pos);
    }
  }

  return QByteArray();

}

void TagReaderTagLib::SetEmbeddedCover(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mimetype) const {

  (void)vorbis_comment;

  flac_file->removePictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mimetype));
    picture->setData(TagLib::ByteVector(data.constData(), static_cast<uint>(data.size())));
    flac_file->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedCover(TagLib::Ogg::XiphComment *vorbis_comment, const QByteArray &data, const QString &mimetype) const {

  vorbis_comment->removeAllPictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mimetype));
    picture->setData(TagLib::ByteVector(data.constData(), static_cast<uint>(data.size())));
    vorbis_comment->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedCover(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mimetype) const {

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
    frontcover->setMimeType(QStringToTagLibString(mimetype));
    frontcover->setPicture(TagLib::ByteVector(data.constData(), static_cast<uint>(data.size())));
    tag->addFrame(frontcover);
  }

}

void TagReaderTagLib::SetEmbeddedCover(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mimetype) const {

  (void)aac_file;

  TagLib::MP4::CoverArtList covers;
  if (data.isEmpty()) {
    if (tag->contains(kMP4_CoverArt)) tag->removeItem(kMP4_CoverArt);
  }
  else {
    TagLib::MP4::CoverArt::Format cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    if (mimetype == "image/jpeg"_L1) {
      cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    }
    else if (mimetype == "image/png"_L1) {
      cover_format = TagLib::MP4::CoverArt::Format::PNG;
    }
    else {
      return;
    }
    covers.append(TagLib::MP4::CoverArt(cover_format, TagLib::ByteVector(data.constData(), static_cast<uint>(data.size()))));
    tag->setItem(kMP4_CoverArt, covers);
  }

}

TagReaderResult TagReaderTagLib::SaveEmbeddedCover(const QString &filename, const SaveTagCoverData &save_tag_cover_data) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving cover to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
  }

  const AlbumCoverTagData cover = LoadAlbumCoverTagData(filename, save_tag_cover_data);

  // FLAC
  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *vorbis_comment = file_flac->xiphComment(true);
    if (vorbis_comment) {
      SetEmbeddedCover(file_flac, vorbis_comment, cover.data, cover.mimetype);
    }
  }

  // Ogg Vorbis / Opus / Speex
  else if (TagLib::Ogg::XiphComment *vorbis_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetEmbeddedCover(vorbis_comment, cover.data, cover.mimetype);
  }

  // MP3
  else if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mp3->ID3v2Tag();
    if (tag) {
      SetEmbeddedCover(tag, cover.data, cover.mimetype);
    }
  }

  // MP4/AAC
  else if (TagLib::MP4::File *file_aac = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_aac->tag();
    if (tag) {
      SetEmbeddedCover(file_aac, tag, cover.data, cover.mimetype);
    }
  }

  // WAV
  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    if (file_wav->ID3v2Tag()) {
      SetEmbeddedCover(file_wav->ID3v2Tag(), cover.data, cover.mimetype);
    }
  }

  // AIFF
  else if (TagLib::RIFF::AIFF::File *file_aiff = dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) {
    if (file_aiff->tag()) {
      SetEmbeddedCover(file_aiff->tag(), cover.data, cover.mimetype);
    }
  }

  // Not supported.
  else {
    qLog(Error) << "Saving embedded art is not supported for %1" << filename;
    return TagReaderResult::ErrorCode::Unsupported;
  }

  const bool success = fileref->file()->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::FileSaveError;

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

TagReaderResult TagReaderTagLib::SaveSongPlaycount(const QString &filename, const uint playcount) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song playcount to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
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
    return TagReaderResult::ErrorCode::Unsupported;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::FileSaveError;

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

TagReaderResult TagReaderTagLib::SaveSongRating(const QString &filename, const float rating) const {

  if (filename.isEmpty()) {
    return TagReaderResult::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song rating to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return TagReaderResult::ErrorCode::FileDoesNotExist;
  }

  if (rating < 0) {
    return TagReaderResult::ErrorCode::Success;
  }

  ScopedPtr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return TagReaderResult::ErrorCode::FileOpenError;
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
    return TagReaderResult::ErrorCode::Unsupported;
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

  return success ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::FileSaveError;

}

TagLib::String TagReaderTagLib::TagLibStringListToSlashSeparatedString(const TagLib::StringList &taglib_string_list, const uint begin_index) {

  TagLib::String result_string;
  for (uint i = begin_index ; i < taglib_string_list.size(); ++i) {
    const TagLib::String &taglib_string = taglib_string_list[i];
    if (!result_string.isEmpty()) {
      result_string += '/';
    }
    result_string += taglib_string;
  }

  return result_string;

}
