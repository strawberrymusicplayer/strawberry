/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <taglib/fileref.h>
#include <taglib/id3v1genres.h>

#ifdef HAVE_LIBGPOD
#  include <gdk-pixbuf/gdk-pixbuf.h>
#  include <gpod/itdb.h>
#endif

#ifdef HAVE_LIBMTP
#  include <libmtp.h>
#endif

#include <QtGlobal>
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSharedData>
#include <QHash>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QImage>
#include <QIcon>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"
#include "core/iconloader.h"

#include "engine/enginebase.h"
#include "timeconstants.h"
#include "utilities.h"
#include "song.h"
#include "application.h"
#include "mpris_common.h"
#include "collection/sqlrow.h"
#include "tagreadermessages.pb.h"

#ifndef USE_SYSTEM_TAGLIB
using namespace Strawberry_TagLib;
#endif

const QStringList Song::kColumns = QStringList() << "title"
                                                 << "album"
                                                 << "artist"
                                                 << "albumartist"
                                                 << "track"
                                                 << "disc"
                                                 << "year"
                                                 << "originalyear"
                                                 << "genre"
                                                 << "compilation"
                                                 << "composer"
                                                 << "performer"
                                                 << "grouping"
                                                 << "comment"
                                                 << "lyrics"

                                                 << "artist_id"
                                                 << "album_id"
                                                 << "song_id"

                                                 << "beginning"
                                                 << "length"

                                                 << "bitrate"
                                                 << "samplerate"
                                                 << "bitdepth"

                                                 << "source"
                                                 << "directory_id"
                                                 << "url"
                                                 << "filetype"
                                                 << "filesize"
                                                 << "mtime"
                                                 << "ctime"
                                                 << "unavailable"

                                                 << "playcount"
                                                 << "skipcount"
                                                 << "lastplayed"

                                                 << "compilation_detected"
                                                 << "compilation_on"
                                                 << "compilation_off"
                                                 << "compilation_effective"

                                                 << "art_automatic"
                                                 << "art_manual"

                                                 << "effective_albumartist"
                                                 << "effective_originalyear"

                                                 << "cue_path"

                                                 << "rating"

						 ;

const QString Song::kColumnSpec = Song::kColumns.join(", ");
const QString Song::kBindSpec = Utilities::Prepend(":", Song::kColumns).join(", ");
const QString Song::kUpdateSpec = Utilities::Updateify(Song::kColumns).join(", ");

const QStringList Song::kFtsColumns = QStringList() << "ftstitle"
                                                    << "ftsalbum"
                                                    << "ftsartist"
                                                    << "ftsalbumartist"
                                                    << "ftscomposer"
                                                    << "ftsperformer"
                                                    << "ftsgrouping"
                                                    << "ftsgenre"
                                                    << "ftscomment";

const QString Song::kFtsColumnSpec = Song::kFtsColumns.join(", ");
const QString Song::kFtsBindSpec = Utilities::Prepend(":", Song::kFtsColumns).join(", ");
const QString Song::kFtsUpdateSpec = Utilities::Updateify(Song::kFtsColumns).join(", ");

const QString Song::kManuallyUnsetCover = "(unset)";
const QString Song::kEmbeddedCover = "(embedded)";

const QRegularExpression Song::kAlbumRemoveDisc(" ?-? ((\\(|\\[)?)(Disc|CD) ?([0-9]{1,2})((\\)|\\])?)$");
const QRegularExpression Song::kAlbumRemoveMisc(" ?-? ((\\(|\\[)?)(Remastered|([0-9]{1,4}) *Remaster) ?((\\)|\\])?)$");
const QRegularExpression Song::kTitleRemoveMisc(" ?-? ((\\(|\\[)?)(Remastered|Live|Remastered Version|([0-9]{1,4}) *Remaster) ?((\\)|\\])?)$");
const QString Song::kVariousArtists("various artists");

const QStringList Song::kArticles = QStringList() << "the " << "a " << "an ";

struct Song::Private : public QSharedData {

  explicit Private(Source source = Source_Unknown);

  bool valid_;
  int id_;

  QString title_;
  QString title_sortable_;
  QString album_;
  QString album_sortable_;
  QString artist_;
  QString artist_sortable_;
  QString albumartist_;
  QString albumartist_sortable_;
  int track_;
  int disc_;
  int year_;
  int originalyear_;
  QString genre_;
  bool compilation_;		// From the file tag
  QString composer_;
  QString performer_;
  QString grouping_;
  QString comment_;
  QString lyrics_;

  QString artist_id_;
  QString album_id_;
  QString song_id_;

  qint64 beginning_;
  qint64 end_;

  int bitrate_;
  int samplerate_;
  int bitdepth_;

  Source source_;
  int directory_id_;
  QString basefilename_;
  QUrl url_;
  FileType filetype_;
  int filesize_;
  qint64 mtime_;
  qint64 ctime_;
  bool unavailable_;

  int playcount_;
  int skipcount_;
  int lastplayed_;

  bool compilation_detected_;   // From the collection scanner
  bool compilation_on_;         // Set by the user
  bool compilation_off_;        // Set by the user

  // Filenames to album art for this song.
  QUrl art_automatic_;          // Guessed by CollectionWatcher
  QUrl art_manual_;             // Set by the user - should take priority

  QString cue_path_;            // If the song has a CUE, this contains it's path.

  float rating_;                // Database rating, not read from tags.

  QUrl stream_url_;             // Temporary stream url set by url handler.
  QImage image_;                // Album Cover image set by album cover loader.
  bool init_from_file_;         // Whether this song was loaded from a file using taglib.
  bool suspicious_tags_;        // Whether our encoding guesser thinks these tags might be incorrectly encoded.

  QString error_;               // Song load error set by song loader.

};

Song::Private::Private(Song::Source source)
    : valid_(false),
      id_(-1),

      track_(-1),
      disc_(-1),
      year_(-1),
      originalyear_(-1),
      compilation_(false),

      beginning_(0),
      end_(-1),

      bitrate_(-1),
      samplerate_(-1),
      bitdepth_(-1),

      source_(source),
      directory_id_(-1),
      filetype_(FileType_Unknown),
      filesize_(-1),
      mtime_(-1),
      ctime_(-1),
      unavailable_(false),

      playcount_(0),
      skipcount_(0),
      lastplayed_(-1),

      compilation_detected_(false),
      compilation_on_(false),
      compilation_off_(false),

      rating_(-1),

      init_from_file_(false),
      suspicious_tags_(false)

      {}

Song::Song(Song::Source source) : d(new Private(source)) {}
Song::Song(const Song &other) : d(other.d) {}
Song::~Song() {}

Song &Song::operator=(const Song &other) {
  d = other.d;
  return *this;
}

bool Song::is_valid() const { return d->valid_; }
bool Song::is_unavailable() const { return d->unavailable_; }
int Song::id() const { return d->id_; }

QString Song::artist_id() const { return d->artist_id_.isNull() ? "" : d->artist_id_; }
QString Song::album_id() const { return d->album_id_.isNull() ? "" : d->album_id_; }
QString Song::song_id() const { return d->song_id_.isNull() ? "" : d->song_id_; }

const QString &Song::title() const { return d->title_; }
const QString &Song::title_sortable() const { return d->title_sortable_; }
const QString &Song::album() const { return d->album_; }
const QString &Song::album_sortable() const { return d->album_sortable_; }
// This value is useful for singles, which are one-track albums on their own.
const QString &Song::effective_album() const { return d->album_.isEmpty() ? d->title_ : d->album_; }
const QString &Song::artist() const { return d->artist_; }
const QString &Song::artist_sortable() const { return d->artist_sortable_; }
const QString &Song::albumartist() const { return d->albumartist_; }
const QString &Song::albumartist_sortable() const { return d->albumartist_sortable_; }
const QString &Song::effective_albumartist() const { return d->albumartist_.isEmpty() ? d->artist_ : d->albumartist_; }
const QString &Song::effective_albumartist_sortable() const { return d->albumartist_.isEmpty() ? d->artist_sortable_ : d->albumartist_sortable_; }
const QString &Song::playlist_albumartist() const { return is_compilation() ? d->albumartist_ : effective_albumartist(); }
const QString &Song::playlist_albumartist_sortable() const { return is_compilation() ? d->albumartist_sortable_ : effective_albumartist_sortable(); }
int Song::track() const { return d->track_; }
int Song::disc() const { return d->disc_; }
int Song::year() const { return d->year_; }
int Song::originalyear() const { return d->originalyear_; }
int Song::effective_originalyear() const { return d->originalyear_ < 0 ? d->year_ : d->originalyear_; }
const QString &Song::genre() const { return d->genre_; }
bool Song::compilation() const { return d->compilation_; }
const QString &Song::composer() const { return d->composer_; }
const QString &Song::performer() const { return d->performer_; }
const QString &Song::grouping() const { return d->grouping_; }
const QString &Song::comment() const { return d->comment_; }
const QString &Song::lyrics() const { return d->lyrics_; }

qint64 Song::beginning_nanosec() const { return d->beginning_; }
qint64 Song::end_nanosec() const { return d->end_; }
qint64 Song::length_nanosec() const { return d->end_ - d->beginning_; }

int Song::bitrate() const { return d->bitrate_; }
int Song::samplerate() const { return d->samplerate_; }
int Song::bitdepth() const { return d->bitdepth_; }

Song::Source Song::source() const { return d->source_; }
int Song::directory_id() const { return d->directory_id_; }
const QUrl &Song::url() const { return d->url_; }
const QString &Song::basefilename() const { return d->basefilename_; }
Song::FileType Song::filetype() const { return d->filetype_; }
int Song::filesize() const { return d->filesize_; }
qint64 Song::mtime() const { return d->mtime_; }
qint64 Song::ctime() const { return d->ctime_; }

int Song::playcount() const { return d->playcount_; }
int Song::skipcount() const { return d->skipcount_; }
int Song::lastplayed() const { return d->lastplayed_; }

bool Song::compilation_detected() const { return d->compilation_detected_; }
bool Song::compilation_off() const { return d->compilation_off_; }
bool Song::compilation_on() const { return d->compilation_on_; }

const QUrl &Song::art_automatic() const { return d->art_automatic_; }
const QUrl &Song::art_manual() const { return d->art_manual_; }
bool Song::has_manually_unset_cover() const { return d->art_manual_.path() == kManuallyUnsetCover; }
void Song::manually_unset_cover() { d->art_manual_ = QUrl::fromLocalFile(kManuallyUnsetCover); }
bool Song::has_embedded_cover() const { return d->art_automatic_.path() == kEmbeddedCover; }
void Song::set_embedded_cover() { d->art_automatic_ = QUrl::fromLocalFile(kEmbeddedCover); }

const QUrl &Song::stream_url() const { return d->stream_url_; }
const QUrl &Song::effective_stream_url() const { return !d->stream_url_.isEmpty() && d->stream_url_.isValid() ? d->stream_url_ : d->url_; }
const QImage &Song::image() const { return d->image_; }

const QString &Song::cue_path() const { return d->cue_path_; }
bool Song::has_cue() const { return !d->cue_path_.isEmpty(); }

float Song::rating() const { return d->rating_; }

bool Song::is_collection_song() const { return d->source_ == Source_Collection; }
bool Song::is_metadata_good() const { return !d->url_.isEmpty() && !d->artist_.isEmpty() && !d->title_.isEmpty(); }
bool Song::is_stream() const { return d->source_ == Source_Stream || d->source_ == Source_Tidal || d->source_ == Source_Subsonic || d->source_ == Source_Qobuz; }
bool Song::is_cdda() const { return d->source_ == Source_CDDA; }
bool Song::is_compilation() const { return (d->compilation_ || d->compilation_detected_ || d->compilation_on_) && !d->compilation_off_; }

bool Song::art_automatic_is_valid() const {
  return !d->art_automatic_.isEmpty() &&
         (
         (d->art_automatic_.path() == kManuallyUnsetCover) ||
         (d->art_automatic_.path() == kEmbeddedCover) ||
         (d->art_automatic_.isValid() && !d->art_automatic_.isLocalFile()) ||
         (d->art_automatic_.isLocalFile() && QFile::exists(d->art_automatic_.toLocalFile())) ||
         (d->art_automatic_.scheme().isEmpty() && !d->art_automatic_.path().isEmpty() && QFile::exists(d->art_automatic_.path()))
         );
}

bool Song::art_manual_is_valid() const {
  return !d->art_manual_.isEmpty() &&
         (
         (d->art_manual_.path() == kManuallyUnsetCover) ||
         (d->art_manual_.path() == kEmbeddedCover) ||
         (d->art_manual_.isValid() && !d->art_manual_.isLocalFile()) ||
         (d->art_manual_.isLocalFile() && QFile::exists(d->art_manual_.toLocalFile())) ||
         (d->art_manual_.scheme().isEmpty() && !d->art_manual_.path().isEmpty() && QFile::exists(d->art_manual_.path()))
         );
}

const QString &Song::error() const { return d->error_; }

void Song::set_id(int id) { d->id_ = id; }
void Song::set_valid(bool v) { d->valid_ = v; }

void Song::set_artist_id(const QString &v) { d->artist_id_ = v; }
void Song::set_album_id(const QString &v) { d->album_id_ = v; }
void Song::set_song_id(const QString &v) { d->song_id_ = v; }

QString Song::sortable(const QString &v) const {

  QString copy = v.toLower();

  for (const auto &i : kArticles) {
    if (copy.startsWith(i)) {
      int ilen = i.length();
      return copy.right(copy.length() - ilen) + ", " + copy.left(ilen - 1);
    }
  }

  return copy;
}

void Song::set_title(const QString &v) { d->title_sortable_ = sortable(v); d->title_ = v; }
void Song::set_album(const QString &v) { d->album_sortable_ = sortable(v); d->album_ = v; }
void Song::set_artist(const QString &v) { d->artist_sortable_ = sortable(v); d->artist_ = v; }
void Song::set_albumartist(const QString &v) { d->albumartist_sortable_ = sortable(v); d->albumartist_ = v; }
void Song::set_track(int v) { d->track_ = v; }
void Song::set_disc(int v) { d->disc_ = v; }
void Song::set_year(int v) { d->year_ = v; }
void Song::set_originalyear(int v) { d->originalyear_ = v; }
void Song::set_genre(const QString &v) { d->genre_ = v; }
void Song::set_compilation(bool v) { d->compilation_ = v; }
void Song::set_composer(const QString &v) { d->composer_ = v; }
void Song::set_performer(const QString &v) { d->performer_ = v; }
void Song::set_grouping(const QString &v) { d->grouping_ = v; }
void Song::set_comment(const QString &v) { d->comment_ = v; }
void Song::set_lyrics(const QString &v) { d->lyrics_ = v; }

void Song::set_beginning_nanosec(qint64 v) { d->beginning_ = qMax(0ll, v); }
void Song::set_end_nanosec(qint64 v) { d->end_ = v; }
void Song::set_length_nanosec(qint64 v) { d->end_ = d->beginning_ + v; }

void Song::set_bitrate(int v) { d->bitrate_ = v; }
void Song::set_samplerate(int v) { d->samplerate_ = v; }
void Song::set_bitdepth(int v) { d->bitdepth_ = v; }

void Song::set_source(Source v) { d->source_ = v; }
void Song::set_directory_id(int v) { d->directory_id_ = v; }
void Song::set_url(const QUrl &v) { d->url_ = v; }
void Song::set_basefilename(const QString &v) { d->basefilename_ = v; }
void Song::set_filetype(FileType v) { d->filetype_ = v; }
void Song::set_filesize(int v) { d->filesize_ = v; }
void Song::set_mtime(qint64 v) { d->mtime_ = v; }
void Song::set_ctime(qint64 v) { d->ctime_ = v; }
void Song::set_unavailable(bool v) { d->unavailable_ = v; }

void Song::set_playcount(int v) { d->playcount_ = v; }
void Song::set_skipcount(int v) { d->skipcount_ = v; }
void Song::set_lastplayed(int v) { d->lastplayed_ = v; }

void Song::set_compilation_detected(bool v) { d->compilation_detected_ = v; }
void Song::set_compilation_on(bool v) { d->compilation_on_ = v; }
void Song::set_compilation_off(bool v) { d->compilation_off_ = v; }

void Song::set_art_automatic(const QUrl &v) { d->art_automatic_ = v; }
void Song::set_art_manual(const QUrl &v) { d->art_manual_ = v; }
void Song::set_cue_path(const QString &v) { d->cue_path_ = v; }

void Song::set_rating(float v) { d->rating_ = v; }

void Song::set_stream_url(const QUrl &v) { d->stream_url_ = v; }
void Song::set_image(const QImage &i) { d->image_ = i; }

QString Song::JoinSpec(const QString &table) {
  return Utilities::Prepend(table + ".", kColumns).join(", ");
}

Song::Source Song::SourceFromURL(const QUrl &url) {

  if (url.isLocalFile()) return Source_LocalFile;
  else if (url.scheme() == "cdda") return Source_CDDA;
  else if (url.scheme() == "tidal") return Source_Tidal;
  else if (url.scheme() == "subsonic") return Source_Subsonic;
  else if (url.scheme() == "qobuz") return Source_Qobuz;
  else if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "rtsp") return Source_Stream;
  else return Source_Unknown;

}

QString Song::TextForSource(Source source) {

  switch (source) {
    case Song::Source_LocalFile:   return "file";
    case Song::Source_Collection:  return "collection";
    case Song::Source_CDDA:        return "cd";
    case Song::Source_Device:      return "device";
    case Song::Source_Stream:      return "stream";
    case Song::Source_Tidal:       return "tidal";
    case Song::Source_Subsonic:    return "subsonic";
    case Song::Source_Qobuz:       return "qobuz";
    case Song::Source_Unknown:     return "unknown";
  }
  return "unknown";

}

Song::Source Song::SourceFromText(const QString &source) {

  if (source == "file") return Source_LocalFile;
  if (source == "collection") return Source_Collection;
  if (source == "cd") return Source_CDDA;
  if (source == "device") return Source_Device;
  if (source == "stream") return Source_Stream;
  if (source == "tidal") return Source_Tidal;
  if (source == "subsonic") return Source_Subsonic;
  if (source == "qobuz") return Source_Qobuz;

  return Source_Unknown;

}

QIcon Song::IconForSource(Source source) {

  switch (source) {
    case Song::Source_LocalFile:   return IconLoader::Load("folder-sound");
    case Song::Source_Collection:  return IconLoader::Load("library-music");
    case Song::Source_CDDA:        return IconLoader::Load("media-optical");
    case Song::Source_Device:      return IconLoader::Load("device");
    case Song::Source_Stream:      return IconLoader::Load("applications-internet");
    case Song::Source_Tidal:       return IconLoader::Load("tidal");
    case Song::Source_Subsonic:    return IconLoader::Load("subsonic");
    case Song::Source_Qobuz:       return IconLoader::Load("qobuz");
    case Song::Source_Unknown:     return IconLoader::Load("edit-delete");
  }
  return IconLoader::Load("edit-delete");

}

QString Song::TextForFiletype(FileType filetype) {

  switch (filetype) {
    case Song::FileType_WAV:         return "Wav";
    case Song::FileType_FLAC:        return "FLAC";
    case Song::FileType_WavPack:     return "WavPack";
    case Song::FileType_OggFlac:     return "Ogg FLAC";
    case Song::FileType_OggVorbis:   return "Ogg Vorbis";
    case Song::FileType_OggOpus:     return "Ogg Opus";
    case Song::FileType_OggSpeex:    return "Ogg Speex";
    case Song::FileType_MPEG:        return "MP3";
    case Song::FileType_MP4:         return "MP4 AAC";
    case Song::FileType_ASF:         return "Windows Media audio";
    case Song::FileType_AIFF:        return "AIFF";
    case Song::FileType_MPC:         return "MPC";
    case Song::FileType_TrueAudio:   return "TrueAudio";
    case Song::FileType_DSF:         return "DSF";
    case Song::FileType_DSDIFF:      return "DSDIFF";
    case Song::FileType_PCM:         return "PCM";
    case Song::FileType_APE:         return "Monkey's Audio";
    case Song::FileType_CDDA:        return "CDDA";
    case Song::FileType_Stream:      return "Stream";
    case Song::FileType_Unknown:
    default:                         return QObject::tr("Unknown");
  }

}

QString Song::ExtensionForFiletype(FileType filetype) {

  switch (filetype) {
    case Song::FileType_WAV:         return "wav";
    case Song::FileType_FLAC:        return "flac";
    case Song::FileType_WavPack:     return "wv";
    case Song::FileType_OggFlac:     return "flac";
    case Song::FileType_OggVorbis:   return "ogg";
    case Song::FileType_OggOpus:     return "opus";
    case Song::FileType_OggSpeex:    return "spx";
    case Song::FileType_MPEG:        return "mp3";
    case Song::FileType_MP4:         return "mp4";
    case Song::FileType_ASF:         return "wma";
    case Song::FileType_AIFF:        return "aiff";
    case Song::FileType_MPC:         return "mpc";
    case Song::FileType_TrueAudio:   return "tta";
    case Song::FileType_DSF:         return "dsf";
    case Song::FileType_DSDIFF:      return "dsd";
    case Song::FileType_APE:         return "ape";
    case Song::FileType_Unknown:
    default:                         return "dat";
  }

}

QIcon Song::IconForFiletype(FileType filetype) {

  switch (filetype) {
    case Song::FileType_WAV:         return IconLoader::Load("wav");
    case Song::FileType_FLAC:        return IconLoader::Load("flac");
    case Song::FileType_WavPack:     return IconLoader::Load("wavpack");
    case Song::FileType_OggFlac:     return IconLoader::Load("flac");
    case Song::FileType_OggVorbis:   return IconLoader::Load("vorbis");
    case Song::FileType_OggOpus:     return IconLoader::Load("opus");
    case Song::FileType_OggSpeex:    return IconLoader::Load("speex");
    case Song::FileType_MPEG:        return IconLoader::Load("mp3");
    case Song::FileType_MP4:         return IconLoader::Load("mp4");
    case Song::FileType_ASF:         return IconLoader::Load("wma");
    case Song::FileType_AIFF:        return IconLoader::Load("aiff");
    case Song::FileType_MPC:         return IconLoader::Load("mpc");
    case Song::FileType_TrueAudio:   return IconLoader::Load("trueaudio");
    case Song::FileType_DSF:         return IconLoader::Load("dsf");
    case Song::FileType_DSDIFF:      return IconLoader::Load("dsd");
    case Song::FileType_PCM:         return IconLoader::Load("pcm");
    case Song::FileType_APE:         return IconLoader::Load("ape");
    case Song::FileType_CDDA:        return IconLoader::Load("cd");
    case Song::FileType_Stream:      return IconLoader::Load("applications-internet");
    case Song::FileType_Unknown:
    default:                         return IconLoader::Load("edit-delete");
  }

}

bool Song::IsFileLossless() const {
  switch (filetype()) {
    case Song::FileType_WAV:
    case Song::FileType_FLAC:
    case Song::FileType_OggFlac:
    case Song::FileType_WavPack:
    case Song::FileType_AIFF:
    case Song::FileType_DSF:
    case Song::FileType_DSDIFF:
    case Song::FileType_APE:
    case Song::FileType_TrueAudio:
    case Song::FileType_PCM:
    case Song::FileType_CDDA:
      return true;
    default:
      return false;
  }
}

Song::FileType Song::FiletypeByMimetype(const QString &mimetype) {

  if (mimetype.toLower() == "audio/wav" || mimetype.toLower() == "audio/x-wav") return Song::FileType_WAV;
  else if (mimetype.toLower() == "audio/x-flac") return Song::FileType_FLAC;
  else if (mimetype.toLower() == "audio/x-wavpack") return Song::FileType_WavPack;
  else if (mimetype.toLower() == "audio/x-vorbis") return Song::FileType_OggVorbis;
  else if (mimetype.toLower() == "audio/x-opus") return Song::FileType_OggOpus;
  else if (mimetype.toLower() == "audio/x-speex")  return Song::FileType_OggSpeex;
  // Gstreamer returns audio/mpeg for both MP3 and MP4/AAC.
  // else if (mimetype.toLower() == "audio/mpeg") return Song::FileType_MPEG;
  else if (mimetype.toLower() == "audio/aac") return Song::FileType_MP4;
  else if (mimetype.toLower() == "audio/x-wma") return Song::FileType_ASF;
  else if (mimetype.toLower() == "audio/aiff" || mimetype.toLower() == "audio/x-aiff") return Song::FileType_AIFF;
  else if (mimetype.toLower() == "application/x-project") return Song::FileType_MPC;
  else if (mimetype.toLower() == "audio/x-dsf") return Song::FileType_DSF;
  else if (mimetype.toLower() == "audio/x-dsd") return Song::FileType_DSDIFF;
  else if (mimetype.toLower() == "audio/x-ape" || mimetype.toLower() == "application/x-ape" || mimetype.toLower() == "audio/x-ffmpeg-parsed-ape") return Song::FileType_APE;
  else return Song::FileType_Unknown;

}

Song::FileType Song::FiletypeByDescription(const QString &text) {

  if (text == "WAV") return Song::FileType_WAV;
  else if (text == "Free Lossless Audio Codec (FLAC)") return Song::FileType_FLAC;
  else if (text == "Wavpack") return Song::FileType_WavPack;
  else if (text == "Vorbis") return Song::FileType_OggVorbis;
  else if (text == "Opus") return Song::FileType_OggOpus;
  else if (text == "Speex") return Song::FileType_OggSpeex;
  else if (text == "MPEG-1 Layer 3 (MP3)") return Song::FileType_MPEG;
  else if (text == "MPEG-4 AAC") return Song::FileType_MP4;
  else if (text == "WMA") return Song::FileType_ASF;
  else if (text == "Audio Interchange File Format") return Song::FileType_AIFF;
  else if (text == "MPC") return Song::FileType_MPC;
  else if (text == "audio/x-dsf") return Song::FileType_DSF;
  else if (text == "audio/x-dsd") return Song::FileType_DSDIFF;
  else if (text == "audio/x-ffmpeg-parsed-ape") return Song::FileType_APE;
  else return Song::FileType_Unknown;

}

Song::FileType Song::FiletypeByExtension(const QString &ext) {

  if (ext.toLower() == "wav" || ext.toLower() == "wave") return Song::FileType_WAV;
  else if (ext.toLower() == "flac") return Song::FileType_FLAC;
  else if (ext.toLower() == "wavpack" || ext.toLower() == "wv") return Song::FileType_WavPack;
  else if (ext.toLower() == "ogg" || ext.toLower() == "oga") return Song::FileType_OggVorbis;
  else if (ext.toLower() == "opus") return Song::FileType_OggOpus;
  else if (ext.toLower() == "speex" || ext.toLower() == "spx") return Song::FileType_OggSpeex;
  else if (ext.toLower() == "mp3") return Song::FileType_MPEG;
  else if (ext.toLower() == "mp4" || ext.toLower() == "m4a" || ext.toLower() == "aac") return Song::FileType_MP4;
  else if (ext.toLower() == "asf" || ext.toLower() == "wma") return Song::FileType_ASF;
  else if (ext.toLower() == "aiff" || ext.toLower() == "aif" || ext.toLower() == "aifc") return Song::FileType_AIFF;
  else if (ext.toLower() == "mpc" || ext.toLower() == "mp+" || ext.toLower() == "mpp") return Song::FileType_MPC;
  else if (ext.toLower() == "dsf") return Song::FileType_DSF;
  else if (ext.toLower() == "dsd" || ext.toLower() == "dff") return Song::FileType_DSDIFF;
  else if (ext.toLower() == "ape") return Song::FileType_APE;
  else return Song::FileType_Unknown;

}

QString Song::ImageCacheDir(const Song::Source source) {

  switch (source) {
    case Song::Source_Collection:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/collectionalbumcovers";
    case Song::Source_Subsonic:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/subsonicalbumcovers";
    case Song::Source_Tidal:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tidalalbumcovers";
    case Song::Source_Qobuz:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/qobuzalbumcovers";
    case Song::Source_Device:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/devicealbumcovers";
    case Song::Source_LocalFile:
    case Song::Source_CDDA:
    case Song::Source_Stream:
    case Song::Source_Unknown:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/albumcovers";
  }

  return QString();

}

int Song::CompareSongsName(const Song &song1, const Song &song2) {
  return song1.PrettyTitleWithArtist().localeAwareCompare(song2.PrettyTitleWithArtist()) < 0;
}

void Song::SortSongsListAlphabetically(SongList *songs) {
  Q_ASSERT(songs);
  std::sort(songs->begin(), songs->end(), CompareSongsName);
}

void Song::Init(const QString &title, const QString &artist, const QString &album, qint64 length_nanosec) {

  d->valid_ = true;

  set_title(title);
  set_artist(artist);
  set_album(album);

  set_length_nanosec(length_nanosec);

}

void Song::Init(const QString &title, const QString &artist, const QString &album, qint64 beginning, qint64 end) {

  d->valid_ = true;

  set_title(title);
  set_artist(artist);
  set_album(album);

  d->beginning_ = beginning;
  d->end_ = end;

}

void Song::set_genre_id3(int id) {
  set_genre(TStringToQString(TagLib::ID3v1::genre(id)));
}

void Song::InitFromProtobuf(const pb::tagreader::SongMetadata &pb) {

  if (d->source_ == Source_Unknown) d->source_ = Source_LocalFile;

  d->init_from_file_ = true;
  d->valid_ = pb.valid();
  set_title(QStringFromStdString(pb.title()));
  set_album(QStringFromStdString(pb.album()));
  set_artist(QStringFromStdString(pb.artist()));
  set_albumartist(QStringFromStdString(pb.albumartist()));
  d->track_ = pb.track();
  d->disc_ = pb.disc();
  d->year_ = pb.year();
  d->originalyear_ = pb.originalyear();
  d->genre_ = QStringFromStdString(pb.genre());
  d->compilation_ = pb.compilation();
  d->composer_ = QStringFromStdString(pb.composer());
  d->performer_ = QStringFromStdString(pb.performer());
  d->grouping_ = QStringFromStdString(pb.grouping());
  d->comment_ = QStringFromStdString(pb.comment());
  d->lyrics_ = QStringFromStdString(pb.lyrics());
  set_length_nanosec(pb.length_nanosec());
  d->bitrate_ = pb.bitrate();
  d->samplerate_ = pb.samplerate();
  d->bitdepth_ = pb.bitdepth();
  set_url(QUrl::fromEncoded(QByteArray(pb.url().data(), pb.url().size())));
  d->basefilename_ = QStringFromStdString(pb.basefilename());
  d->filetype_ = static_cast<FileType>(pb.filetype());
  d->filesize_ = pb.filesize();
  d->mtime_ = pb.mtime();
  d->ctime_ = pb.ctime();
  d->skipcount_ = pb.skipcount();
  d->lastplayed_ = pb.lastplayed();
  d->suspicious_tags_ = pb.suspicious_tags();

  if (pb.has_playcount()) {
    d->playcount_ = pb.playcount();
  }

  if (pb.has_art_automatic()) {
    set_art_automatic(QUrl::fromLocalFile(QByteArray(pb.art_automatic().data(), pb.art_automatic().size())));
  }

  InitArtManual();

}

void Song::ToProtobuf(pb::tagreader::SongMetadata *pb) const {

  const QByteArray url(d->url_.toEncoded());
  const QByteArray art_automatic(d->art_automatic_.toEncoded());

  pb->set_valid(d->valid_);
  pb->set_title(DataCommaSizeFromQString(d->title_));
  pb->set_album(DataCommaSizeFromQString(d->album_));
  pb->set_artist(DataCommaSizeFromQString(d->artist_));
  pb->set_albumartist(DataCommaSizeFromQString(d->albumartist_));
  pb->set_composer(DataCommaSizeFromQString(d->composer_));
  pb->set_performer(DataCommaSizeFromQString(d->performer_));
  pb->set_grouping(DataCommaSizeFromQString(d->grouping_));
  pb->set_lyrics(DataCommaSizeFromQString(d->lyrics_));
  pb->set_track(d->track_);
  pb->set_disc(d->disc_);
  pb->set_year(d->year_);
  pb->set_originalyear(d->originalyear_);
  pb->set_genre(DataCommaSizeFromQString(d->genre_));
  pb->set_comment(DataCommaSizeFromQString(d->comment_));
  pb->set_compilation(d->compilation_);
  pb->set_playcount(d->playcount_);
  pb->set_skipcount(d->skipcount_);
  pb->set_lastplayed(d->lastplayed_);
  pb->set_length_nanosec(length_nanosec());
  pb->set_bitrate(d->bitrate_);
  pb->set_samplerate(d->samplerate_);
  pb->set_bitdepth(d->bitdepth_);
  pb->set_url(url.constData(), url.size());
  pb->set_basefilename(DataCommaSizeFromQString(d->basefilename_));
  pb->set_mtime(d->mtime_);
  pb->set_ctime(d->ctime_);
  pb->set_filesize(d->filesize_);
  pb->set_suspicious_tags(d->suspicious_tags_);
  pb->set_art_automatic(art_automatic.constData(), art_automatic.size());
  pb->set_filetype(static_cast<pb::tagreader::SongMetadata_FileType>(d->filetype_));

}

#define tostr(n) (q.value(n).isNull() ? QString() : q.value(n).toString())
#define toint(n) (q.value(n).isNull() ? -1 : q.value(n).toInt())
#define tolonglong(n) (q.value(n).isNull() ? -1 : q.value(n).toLongLong())
#define tofloat(n) (q.value(n).isNull() ? -1 : q.value(n).toDouble())

void Song::InitFromQuery(const SqlRow &q, bool reliable_metadata, int col) {

  //qLog(Debug) << "Song::kColumns.size():" << Song::kColumns.size() << "q.columns_.size():" << q.columns_.size() << "col:" << col;

  int x = col;
  d->id_ = toint(col);

  for (int i = 0 ; i < Song::kColumns.size(); i++) {
    x++;

    if (x >= q.columns_.size()) {
      qLog(Error) << "Skipping" << Song::kColumns.value(i);
      break;
    }

    //qLog(Debug) << "Index:" << i << x << Song::kColumns.value(i) << q.value(x).toString();

    if (Song::kColumns.value(i) == "title") {
      set_title(tostr(x));
    }
    else if (Song::kColumns.value(i) == "album") {
      set_album(tostr(x));
    }
    else if (Song::kColumns.value(i) == "artist") {
      set_artist(tostr(x));
    }
    else if (Song::kColumns.value(i) == "albumartist") {
      set_albumartist(tostr(x));
    }
    else if (Song::kColumns.value(i) == "track") {
      d->track_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "disc") {
      d->disc_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "year") {
      d->year_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "originalyear") {
      d->originalyear_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "genre") {
      d->genre_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "compilation") {
      d->compilation_ = q.value(x).toBool();
    }
    else if (Song::kColumns.value(i) == "composer") {
      d->composer_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "performer") {
      d->performer_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "grouping") {
      d->grouping_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "comment") {
      d->comment_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "lyrics") {
      d->lyrics_ = tostr(x);
    }

    else if (Song::kColumns.value(i) == "artist_id") {
      d->artist_id_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "album_id") {
      d->album_id_ = tostr(x);
    }
    else if (Song::kColumns.value(i) == "song_id") {
      d->song_id_ = tostr(x);
    }

    else if (Song::kColumns.value(i) == "beginning") {
      d->beginning_ = q.value(x).isNull() ? 0 : q.value(x).toLongLong();
    }
    else if (Song::kColumns.value(i) == "length") {
      set_length_nanosec(tolonglong(x));
    }

    else if (Song::kColumns.value(i) == "bitrate") {
      d->bitrate_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "samplerate") {
      d->samplerate_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "bitdepth") {
      d->bitdepth_ = toint(x);
    }

    else if (Song::kColumns.value(i) == "source") {
      d->source_ = Source(q.value(x).toInt());
    }
    else if (Song::kColumns.value(i) == "directory_id") {
      d->directory_id_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "url") {
     set_url(QUrl::fromEncoded(tostr(x).toUtf8()));
     d->basefilename_ = QFileInfo(d->url_.toLocalFile()).fileName();
    }
    else if (Song::kColumns.value(i) == "filetype") {
      d->filetype_ = FileType(q.value(x).toInt());
    }
    else if (Song::kColumns.value(i) == "filesize") {
      d->filesize_ = toint(x);
    }
    else if (Song::kColumns.value(i) == "mtime") {
      d->mtime_ = tolonglong(x);
    }
    else if (Song::kColumns.value(i) == "ctime") {
      d->ctime_ = tolonglong(x);
    }
    else if (Song::kColumns.value(i) == "unavailable") {
      d->unavailable_ = q.value(x).toBool();
    }

    else if (Song::kColumns.value(i) == "playcount") {
      d->playcount_ = q.value(x).isNull() ? 0 : q.value(x).toInt();
    }
    else if (Song::kColumns.value(i) == "skipcount") {
      d->skipcount_ = q.value(x).isNull() ? 0 : q.value(x).toInt();
    }
    else if (Song::kColumns.value(i) == "lastplayed") {
      d->lastplayed_ = toint(x);
    }

    else if (Song::kColumns.value(i) == "compilation_detected") {
      d->compilation_detected_ = q.value(x).toBool();
    }
    else if (Song::kColumns.value(i) == "compilation_on") {
      d->compilation_on_ = q.value(x).toBool();
    }
    else if (Song::kColumns.value(i) == "compilation_off") {
      d->compilation_off_ = q.value(x).toBool();
    }
    else if (Song::kColumns.value(i) == "compilation_effective") {
    }

    else if (Song::kColumns.value(i) == "art_automatic") {
      QString art_automatic = tostr(x);
      if (art_automatic.contains(QRegularExpression("..+:.*"))) {
        set_art_automatic(QUrl::fromEncoded(art_automatic.toUtf8()));
      }
      else {
        set_art_automatic(QUrl::fromLocalFile(art_automatic));
      }
    }
    else if (Song::kColumns.value(i) == "art_manual") {
      QString art_manual = tostr(x);
      if (art_manual.contains(QRegularExpression("..+:.*"))) {
        set_art_manual(QUrl::fromEncoded(art_manual.toUtf8()));
      }
      else {
        set_art_manual(QUrl::fromLocalFile(art_manual));
      }
    }

    else if (Song::kColumns.value(i) == "effective_albumartist") {
    }
    else if (Song::kColumns.value(i) == "effective_originalyear") {
    }

    else if (Song::kColumns.value(i) == "cue_path") {
      d->cue_path_ = tostr(x);
    }

    else if (Song::kColumns.value(i) == "rating") {
      d->rating_ = tofloat(x);
    }

    else {
      qLog(Error) << "Forgot to handle" << Song::kColumns.value(i);
    }
  }

  d->valid_ = true;
  d->init_from_file_ = reliable_metadata;

  InitArtManual();

#undef tostr
#undef toint
#undef tolonglong
#undef tofloat

}

void Song::InitFromFilePartial(const QString &filename) {

  set_url(QUrl::fromLocalFile(filename));
  QFileInfo info(filename);
  d->basefilename_ = info.fileName();
  QString suffix = info.suffix().toLower();

  TagLib::FileRef fileref(filename.toUtf8().constData());
  if (fileref.file()) {
    d->valid_ = true;
    d->source_ = Source_LocalFile;
    if (d->art_manual_.isEmpty()) InitArtManual();
  }
  else {
    d->valid_ = false;
    d->error_ = QObject::tr("File %1 is not recognized as a valid audio file.").arg(filename);
  }

}

void Song::InitArtManual() {

  QString album = effective_album();
  album.remove(Song::kAlbumRemoveDisc);

  // If we don't have an art, check if we have one in the cache
  if (d->art_manual_.isEmpty() && d->art_automatic_.isEmpty() && !effective_albumartist().isEmpty() && !album.isEmpty()) {
    QString filename(Utilities::Sha1CoverHash(effective_albumartist(), album).toHex() + ".jpg");
    QString path(ImageCacheDir(d->source_) + "/" + filename);
    if (QFile::exists(path)) {
      d->art_manual_ = QUrl::fromLocalFile(path);
    }
    else if (d->url_.isLocalFile()) { // Pick the first image file in the album directory.
      QFileInfo file(d->url_.toLocalFile());
      QDir dir(file.path());
      QStringList files = dir.entryList(QStringList() << "*.jpg" << "*.png" << "*.gif" << "*.jpeg", QDir::Files|QDir::Readable, QDir::Name);
      if (files.count() > 0) {
        d->art_manual_ = QUrl::fromLocalFile(file.path() + QDir::separator() + files.first());
      }
    }
  }

}

#ifdef HAVE_LIBGPOD
void Song::InitFromItdb(Itdb_Track *track, const QString &prefix) {

  d->valid_ = true;

  set_title(QString::fromUtf8(track->title));
  set_album(QString::fromUtf8(track->album));
  set_artist(QString::fromUtf8(track->artist));
  set_albumartist(QString::fromUtf8(track->albumartist));
  d->track_ = track->track_nr;
  d->disc_ = track->cd_nr;
  d->year_ = track->year;
  d->genre_ = QString::fromUtf8(track->genre);
  d->compilation_ = track->compilation;
  d->composer_ = QString::fromUtf8(track->composer);
  d->grouping_ = QString::fromUtf8(track->grouping);
  d->comment_ = QString::fromUtf8(track->comment);

  set_length_nanosec(track->tracklen * kNsecPerMsec);

  d->bitrate_ = track->bitrate;
  d->samplerate_ = track->samplerate;
  d->bitdepth_ = -1; //track->bitdepth;

  d->source_ = Source_Device;
  QString filename = QString::fromLocal8Bit(track->ipod_path);
  filename.replace(':', '/');
  if (prefix.contains("://")) {
    set_url(QUrl(prefix + filename));
  }
  else {
    set_url(QUrl::fromLocalFile(prefix + filename));
  }
  d->basefilename_ = QFileInfo(filename).fileName();

  d->filetype_ = track->type2 ? FileType_MPEG : FileType_MP4;
  d->filesize_ = track->size;
  d->mtime_ = track->time_modified;
  d->ctime_ = track->time_added;

  d->playcount_ = track->playcount;
  d->skipcount_ = track->skipcount;
  d->lastplayed_ = track->time_played;

  if (itdb_track_has_thumbnails(track) && !d->artist_.isEmpty() && !d->title_.isEmpty()) {
    GdkPixbuf *pixbuf = static_cast<GdkPixbuf*>(itdb_track_get_thumbnail(track, -1, -1));
    if (pixbuf) {
      QString cover_path = ImageCacheDir(Source_Device);
      QDir dir(cover_path);
      if (!dir.exists()) dir.mkpath(cover_path);
      QString cover_file = cover_path + "/" + Utilities::Sha1CoverHash(effective_albumartist(), effective_album()).toHex() + ".jpg";
      GError *error = nullptr;
      if (dir.exists() && gdk_pixbuf_save(pixbuf, cover_file.toUtf8().constData(), "jpeg", &error, nullptr)) {
        d->art_manual_ = QUrl::fromLocalFile(cover_file);
      }
      g_object_unref(pixbuf);
    }
  }

}

void Song::ToItdb(Itdb_Track *track) const {

  track->title = strdup(d->title_.toUtf8().constData());
  track->album = strdup(d->album_.toUtf8().constData());
  track->artist = strdup(d->artist_.toUtf8().constData());
  track->albumartist = strdup(d->albumartist_.toUtf8().constData());
  track->track_nr = d->track_;
  track->cd_nr = d->disc_;
  track->year = d->year_;
  track->genre = strdup(d->genre_.toUtf8().constData());
  track->compilation = d->compilation_;
  track->composer = strdup(d->composer_.toUtf8().constData());
  track->grouping = strdup(d->grouping_.toUtf8().constData());
  track->comment = strdup(d->comment_.toUtf8().constData());

  track->tracklen = length_nanosec() / kNsecPerMsec;

  track->bitrate = d->bitrate_;
  track->samplerate = d->samplerate_;

  track->type1 = (d->filetype_ == FileType_MPEG ? 1 : 0);
  track->type2 = (d->filetype_ == FileType_MPEG ? 1 : 0);
  track->mediatype = 1;              // Audio
  track->size = d->filesize_;
  track->time_modified = d->mtime_;
  track->time_added = d->ctime_;

  track->playcount = d->playcount_;
  track->skipcount = d->skipcount_;
  track->time_played = d->lastplayed_;

}
#endif

#ifdef HAVE_LIBMTP
void Song::InitFromMTP(const LIBMTP_track_t *track, const QString &host) {

  d->valid_ = true;
  d->source_ = Source_Device;

  set_title(QString::fromUtf8(track->title));
  set_artist(QString::fromUtf8(track->artist));
  set_album(QString::fromUtf8(track->album));
  d->genre_ = QString::fromUtf8(track->genre);
  d->composer_ = QString::fromUtf8(track->composer);
  d->track_ = track->tracknumber;

  d->url_ = QUrl(QString("mtp://%1/%2").arg(host, QString::number(track->item_id)));
  d->basefilename_ = QString::number(track->item_id);
  d->filesize_ = track->filesize;
  d->mtime_ = track->modificationdate;
  d->ctime_ = track->modificationdate;

  set_length_nanosec(track->duration * kNsecPerMsec);

  d->samplerate_ = track->samplerate;
  d->bitdepth_ = 0;
  d->bitrate_ = track->bitrate;

  d->playcount_ = track->usecount;

  switch (track->filetype) {
      case LIBMTP_FILETYPE_WAV:  d->filetype_ = FileType_WAV;       break;
      case LIBMTP_FILETYPE_MP3:  d->filetype_ = FileType_MPEG;      break;
      case LIBMTP_FILETYPE_WMA:  d->filetype_ = FileType_ASF;       break;
      case LIBMTP_FILETYPE_OGG:  d->filetype_ = FileType_OggVorbis; break;
      case LIBMTP_FILETYPE_MP4:  d->filetype_ = FileType_MP4;       break;
      case LIBMTP_FILETYPE_AAC:  d->filetype_ = FileType_MP4;       break;
      case LIBMTP_FILETYPE_FLAC: d->filetype_ = FileType_OggFlac;   break;
      case LIBMTP_FILETYPE_MP2:  d->filetype_ = FileType_MPEG;      break;
      case LIBMTP_FILETYPE_M4A:  d->filetype_ = FileType_MP4;       break;
      default:
        d->filetype_ = FileType_Unknown;
        d->valid_ = false;
        break;
  }

}

void Song::ToMTP(LIBMTP_track_t *track) const {

  track->item_id = 0;
  track->parent_id = 0;
  track->storage_id = 0;

  track->title = strdup(d->title_.toUtf8().constData());
  track->artist = strdup(effective_albumartist().toUtf8().constData());
  track->album = strdup(d->album_.toUtf8().constData());
  track->genre = strdup(d->genre_.toUtf8().constData());
  track->date = nullptr;
  track->tracknumber = d->track_;
  if (d->composer_.isEmpty())
    track->composer = nullptr;
  else
    track->composer = strdup(d->composer_.toUtf8().constData());

  track->filename = strdup(d->basefilename_.toUtf8().constData());

  track->filesize = d->filesize_;
  track->modificationdate = d->mtime_;

  track->duration = length_nanosec() / kNsecPerMsec;

  track->bitrate = d->bitrate_;
  track->bitratetype = 0;
  track->samplerate = d->samplerate_;
  track->nochannels = 0;
  track->wavecodec = 0;

  track->usecount = d->playcount_;

  switch (d->filetype_) {
    case FileType_ASF:       track->filetype = LIBMTP_FILETYPE_ASF;         break;
    case FileType_MP4:       track->filetype = LIBMTP_FILETYPE_MP4;         break;
    case FileType_MPEG:      track->filetype = LIBMTP_FILETYPE_MP3;         break;
    case FileType_FLAC:
    case FileType_OggFlac:   track->filetype = LIBMTP_FILETYPE_FLAC;        break;
    case FileType_OggSpeex:
    case FileType_OggVorbis: track->filetype = LIBMTP_FILETYPE_OGG;         break;
    case FileType_WAV:       track->filetype = LIBMTP_FILETYPE_WAV;         break;
    default:                 track->filetype = LIBMTP_FILETYPE_UNDEF_AUDIO; break;
  }

}
#endif

bool Song::MergeFromSimpleMetaBundle(const Engine::SimpleMetaBundle &bundle) {

  d->valid_ = true;

  bool minor = true;

  if (d->init_from_file_ || is_collection_song() || d->url_.isLocalFile()) {
    // This Song was already loaded using taglib. Our tags are probably better than the engine's.
    if (title() != bundle.title && title().isEmpty() && !bundle.title.isEmpty()) {
      set_title(bundle.title);
      minor = false;
    }
    if (artist() != bundle.artist && artist().isEmpty() && !bundle.artist.isEmpty()) {
      set_artist(bundle.artist);
      minor = false;
    }
    if (album() != bundle.album && album().isEmpty() && !bundle.album.isEmpty()) {
      set_album(bundle.album);
      minor = false;
    }
    if (comment().isEmpty() && !bundle.comment.isEmpty()) set_comment(bundle.comment);
    if (genre().isEmpty() && !bundle.genre.isEmpty()) set_genre(bundle.genre);
    if (lyrics().isEmpty() && !bundle.lyrics.isEmpty()) set_lyrics(bundle.lyrics);
  }
  else {
    if (title() != bundle.title && !bundle.title.isEmpty()) {
      set_title(bundle.title);
      minor = false;
    }
    if (artist() != bundle.artist && !bundle.artist.isEmpty()) {
      set_artist(bundle.artist);
      minor = false;
    }
    if (album() != bundle.album && !bundle.album.isEmpty()) {
      set_album(bundle.album);
      minor = false;
    }
    if (!bundle.comment.isEmpty()) set_comment(bundle.comment);
    if (!bundle.genre.isEmpty()) set_genre(bundle.genre);
    if (!bundle.lyrics.isEmpty()) set_lyrics(bundle.lyrics);
  }

  if (bundle.length > 0) set_length_nanosec(bundle.length);
  if (bundle.year > 0) d->year_ = bundle.year;
  if (bundle.track > 0) d->track_ = bundle.track;
  if (bundle.filetype != FileType_Unknown) d->filetype_ = bundle.filetype;
  if (bundle.samplerate > 0) d->samplerate_ = bundle.samplerate;
  if (bundle.bitdepth > 0) d->bitdepth_ = bundle.bitdepth;
  if (bundle.bitrate > 0) d->bitrate_ = bundle.bitrate;

  return minor;

}

void Song::BindToQuery(QSqlQuery *query) const {

#define strval(x) ((x).isNull() ? "" : (x))
#define intval(x) ((x) <= 0 ? -1 : (x))
#define notnullintval(x) ((x) == -1 ? QVariant() : (x))

  // Remember to bind these in the same order as kBindSpec

  query->bindValue(":title", strval(d->title_));
  query->bindValue(":album", strval(d->album_));
  query->bindValue(":artist", strval(d->artist_));
  query->bindValue(":albumartist", strval(d->albumartist_));
  query->bindValue(":track", intval(d->track_));
  query->bindValue(":disc", intval(d->disc_));
  query->bindValue(":year", intval(d->year_));
  query->bindValue(":originalyear", intval(d->originalyear_));
  query->bindValue(":genre", strval(d->genre_));
  query->bindValue(":compilation", d->compilation_ ? 1 : 0);
  query->bindValue(":composer", strval(d->composer_));
  query->bindValue(":performer", strval(d->performer_));
  query->bindValue(":grouping", strval(d->grouping_));
  query->bindValue(":comment", strval(d->comment_));
  query->bindValue(":lyrics", strval(d->lyrics_));

  query->bindValue(":artist_id", strval(d->artist_id_));
  query->bindValue(":album_id", strval(d->album_id_));
  query->bindValue(":song_id", strval(d->song_id_));

  query->bindValue(":beginning", d->beginning_);
  query->bindValue(":length", intval(length_nanosec()));

  query->bindValue(":bitrate", intval(d->bitrate_));
  query->bindValue(":samplerate", intval(d->samplerate_));
  query->bindValue(":bitdepth", intval(d->bitdepth_));

  query->bindValue(":source", d->source_);
  query->bindValue(":directory_id", notnullintval(d->directory_id_));
  query->bindValue(":url", d->url_.toString(QUrl::FullyEncoded));
  query->bindValue(":filetype", d->filetype_);
  query->bindValue(":filesize", notnullintval(d->filesize_));
  query->bindValue(":mtime", notnullintval(d->mtime_));
  query->bindValue(":ctime", notnullintval(d->ctime_));
  query->bindValue(":unavailable", d->unavailable_ ? 1 : 0);

  query->bindValue(":playcount", d->playcount_);
  query->bindValue(":skipcount", d->skipcount_);
  query->bindValue(":lastplayed", intval(d->lastplayed_));

  query->bindValue(":compilation_detected", d->compilation_detected_ ? 1 : 0);
  query->bindValue(":compilation_on", d->compilation_on_ ? 1 : 0);
  query->bindValue(":compilation_off", d->compilation_off_ ? 1 : 0);
  query->bindValue(":compilation_effective", is_compilation() ? 1 : 0);

  query->bindValue(":art_automatic", d->art_automatic_.toString(QUrl::FullyEncoded));
  query->bindValue(":art_manual", d->art_manual_.toString(QUrl::FullyEncoded));

  query->bindValue(":effective_albumartist", strval(this->effective_albumartist()));
  query->bindValue(":effective_originalyear", intval(this->effective_originalyear()));

  query->bindValue(":cue_path", d->cue_path_);

  query->bindValue(":rating", intval(d->rating_));

#undef intval
#undef notnullintval
#undef strval

}

void Song::BindToFtsQuery(QSqlQuery *query) const {

  query->bindValue(":ftstitle", d->title_);
  query->bindValue(":ftsalbum", d->album_);
  query->bindValue(":ftsartist", d->artist_);
  query->bindValue(":ftsalbumartist", d->albumartist_);
  query->bindValue(":ftscomposer", d->composer_);
  query->bindValue(":ftsperformer", d->performer_);
  query->bindValue(":ftsgrouping", d->grouping_);
  query->bindValue(":ftsgenre", d->genre_);
  query->bindValue(":ftscomment", d->comment_);

}

QString Song::PrettyTitle() const {

  QString title(d->title_);

  if (title.isEmpty()) title = d->basefilename_;
  if (title.isEmpty()) title = d->url_.toString();

  return title;

}

QString Song::PrettyTitleWithArtist() const {

  QString title(PrettyTitle());

  if (!d->artist_.isEmpty()) title = d->artist_ + " - " + title;

  return title;

}

QString Song::PrettyLength() const {

  if (length_nanosec() == -1) return QString();

  return Utilities::PrettyTimeNanosec(length_nanosec());

}

QString Song::PrettyYear() const {

  if (d->year_ == -1) return QString();

  return QString::number(d->year_);

}

QString Song::TitleWithCompilationArtist() const {

  QString title(d->title_);

  if (title.isEmpty()) title = d->basefilename_;

  if (is_compilation() && !d->artist_.isEmpty() && !d->artist_.toLower().contains("various")) title = d->artist_ + " - " + title;

  return title;

}

QString Song::SampleRateBitDepthToText() const {

  if (d->samplerate_ == -1) return QString("");
  if (d->bitdepth_ == -1) return QString("%1 hz").arg(d->samplerate_);

  return QString("%1 hz / %2 bit").arg(d->samplerate_).arg(d->bitdepth_);

}

QString Song::PrettyRating() const {

  float rating = d->rating_;

  if (rating == -1.0f) return "0";

  return QString::number(static_cast<int>(rating * 100));

}

bool Song::IsMetadataEqual(const Song &other) const {

  return d->title_ == other.d->title_ &&
         d->album_ == other.d->album_ &&
         d->artist_ == other.d->artist_ &&
         d->albumartist_ == other.d->albumartist_ &&
         d->track_ == other.d->track_ &&
         d->disc_ == other.d->disc_ &&
         d->year_ == other.d->year_ &&
         d->originalyear_ == other.d->originalyear_ &&
         d->genre_ == other.d->genre_ &&
         d->compilation_ == other.d->compilation_ &&
         d->composer_ == other.d->composer_ &&
         d->performer_ == other.d->performer_ &&
         d->grouping_ == other.d->grouping_ &&
         d->comment_ == other.d->comment_ &&
         d->lyrics_ == other.d->lyrics_ &&
         d->artist_id_ == other.d->artist_id_ &&
         d->album_id_ == other.d->album_id_ &&
         d->song_id_ == other.d->song_id_ &&
         d->beginning_ == other.d->beginning_ &&
         length_nanosec() == other.length_nanosec() &&
         d->bitrate_ == other.d->bitrate_ &&
         d->samplerate_ == other.d->samplerate_ &&
         d->bitdepth_ == other.d->bitdepth_ &&
         d->art_automatic_ == other.d->art_automatic_ &&
         d->art_manual_ == other.d->art_manual_ &&
         d->cue_path_ == other.d->cue_path_;
}

bool Song::IsEditable() const {
  return d->valid_ && !d->url_.isEmpty() && !is_stream() && d->source_ != Source_Unknown && d->filetype_ != FileType_Unknown && !has_cue();
}

bool Song::operator==(const Song &other) const {
  return source() == other.source() && url() == other.url() && beginning_nanosec() == other.beginning_nanosec();
}

bool Song::operator!=(const Song &other) const {
  return source() != other.source() || url() != other.url() || beginning_nanosec() != other.beginning_nanosec();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const Song &song) {
#else
uint qHash(const Song &song) {
#endif
  // Should compare the same fields as operator==
  return qHash(song.url().toString()) ^ qHash(song.beginning_nanosec());
}

bool Song::IsSimilar(const Song &other) const {
  return title().compare(other.title(), Qt::CaseInsensitive) == 0 &&
         artist().compare(other.artist(), Qt::CaseInsensitive) == 0 &&
         album().compare(other.album(), Qt::CaseInsensitive) == 0;
}

uint HashSimilar(const Song &song) {
  // Should compare the same fields as function IsSimilar
  return qHash(song.title().toLower()) ^ qHash(song.artist().toLower()) ^ qHash(song.album().toLower());
}

bool Song::IsOnSameAlbum(const Song &other) const {

  if (is_compilation() != other.is_compilation()) return false;

  if (has_cue() && other.has_cue() && cue_path() == other.cue_path())
    return true;

  if (is_compilation() && album() == other.album()) return true;

  return effective_album() == other.effective_album() && effective_albumartist() == other.effective_albumartist();

}

QString Song::AlbumKey() const {
  return QString("%1|%2|%3").arg(is_compilation() ? "_compilation" : effective_albumartist(), has_cue() ? cue_path() : "", effective_album());
}

void Song::ToXesam(QVariantMap *map) const {

  using mpris::AddMetadata;
  using mpris::AddMetadataAsList;
  using mpris::AsMPRISDateTimeType;

  AddMetadata("xesam:url", effective_stream_url().toString(), map);
  AddMetadata("xesam:title", PrettyTitle(), map);
  AddMetadataAsList("xesam:artist", artist(), map);
  AddMetadata("xesam:album", album(), map);
  AddMetadataAsList("xesam:albumArtist", albumartist(), map);
  AddMetadata("mpris:length", length_nanosec() / kNsecPerUsec, map);
  AddMetadata("xesam:trackNumber", track(), map);
  AddMetadataAsList("xesam:genre", genre(), map);
  AddMetadata("xesam:discNumber", disc(), map);
  AddMetadataAsList("xesam:comment", comment(), map);
  AddMetadata("xesam:contentCreated", AsMPRISDateTimeType(ctime()), map);
  AddMetadata("xesam:lastUsed", AsMPRISDateTimeType(lastplayed()), map);
  AddMetadataAsList("xesam:composer", composer(), map);
  AddMetadata("xesam:useCount", playcount(), map);

}

void Song::MergeUserSetData(const Song &other) {

  set_playcount(other.playcount());
  set_skipcount(other.skipcount());
  set_lastplayed(other.lastplayed());
  set_art_manual(other.art_manual());
  set_compilation_on(other.compilation_on());
  set_compilation_off(other.compilation_off());
  set_rating(other.rating());

}
