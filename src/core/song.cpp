/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QIcon>
#include <QStandardPaths>

#include "core/iconloader.h"

#include "engine/enginebase.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/cryptutils.h"
#include "utilities/timeconstants.h"
#include "song.h"
#include "application.h"
#include "sqlquery.h"
#include "mpris_common.h"
#include "sqlrow.h"
#include "tagreadermessages.pb.h"

#define QStringFromStdString(x) QString::fromUtf8((x).data(), (x).size())
#define DataCommaSizeFromQString(x) (x).toUtf8().constData(), (x).toUtf8().length()

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

                                                 << "fingerprint"

                                                 << "playcount"
                                                 << "skipcount"
                                                 << "lastplayed"
                                                 << "lastseen"

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

const QRegularExpression Song::kAlbumRemoveDisc(" ?-? ((\\(|\\[)?)(Disc|CD) ?([0-9]{1,2})((\\)|\\])?)$", QRegularExpression::CaseInsensitiveOption);
const QRegularExpression Song::kAlbumRemoveMisc(" ?-? ((\\(|\\[)?)(Remastered|([0-9]{1,4}) *Remaster|Explicit) ?((\\)|\\])?)$", QRegularExpression::CaseInsensitiveOption);
const QRegularExpression Song::kTitleRemoveMisc(" ?-? ((\\(|\\[)?)(Remastered|Remastered Version|([0-9]{1,4}) *Remaster) ?((\\)|\\])?)$", QRegularExpression::CaseInsensitiveOption);

const QStringList Song::kArticles = QStringList() << "the " << "a " << "an ";

const QStringList Song::kAcceptedExtensions = QStringList() << "wav" << "flac" << "wv" << "ogg" << "oga" << "opus" << "spx" << "ape" << "mpc"
                                                            << "mp2" << "mp3" <<  "m4a" << "mp4" << "aac" << "asf" << "asx" << "wma"
                                                            << "aif << aiff" << "mka" << "tta" << "dsf" << "dsd"
                                                            << "ac3" << "dts" << "spc" << "vgm";

struct Song::Private : public QSharedData {

  explicit Private(Source source = Source::Unknown);

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
  bool compilation_;  // From the file tag
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
  qint64 filesize_;
  qint64 mtime_;
  qint64 ctime_;
  bool unavailable_;

  QString fingerprint_;

  uint playcount_;
  uint skipcount_;
  qint64 lastplayed_;
  qint64 lastseen_;

  bool compilation_detected_;   // From the collection scanner
  bool compilation_on_;         // Set by the user
  bool compilation_off_;        // Set by the user

  // Filenames to album art for this song.
  QUrl art_automatic_;          // Guessed by CollectionWatcher
  QUrl art_manual_;             // Set by the user - should take priority

  QString cue_path_;            // If the song has a CUE, this contains it's path.

  float rating_;                // Database rating, initial rating read from tag.

  QUrl stream_url_;             // Temporary stream url set by url handler.
  bool init_from_file_;         // Whether this song was loaded from a file using taglib.
  bool suspicious_tags_;        // Whether our encoding guesser thinks these tags might be incorrectly encoded.

};

Song::Private::Private(const Source source)
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
      filetype_(FileType::Unknown),
      filesize_(-1),
      mtime_(-1),
      ctime_(-1),
      unavailable_(false),

      playcount_(0),
      skipcount_(0),
      lastplayed_(-1),
      lastseen_(-1),

      compilation_detected_(false),
      compilation_on_(false),
      compilation_off_(false),

      rating_(-1),

      init_from_file_(false),
      suspicious_tags_(false)

      {}

Song::Song(const Source source) : d(new Private(source)) {}
Song::Song(const Song &other) = default;
Song::~Song() = default;

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
qint64 Song::filesize() const { return d->filesize_; }
qint64 Song::mtime() const { return d->mtime_; }
qint64 Song::ctime() const { return d->ctime_; }

QString Song::fingerprint() const { return d->fingerprint_; }

uint Song::playcount() const { return d->playcount_; }
uint Song::skipcount() const { return d->skipcount_; }
qint64 Song::lastplayed() const { return d->lastplayed_; }
qint64 Song::lastseen() const { return d->lastseen_; }

bool Song::compilation_detected() const { return d->compilation_detected_; }
bool Song::compilation_off() const { return d->compilation_off_; }
bool Song::compilation_on() const { return d->compilation_on_; }

const QUrl &Song::art_automatic() const { return d->art_automatic_; }
const QUrl &Song::art_manual() const { return d->art_manual_; }
bool Song::has_manually_unset_cover() const { return d->art_manual_.path() == kManuallyUnsetCover; }
void Song::set_manually_unset_cover() { d->art_manual_ = QUrl::fromLocalFile(kManuallyUnsetCover); }
bool Song::has_embedded_cover() const { return d->art_automatic_.path() == kEmbeddedCover; }
void Song::set_embedded_cover() { d->art_automatic_ = QUrl::fromLocalFile(kEmbeddedCover); }

void Song::clear_art_automatic() { d->art_automatic_.clear(); }
void Song::clear_art_manual() { d->art_manual_.clear(); }

bool Song::additional_tags_supported() const {
  return d->filetype_ == FileType::FLAC ||
         d->filetype_ == FileType::WavPack ||
         d->filetype_ == FileType::OggFlac ||
         d->filetype_ == FileType::OggVorbis ||
         d->filetype_ == FileType::OggOpus ||
         d->filetype_ == FileType::OggSpeex ||
         d->filetype_ == FileType::MPEG ||
         d->filetype_ == FileType::MP4 ||
         d->filetype_ == FileType::MPC ||
         d->filetype_ == FileType::APE;
}

bool Song::albumartist_supported() const {
  return additional_tags_supported();
}

bool Song::composer_supported() const {
  return additional_tags_supported();
}

bool Song::performer_supported() const {
  return d->filetype_ == FileType::FLAC ||
         d->filetype_ == FileType::WavPack ||
         d->filetype_ == FileType::OggFlac ||
         d->filetype_ == FileType::OggVorbis ||
         d->filetype_ == FileType::OggOpus ||
         d->filetype_ == FileType::OggSpeex ||
         d->filetype_ == FileType::MPEG ||
         d->filetype_ == FileType::MPC ||
         d->filetype_ == FileType::APE;
}

bool Song::grouping_supported() const {
  return additional_tags_supported();
}

bool Song::genre_supported() const {
  return additional_tags_supported();
}

bool Song::compilation_supported() const {
  return additional_tags_supported();
}

bool Song::rating_supported() const {
  return d->filetype_ == FileType::FLAC ||
         d->filetype_ == FileType::WavPack ||
         d->filetype_ == FileType::OggFlac ||
         d->filetype_ == FileType::OggVorbis ||
         d->filetype_ == FileType::OggOpus ||
         d->filetype_ == FileType::OggSpeex ||
         d->filetype_ == FileType::MPEG ||
         d->filetype_ == FileType::MP4 ||
         d->filetype_ == FileType::ASF ||
         d->filetype_ == FileType::MPC ||
         d->filetype_ == FileType::APE;
}

bool Song::comment_supported() const {
  return additional_tags_supported();
}

bool Song::lyrics_supported() const {
  return additional_tags_supported();
}

bool Song::save_embedded_cover_supported(const FileType filetype) {

  return filetype == FileType::FLAC ||
         filetype == FileType::OggVorbis ||
         filetype == FileType::OggOpus ||
         filetype == FileType::MPEG ||
         filetype == FileType::MP4;

}

const QUrl &Song::stream_url() const { return d->stream_url_; }
const QUrl &Song::effective_stream_url() const { return !d->stream_url_.isEmpty() && d->stream_url_.isValid() ? d->stream_url_ : d->url_; }
bool Song::init_from_file() const { return d->init_from_file_; }

const QString &Song::cue_path() const { return d->cue_path_; }
bool Song::has_cue() const { return !d->cue_path_.isEmpty(); }

float Song::rating() const { return d->rating_; }

bool Song::is_collection_song() const { return d->source_ == Source::Collection; }
bool Song::is_metadata_good() const { return !d->url_.isEmpty() && !d->artist_.isEmpty() && !d->title_.isEmpty(); }
bool Song::is_stream() const { return is_radio() || d->source_ == Source::Tidal || d->source_ == Source::Subsonic || d->source_ == Source::Qobuz; }
bool Song::is_radio() const { return d->source_ == Source::Stream || d->source_ == Source::SomaFM || d->source_ == Source::RadioParadise; }
bool Song::is_cdda() const { return d->source_ == Source::CDDA; }
bool Song::is_compilation() const { return (d->compilation_ || d->compilation_detected_ || d->compilation_on_) && !d->compilation_off_; }
bool Song::stream_url_can_expire() const { return d->source_ == Source::Tidal || d->source_ == Source::Qobuz; }
bool Song::is_module_music() const { return d->filetype_ == FileType::MOD || d->filetype_ == FileType::S3M || d->filetype_ == FileType::XM || d->filetype_ == FileType::IT; }

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

bool Song::has_valid_art() const { return art_automatic_is_valid() || art_manual_is_valid(); }

void Song::set_id(const int id) { d->id_ = id; }
void Song::set_valid(const bool v) { d->valid_ = v; }

void Song::set_artist_id(const QString &v) { d->artist_id_ = v; }
void Song::set_album_id(const QString &v) { d->album_id_ = v; }
void Song::set_song_id(const QString &v) { d->song_id_ = v; }

QString Song::sortable(const QString &v) {

  QString copy = v.toLower();

  for (const auto &i : kArticles) {
    if (copy.startsWith(i)) {
      qint64 ilen = i.length();
      return copy.right(copy.length() - ilen) + ", " + copy.left(ilen - 1);
    }
  }

  return copy;
}

void Song::set_title(const QString &v) { d->title_sortable_ = sortable(v); d->title_ = v; }
void Song::set_album(const QString &v) { d->album_sortable_ = sortable(v); d->album_ = v; }
void Song::set_artist(const QString &v) { d->artist_sortable_ = sortable(v); d->artist_ = v; }
void Song::set_albumartist(const QString &v) { d->albumartist_sortable_ = sortable(v); d->albumartist_ = v; }
void Song::set_track(const int v) { d->track_ = v; }
void Song::set_disc(const int v) { d->disc_ = v; }
void Song::set_year(const int v) { d->year_ = v; }
void Song::set_originalyear(const int v) { d->originalyear_ = v; }
void Song::set_genre(const QString &v) { d->genre_ = v; }
void Song::set_compilation(bool v) { d->compilation_ = v; }
void Song::set_composer(const QString &v) { d->composer_ = v; }
void Song::set_performer(const QString &v) { d->performer_ = v; }
void Song::set_grouping(const QString &v) { d->grouping_ = v; }
void Song::set_comment(const QString &v) { d->comment_ = v; }
void Song::set_lyrics(const QString &v) { d->lyrics_ = v; }

void Song::set_beginning_nanosec(const qint64 v) { d->beginning_ = qMax(0LL, v); }
void Song::set_end_nanosec(const qint64 v) { d->end_ = v; }
void Song::set_length_nanosec(const qint64 v) { d->end_ = d->beginning_ + v; }

void Song::set_bitrate(const int v) { d->bitrate_ = v; }
void Song::set_samplerate(const int v) { d->samplerate_ = v; }
void Song::set_bitdepth(const int v) { d->bitdepth_ = v; }

void Song::set_source(const Source v) { d->source_ = v; }
void Song::set_directory_id(const int v) { d->directory_id_ = v; }
void Song::set_url(const QUrl &v) { d->url_ = v; }
void Song::set_basefilename(const QString &v) { d->basefilename_ = v; }
void Song::set_filetype(const FileType v) { d->filetype_ = v; }
void Song::set_filesize(const qint64 v) { d->filesize_ = v; }
void Song::set_mtime(const qint64 v) { d->mtime_ = v; }
void Song::set_ctime(const qint64 v) { d->ctime_ = v; }
void Song::set_unavailable(const bool v) { d->unavailable_ = v; }

void Song::set_fingerprint(const QString &v) { d->fingerprint_ = v; }

void Song::set_playcount(const uint v) { d->playcount_ = v; }
void Song::set_skipcount(const uint v) { d->skipcount_ = v; }
void Song::set_lastplayed(const qint64 v) { d->lastplayed_ = v; }
void Song::set_lastseen(const qint64 v) { d->lastseen_ = v; }

void Song::set_compilation_detected(const bool v) { d->compilation_detected_ = v; }
void Song::set_compilation_on(const bool v) { d->compilation_on_ = v; }
void Song::set_compilation_off(const bool v) { d->compilation_off_ = v; }

void Song::set_art_automatic(const QUrl &v) { d->art_automatic_ = v; }
void Song::set_art_manual(const QUrl &v) { d->art_manual_ = v; }
void Song::set_cue_path(const QString &v) { d->cue_path_ = v; }

void Song::set_rating(const float v) { d->rating_ = v; }

void Song::set_stream_url(const QUrl &v) { d->stream_url_ = v; }

QString Song::JoinSpec(const QString &table) {
  return Utilities::Prepend(table + ".", kColumns).join(", ");
}

Song::Source Song::SourceFromURL(const QUrl &url) {

  if (url.isLocalFile()) return Source::LocalFile;
  else if (url.scheme() == "cdda") return Source::CDDA;
  else if (url.scheme() == "tidal") return Source::Tidal;
  else if (url.scheme() == "subsonic") return Source::Subsonic;
  else if (url.scheme() == "qobuz") return Source::Qobuz;
  else if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "rtsp") {
    if (url.host().endsWith("tidal.com", Qt::CaseInsensitive)) { return Source::Tidal; }
    if (url.host().endsWith("qobuz.com", Qt::CaseInsensitive)) { return Source::Qobuz; }
    if (url.host().endsWith("somafm.com", Qt::CaseInsensitive)) { return Source::SomaFM; }
    if (url.host().endsWith("radioparadise.com", Qt::CaseInsensitive)) { return Source::RadioParadise; }
    return Source::Stream;
  }
  else return Source::Unknown;

}

QString Song::TextForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return "file";
    case Source::Collection:    return "collection";
    case Source::CDDA:          return "cd";
    case Source::Device:        return "device";
    case Source::Stream:        return "stream";
    case Source::Tidal:         return "tidal";
    case Source::Subsonic:      return "subsonic";
    case Source::Qobuz:         return "qobuz";
    case Source::SomaFM:        return "somafm";
    case Source::RadioParadise: return "radioparadise";
    case Source::Unknown:       return "unknown";
  }
  return "unknown";

}

QString Song::DescriptionForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return "File";
    case Source::Collection:    return "Collection";
    case Source::CDDA:          return "CD";
    case Source::Device:        return "Device";
    case Source::Stream:        return "Stream";
    case Source::Tidal:         return "Tidal";
    case Source::Subsonic:      return "Subsonic";
    case Source::Qobuz:         return "Qobuz";
    case Source::SomaFM:        return "SomaFM";
    case Source::RadioParadise: return "Radio Paradise";
    case Source::Unknown:       return "Unknown";
  }
  return "unknown";

}

Song::Source Song::SourceFromText(const QString &source) {

  if (source.compare("file", Qt::CaseInsensitive) == 0) return Source::LocalFile;
  if (source.compare("collection", Qt::CaseInsensitive) == 0) return Source::Collection;
  if (source.compare("cd", Qt::CaseInsensitive) == 0) return Source::CDDA;
  if (source.compare("device", Qt::CaseInsensitive) == 0) return Source::Device;
  if (source.compare("stream", Qt::CaseInsensitive) == 0) return Source::Stream;
  if (source.compare("tidal", Qt::CaseInsensitive) == 0) return Source::Tidal;
  if (source.compare("subsonic", Qt::CaseInsensitive) == 0) return Source::Subsonic;
  if (source.compare("qobuz", Qt::CaseInsensitive) == 0) return Source::Qobuz;
  if (source.compare("somafm", Qt::CaseInsensitive) == 0) return Source::SomaFM;
  if (source.compare("radioparadise", Qt::CaseInsensitive) == 0) return Source::RadioParadise;

  return Source::Unknown;

}

QIcon Song::IconForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return IconLoader::Load("folder-sound");
    case Source::Collection:    return IconLoader::Load("library-music");
    case Source::CDDA:          return IconLoader::Load("media-optical");
    case Source::Device:        return IconLoader::Load("device");
    case Source::Stream:        return IconLoader::Load("applications-internet");
    case Source::Tidal:         return IconLoader::Load("tidal");
    case Source::Subsonic:      return IconLoader::Load("subsonic");
    case Source::Qobuz:         return IconLoader::Load("qobuz");
    case Source::SomaFM:        return IconLoader::Load("somafm");
    case Source::RadioParadise: return IconLoader::Load("radioparadise");
    case Source::Unknown:       return IconLoader::Load("edit-delete");
  }
  return IconLoader::Load("edit-delete");

}

QString Song::TextForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return "Wav";
    case FileType::FLAC:        return "FLAC";
    case FileType::WavPack:     return "WavPack";
    case FileType::OggFlac:     return "Ogg FLAC";
    case FileType::OggVorbis:   return "Ogg Vorbis";
    case FileType::OggOpus:     return "Ogg Opus";
    case FileType::OggSpeex:    return "Ogg Speex";
    case FileType::MPEG:        return "MP3";
    case FileType::MP4:         return "MP4 AAC";
    case FileType::ASF:         return "Windows Media audio";
    case FileType::AIFF:        return "AIFF";
    case FileType::MPC:         return "MPC";
    case FileType::TrueAudio:   return "TrueAudio";
    case FileType::DSF:         return "DSF";
    case FileType::DSDIFF:      return "DSDIFF";
    case FileType::PCM:         return "PCM";
    case FileType::APE:         return "Monkey's Audio";
    case FileType::MOD:         return "Module Music Format";
    case FileType::S3M:         return "Module Music Format";
    case FileType::XM:          return "Module Music Format";
    case FileType::IT:          return "Module Music Format";
    case FileType::CDDA:        return "CDDA";
    case FileType::SPC:         return "SNES SPC700";
    case FileType::VGM:         return "VGM";
    case FileType::Stream:      return "Stream";
    case FileType::Unknown:
    default:                         return QObject::tr("Unknown");
  }

}

QString Song::ExtensionForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return "wav";
    case FileType::FLAC:        return "flac";
    case FileType::WavPack:     return "wv";
    case FileType::OggFlac:     return "flac";
    case FileType::OggVorbis:   return "ogg";
    case FileType::OggOpus:     return "opus";
    case FileType::OggSpeex:    return "spx";
    case FileType::MPEG:        return "mp3";
    case FileType::MP4:         return "mp4";
    case FileType::ASF:         return "wma";
    case FileType::AIFF:        return "aiff";
    case FileType::MPC:         return "mpc";
    case FileType::TrueAudio:   return "tta";
    case FileType::DSF:         return "dsf";
    case FileType::DSDIFF:      return "dsd";
    case FileType::APE:         return "ape";
    case FileType::MOD:         return "mod";
    case FileType::S3M:         return "s3m";
    case FileType::XM:          return "xm";
    case FileType::IT:          return "it";
    case FileType::SPC:         return "spc";
    case FileType::VGM:         return "vgm";
    case FileType::Unknown:
    default:                         return "dat";
  }

}

QIcon Song::IconForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return IconLoader::Load("wav");
    case FileType::FLAC:        return IconLoader::Load("flac");
    case FileType::WavPack:     return IconLoader::Load("wavpack");
    case FileType::OggFlac:     return IconLoader::Load("flac");
    case FileType::OggVorbis:   return IconLoader::Load("vorbis");
    case FileType::OggOpus:     return IconLoader::Load("opus");
    case FileType::OggSpeex:    return IconLoader::Load("speex");
    case FileType::MPEG:        return IconLoader::Load("mp3");
    case FileType::MP4:         return IconLoader::Load("mp4");
    case FileType::ASF:         return IconLoader::Load("wma");
    case FileType::AIFF:        return IconLoader::Load("aiff");
    case FileType::MPC:         return IconLoader::Load("mpc");
    case FileType::TrueAudio:   return IconLoader::Load("trueaudio");
    case FileType::DSF:         return IconLoader::Load("dsf");
    case FileType::DSDIFF:      return IconLoader::Load("dsd");
    case FileType::PCM:         return IconLoader::Load("pcm");
    case FileType::APE:         return IconLoader::Load("ape");
    case FileType::MOD:         return IconLoader::Load("mod");
    case FileType::S3M:         return IconLoader::Load("s3m");
    case FileType::XM:          return IconLoader::Load("xm");
    case FileType::IT:          return IconLoader::Load("it");
    case FileType::CDDA:        return IconLoader::Load("cd");
    case FileType::Stream:      return IconLoader::Load("applications-internet");
    case FileType::Unknown:
    default:                         return IconLoader::Load("edit-delete");
  }

}

bool Song::IsFileLossless() const {
  switch (filetype()) {
    case FileType::WAV:
    case FileType::FLAC:
    case FileType::OggFlac:
    case FileType::WavPack:
    case FileType::AIFF:
    case FileType::DSF:
    case FileType::DSDIFF:
    case FileType::APE:
    case FileType::TrueAudio:
    case FileType::PCM:
    case FileType::CDDA:
      return true;
    default:
      return false;
  }
}

Song::FileType Song::FiletypeByMimetype(const QString &mimetype) {

  if (mimetype.compare("audio/wav", Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-wav", Qt::CaseInsensitive) == 0) return FileType::WAV;
  else if (mimetype.compare("audio/x-flac", Qt::CaseInsensitive) == 0) return FileType::FLAC;
  else if (mimetype.compare("audio/x-wavpack", Qt::CaseInsensitive) == 0) return FileType::WavPack;
  else if (mimetype.compare("audio/x-vorbis", Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  else if (mimetype.compare("audio/x-opus", Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  else if (mimetype.compare("audio/x-speex", Qt::CaseInsensitive) == 0)  return FileType::OggSpeex;
  // Gstreamer returns audio/mpeg for both MP3 and MP4/AAC.
  // else if (mimetype.compare("audio/mpeg", Qt::CaseInsensitive) == 0) return FileType::MPEG;
  else if (mimetype.compare("audio/aac", Qt::CaseInsensitive) == 0) return FileType::MP4;
  else if (mimetype.compare("audio/x-wma", Qt::CaseInsensitive) == 0) return FileType::ASF;
  else if (mimetype.compare("audio/aiff", Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-aiff", Qt::CaseInsensitive) == 0) return FileType::AIFF;
  else if (mimetype.compare("application/x-project", Qt::CaseInsensitive) == 0) return FileType::MPC;
  else if (mimetype.compare("audio/x-dsf", Qt::CaseInsensitive) == 0) return FileType::DSF;
  else if (mimetype.compare("audio/x-dsd", Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  else if (mimetype.compare("audio/x-ape", Qt::CaseInsensitive) == 0 || mimetype.compare("application/x-ape", Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-ffmpeg-parsed-ape", Qt::CaseInsensitive) == 0) return FileType::APE;
  else if (mimetype.compare("audio/x-mod", Qt::CaseInsensitive) == 0) return FileType::MOD;
  else if (mimetype.compare("audio/x-s3m", Qt::CaseInsensitive) == 0) return FileType::S3M;
  else if (mimetype.compare("audio/x-spc", Qt::CaseInsensitive) == 0) return FileType::SPC;
  else if (mimetype.compare("audio/x-vgm", Qt::CaseInsensitive) == 0) return FileType::VGM;

  else return FileType::Unknown;

}

Song::FileType Song::FiletypeByDescription(const QString &text) {

  if (text.compare("WAV", Qt::CaseInsensitive) == 0) return FileType::WAV;
  else if (text.compare("Free Lossless Audio Codec (FLAC)", Qt::CaseInsensitive) == 0) return FileType::FLAC;
  else if (text.compare("Wavpack", Qt::CaseInsensitive) == 0) return FileType::WavPack;
  else if (text.compare("Vorbis", Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  else if (text.compare("Opus", Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  else if (text.compare("Speex", Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  else if (text.compare("MPEG-1 Layer 3 (MP3)", Qt::CaseInsensitive) == 0) return FileType::MPEG;
  else if (text.compare("MPEG-4 AAC", Qt::CaseInsensitive) == 0) return FileType::MP4;
  else if (text.compare("WMA", Qt::CaseInsensitive) == 0) return FileType::ASF;
  else if (text.compare("Audio Interchange File Format", Qt::CaseInsensitive) == 0) return FileType::AIFF;
  else if (text.compare("MPC", Qt::CaseInsensitive) == 0) return FileType::MPC;
  else if (text.compare("audio/x-dsf", Qt::CaseInsensitive) == 0) return FileType::DSF;
  else if (text.compare("audio/x-dsd", Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  else if (text.compare("audio/x-ffmpeg-parsed-ape", Qt::CaseInsensitive) == 0) return FileType::APE;
  else if (text.compare("Module Music Format (MOD)", Qt::CaseInsensitive) == 0) return FileType::MOD;
  else if (text.compare("Module Music Format (MOD)", Qt::CaseInsensitive) == 0) return FileType::S3M;
  else if (text.compare("SNES SPC700", Qt::CaseInsensitive) == 0) return FileType::SPC;
  else if (text.compare("VGM", Qt::CaseInsensitive) == 0) return FileType::VGM;
  else return FileType::Unknown;

}

Song::FileType Song::FiletypeByExtension(const QString &ext) {

  if (ext.compare("wav", Qt::CaseInsensitive) == 0 || ext.compare("wave", Qt::CaseInsensitive) == 0) return FileType::WAV;
  else if (ext.compare("flac", Qt::CaseInsensitive) == 0) return FileType::FLAC;
  else if (ext.compare("wavpack", Qt::CaseInsensitive) == 0 || ext.compare("wv", Qt::CaseInsensitive) == 0) return FileType::WavPack;
  else if (ext.compare("ogg", Qt::CaseInsensitive) == 0 || ext.compare("oga", Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  else if (ext.compare("opus", Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  else if (ext.compare("speex", Qt::CaseInsensitive) == 0 || ext.compare("spx", Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  else if (ext.compare("mp3", Qt::CaseInsensitive) == 0) return FileType::MPEG;
  else if (ext.compare("mp4", Qt::CaseInsensitive) == 0 || ext.compare("m4a", Qt::CaseInsensitive) == 0 || ext.compare("aac", Qt::CaseInsensitive) == 0) return FileType::MP4;
  else if (ext.compare("asf", Qt::CaseInsensitive) == 0 || ext.compare("wma", Qt::CaseInsensitive) == 0) return FileType::ASF;
  else if (ext.compare("aiff", Qt::CaseInsensitive) == 0 || ext.compare("aif", Qt::CaseInsensitive) == 0 || ext.compare("aifc", Qt::CaseInsensitive) == 0) return FileType::AIFF;
  else if (ext.compare("mpc", Qt::CaseInsensitive) == 0 || ext.compare("mp+", Qt::CaseInsensitive) == 0 || ext.compare("mpp", Qt::CaseInsensitive) == 0) return FileType::MPC;
  else if (ext.compare("dsf", Qt::CaseInsensitive) == 0) return FileType::DSF;
  else if (ext.compare("dsd", Qt::CaseInsensitive) == 0 || ext.compare("dff", Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  else if (ext.compare("ape", Qt::CaseInsensitive) == 0) return FileType::APE;
  else if (ext.compare("mod", Qt::CaseInsensitive) == 0 ||
           ext.compare("module", Qt::CaseInsensitive) == 0 ||
           ext.compare("nst", Qt::CaseInsensitive) == 0||
           ext.compare("wow", Qt::CaseInsensitive) == 0) return FileType::MOD;
  else if (ext.compare("s3m", Qt::CaseInsensitive) == 0) return FileType::S3M;
  else if (ext.compare("xm", Qt::CaseInsensitive) == 0) return FileType::XM;
  else if (ext.compare("it", Qt::CaseInsensitive) == 0) return FileType::IT;
  else if (ext.compare("spc", Qt::CaseInsensitive) == 0) return FileType::SPC;
  else if (ext.compare("vgm", Qt::CaseInsensitive) == 0) return FileType::VGM;

  else return FileType::Unknown;

}

QString Song::ImageCacheDir(const Source source) {

  switch (source) {
    case Source::Collection:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/collectionalbumcovers";
    case Source::Subsonic:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/subsonicalbumcovers";
    case Source::Tidal:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tidalalbumcovers";
    case Source::Qobuz:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/qobuzalbumcovers";
    case Source::Device:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/devicealbumcovers";
    case Source::LocalFile:
    case Source::CDDA:
    case Source::Stream:
    case Source::SomaFM:
    case Source::RadioParadise:
    case Source::Unknown:
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

void Song::InitFromProtobuf(const spb::tagreader::SongMetadata &pb) {

  if (d->source_ == Source::Unknown) d->source_ = Source::LocalFile;

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
  set_length_nanosec(static_cast<qint64>(pb.length_nanosec()));
  d->bitrate_ = pb.bitrate();
  d->samplerate_ = pb.samplerate();
  d->bitdepth_ = pb.bitdepth();
  set_url(QUrl::fromEncoded(QByteArray(pb.url().data(), static_cast<qint64>(pb.url().size()))));
  d->basefilename_ = QStringFromStdString(pb.basefilename());
  d->filetype_ = static_cast<FileType>(pb.filetype());
  d->filesize_ = pb.filesize();
  d->mtime_ = pb.mtime();
  d->ctime_ = pb.ctime();
  d->skipcount_ = pb.skipcount();
  d->lastplayed_ = pb.lastplayed();
  d->lastseen_ = pb.lastseen();

  if (pb.has_playcount()) {
    d->playcount_ = pb.playcount();
  }
  if (pb.has_rating()) {
    d->rating_ = pb.rating();
  }

  if (pb.has_art_automatic()) {
    QByteArray art_automatic(pb.art_automatic().data(), static_cast<qint64>(pb.art_automatic().size()));
    if (!art_automatic.isEmpty()) set_art_automatic(QUrl::fromLocalFile(art_automatic));
  }

  d->suspicious_tags_ = pb.suspicious_tags();

  InitArtManual();

}

void Song::ToProtobuf(spb::tagreader::SongMetadata *pb) const {

  const QByteArray url(d->url_.toEncoded());
  const QByteArray art_automatic(d->art_automatic_.toEncoded());

  pb->set_valid(d->valid_);
  pb->set_title(DataCommaSizeFromQString(d->title_));
  pb->set_album(DataCommaSizeFromQString(d->album_));
  pb->set_artist(DataCommaSizeFromQString(d->artist_));
  pb->set_albumartist(DataCommaSizeFromQString(d->albumartist_));
  pb->set_track(d->track_);
  pb->set_disc(d->disc_);
  pb->set_year(d->year_);
  pb->set_originalyear(d->originalyear_);
  pb->set_genre(DataCommaSizeFromQString(d->genre_));
  pb->set_compilation(d->compilation_);
  pb->set_composer(DataCommaSizeFromQString(d->composer_));
  pb->set_performer(DataCommaSizeFromQString(d->performer_));
  pb->set_grouping(DataCommaSizeFromQString(d->grouping_));
  pb->set_comment(DataCommaSizeFromQString(d->comment_));
  pb->set_lyrics(DataCommaSizeFromQString(d->lyrics_));
  pb->set_length_nanosec(length_nanosec());
  pb->set_bitrate(d->bitrate_);
  pb->set_samplerate(d->samplerate_);
  pb->set_bitdepth(d->bitdepth_);
  pb->set_url(url.constData(), url.size());
  pb->set_basefilename(DataCommaSizeFromQString(d->basefilename_));
  pb->set_filetype(static_cast<spb::tagreader::SongMetadata_FileType>(d->filetype_));
  pb->set_filesize(d->filesize_);
  pb->set_mtime(d->mtime_);
  pb->set_ctime(d->ctime_);
  pb->set_playcount(d->playcount_);
  pb->set_skipcount(d->skipcount_);
  pb->set_lastplayed(d->lastplayed_);
  pb->set_lastseen(d->lastseen_);
  pb->set_art_automatic(art_automatic.constData(), art_automatic.size());
  pb->set_rating(d->rating_);
  pb->set_suspicious_tags(d->suspicious_tags_);

}

void Song::InitFromQuery(const SqlRow &q, const bool reliable_metadata) {

  d->id_ = q.value("rowid").isNull() ? -1 : q.value("rowid").toInt();

  set_title(q.ValueToString("title"));
  set_album(q.ValueToString("album"));
  set_artist(q.ValueToString("artist"));
  set_albumartist(q.ValueToString("albumartist"));
  d->track_ = q.ValueToInt("track");
  d->disc_ = q.ValueToInt("disc");
  d->year_ = q.ValueToInt("year");
  d->originalyear_ = q.ValueToInt("originalyear");
  d->genre_ = q.ValueToString("genre");
  d->compilation_ = q.value("compilation").toBool();
  d->composer_ = q.ValueToString("composer");
  d->performer_ = q.ValueToString("performer");
  d->grouping_ = q.ValueToString("grouping");
  d->comment_ = q.ValueToString("comment");
  d->lyrics_ = q.ValueToString("lyrics");
  d->artist_id_ = q.ValueToString("artist_id");
  d->album_id_ = q.ValueToString("album_id");
  d->song_id_ = q.ValueToString("song_id");
  d->beginning_ = q.value("beginning").isNull() ? 0 : q.value("beginning").toLongLong();
  set_length_nanosec(q.ValueToLongLong("length"));
  d->bitrate_ = q.ValueToInt("bitrate");
  d->samplerate_ = q.ValueToInt("samplerate");
  d->bitdepth_ = q.ValueToInt("bitdepth");
  d->source_ = Source(q.value("source").isNull() ? 0 : q.value("source").toInt());
  d->directory_id_ = q.ValueToInt("directory_id");
  set_url(QUrl::fromEncoded(q.ValueToString("url").toUtf8()));
  d->basefilename_ = QFileInfo(d->url_.toLocalFile()).fileName();
  d->filetype_ = FileType(q.value("filetype").isNull() ? 0 : q.value("filetype").toInt());
  d->filesize_ = q.ValueToLongLong("filesize");
  d->mtime_ = q.ValueToLongLong("mtime");
  d->ctime_ = q.ValueToLongLong("ctime");
  d->unavailable_ = q.value("unavailable").toBool();
  d->fingerprint_ = q.ValueToString("fingerprint");
  d->playcount_ = q.ValueToUInt("playcount");
  d->skipcount_ = q.ValueToUInt("skipcount");
  d->lastplayed_ = q.ValueToLongLong("lastplayed");
  d->lastseen_ = q.ValueToLongLong("lastseen");
  d->compilation_detected_ = q.ValueToBool("compilation_detected");
  d->compilation_on_ = q.ValueToBool("compilation_on");
  d->compilation_off_ = q.ValueToBool("compilation_off");
  QString art_automatic = q.ValueToString("art_automatic");
  if (!art_automatic.isEmpty()) {
    if (art_automatic.contains(QRegularExpression("..+:.*"))) {
      set_art_automatic(QUrl::fromEncoded(art_automatic.toUtf8()));
    }
    else {
      set_art_automatic(QUrl::fromLocalFile(art_automatic));
    }
  }
  QString art_manual = q.ValueToString("art_manual");
  if (!art_manual.isEmpty()) {
    if (art_manual.contains(QRegularExpression("..+:.*"))) {
      set_art_manual(QUrl::fromEncoded(art_manual.toUtf8()));
    }
    else {
      set_art_manual(QUrl::fromLocalFile(art_manual));
    }
  }

  d->cue_path_ = q.ValueToString("cue_path");
  d->rating_ = q.ValueToFloat("rating");

  d->valid_ = true;
  d->init_from_file_ = reliable_metadata;

  InitArtManual();

}

void Song::InitFromFilePartial(const QString &filename, const QFileInfo &fileinfo) {

  set_url(QUrl::fromLocalFile(filename));
  d->valid_ = true;
  d->source_ = Source::LocalFile;
  d->filetype_ = FiletypeByExtension(fileinfo.suffix());
  d->basefilename_ = fileinfo.fileName();
  d->title_ = fileinfo.fileName();
  if (d->art_manual_.isEmpty()) InitArtManual();

}

void Song::InitArtManual() {

  // If we don't have an art, check if we have one in the cache
  if (d->art_manual_.isEmpty() && d->art_automatic_.isEmpty() && !effective_albumartist().isEmpty() && !effective_album().isEmpty()) {
    QString filename(Utilities::Sha1CoverHash(effective_albumartist(), effective_album()).toHex() + ".jpg");
    QString path(ImageCacheDir(d->source_) + "/" + filename);
    if (QFile::exists(path)) {
      d->art_manual_ = QUrl::fromLocalFile(path);
    }
  }

}

void Song::InitArtAutomatic() {

  if (d->source_ == Source::LocalFile && d->url_.isLocalFile() && d->art_automatic_.isEmpty()) {
    // Pick the first image file in the album directory.
    QFileInfo file(d->url_.toLocalFile());
    QDir dir(file.path());
    QStringList files = dir.entryList(QStringList() << "*.jpg" << "*.png" << "*.gif" << "*.jpeg", QDir::Files|QDir::Readable, QDir::Name);
    if (files.count() > 0) {
      d->art_automatic_ = QUrl::fromLocalFile(file.path() + QDir::separator() + files.first());
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
  d->compilation_ = track->compilation == 1;
  d->composer_ = QString::fromUtf8(track->composer);
  d->grouping_ = QString::fromUtf8(track->grouping);
  d->comment_ = QString::fromUtf8(track->comment);

  set_length_nanosec(track->tracklen * kNsecPerMsec);

  d->bitrate_ = track->bitrate;
  d->samplerate_ = track->samplerate;
  d->bitdepth_ = -1; //track->bitdepth;

  d->source_ = Source::Device;
  QString filename = QString::fromLocal8Bit(track->ipod_path);
  filename.replace(':', '/');
  if (prefix.contains("://")) {
    set_url(QUrl(prefix + filename));
  }
  else {
    set_url(QUrl::fromLocalFile(prefix + filename));
  }
  d->basefilename_ = QFileInfo(filename).fileName();

  d->filetype_ = track->type2 ? FileType::MPEG : FileType::MP4;
  d->filesize_ = track->size;
  d->mtime_ = track->time_modified;
  d->ctime_ = track->time_added;

  d->playcount_ = static_cast<int>(track->playcount);
  d->skipcount_ = static_cast<int>(track->skipcount);
  d->lastplayed_ = track->time_played;

  if (itdb_track_has_thumbnails(track) && !d->artist_.isEmpty() && !d->title_.isEmpty()) {
    GdkPixbuf *pixbuf = static_cast<GdkPixbuf*>(itdb_track_get_thumbnail(track, -1, -1));
    if (pixbuf) {
      QString cover_path = ImageCacheDir(Source::Device);
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

  track->tracklen = static_cast<int>(length_nanosec() / kNsecPerMsec);

  track->bitrate = d->bitrate_;
  track->samplerate = d->samplerate_;

  track->type1 = (d->filetype_ == FileType::MPEG ? 1 : 0);
  track->type2 = (d->filetype_ == FileType::MPEG ? 1 : 0);
  track->mediatype = 1;  // Audio
  track->size = static_cast<uint>(d->filesize_);
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
  d->source_ = Source::Device;

  set_title(QString::fromUtf8(track->title));
  set_artist(QString::fromUtf8(track->artist));
  set_album(QString::fromUtf8(track->album));
  d->genre_ = QString::fromUtf8(track->genre);
  d->composer_ = QString::fromUtf8(track->composer);
  d->track_ = track->tracknumber;

  d->url_ = QUrl(QString("mtp://%1/%2").arg(host, QString::number(track->item_id)));
  d->basefilename_ = QString::number(track->item_id);
  d->filesize_ = static_cast<qint64>(track->filesize);
  d->mtime_ = track->modificationdate;
  d->ctime_ = track->modificationdate;

  set_length_nanosec(track->duration * kNsecPerMsec);

  d->samplerate_ = static_cast<int>(track->samplerate);
  d->bitdepth_ = 0;
  d->bitrate_ = static_cast<int>(track->bitrate);

  d->playcount_ = track->usecount;

  switch (track->filetype) {
    case LIBMTP_FILETYPE_WAV:  d->filetype_ = FileType::WAV;       break;
    case LIBMTP_FILETYPE_MP3:  d->filetype_ = FileType::MPEG;      break;
    case LIBMTP_FILETYPE_WMA:  d->filetype_ = FileType::ASF;       break;
    case LIBMTP_FILETYPE_OGG:  d->filetype_ = FileType::OggVorbis; break;
    case LIBMTP_FILETYPE_MP4:  d->filetype_ = FileType::MP4;       break;
    case LIBMTP_FILETYPE_AAC:  d->filetype_ = FileType::MP4;       break;
    case LIBMTP_FILETYPE_FLAC: d->filetype_ = FileType::OggFlac;   break;
    case LIBMTP_FILETYPE_MP2:  d->filetype_ = FileType::MPEG;      break;
    case LIBMTP_FILETYPE_M4A:  d->filetype_ = FileType::MP4;       break;
    default:
      d->filetype_ = FileType::Unknown;
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

  track->filesize = static_cast<quint64>(d->filesize_);
  track->modificationdate = d->mtime_;

  track->duration = length_nanosec() / kNsecPerMsec;

  track->bitrate = d->bitrate_;
  track->bitratetype = 0;
  track->samplerate = d->samplerate_;
  track->nochannels = 0;
  track->wavecodec = 0;

  track->usecount = d->playcount_;

  switch (d->filetype_) {
    case FileType::ASF:       track->filetype = LIBMTP_FILETYPE_ASF;         break;
    case FileType::MP4:       track->filetype = LIBMTP_FILETYPE_MP4;         break;
    case FileType::MPEG:      track->filetype = LIBMTP_FILETYPE_MP3;         break;
    case FileType::FLAC:
    case FileType::OggFlac:   track->filetype = LIBMTP_FILETYPE_FLAC;        break;
    case FileType::OggSpeex:
    case FileType::OggVorbis: track->filetype = LIBMTP_FILETYPE_OGG;         break;
    case FileType::WAV:       track->filetype = LIBMTP_FILETYPE_WAV;         break;
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
  if (bundle.filetype != FileType::Unknown) d->filetype_ = bundle.filetype;
  if (bundle.samplerate > 0) d->samplerate_ = bundle.samplerate;
  if (bundle.bitdepth > 0) d->bitdepth_ = bundle.bitdepth;
  if (bundle.bitrate > 0) d->bitrate_ = bundle.bitrate;

  return minor;

}

void Song::BindToQuery(SqlQuery *query) const {

  // Remember to bind these in the same order as kBindSpec

  query->BindStringValue(":title", d->title_);
  query->BindStringValue(":album", d->album_);
  query->BindStringValue(":artist", d->artist_);
  query->BindStringValue(":albumartist", d->albumartist_);
  query->BindIntValue(":track", d->track_);
  query->BindIntValue(":disc", d->disc_);
  query->BindIntValue(":year", d->year_);
  query->BindIntValue(":originalyear", d->originalyear_);
  query->BindStringValue(":genre", d->genre_);
  query->BindBoolValue(":compilation", d->compilation_);
  query->BindStringValue(":composer", d->composer_);
  query->BindStringValue(":performer", d->performer_);
  query->BindStringValue(":grouping", d->grouping_);
  query->BindStringValue(":comment", d->comment_);
  query->BindStringValue(":lyrics", d->lyrics_);

  query->BindStringValue(":artist_id", d->artist_id_);
  query->BindStringValue(":album_id", d->album_id_);
  query->BindStringValue(":song_id", d->song_id_);

  query->BindValue(":beginning", d->beginning_);
  query->BindLongLongValue(":length", length_nanosec());

  query->BindIntValue(":bitrate", d->bitrate_);
  query->BindIntValue(":samplerate", d->samplerate_);
  query->BindIntValue(":bitdepth", d->bitdepth_);

  query->BindValue(":source", static_cast<int>(d->source_));
  query->BindNotNullIntValue(":directory_id", d->directory_id_);
  query->BindUrlValue(":url", d->url_);
  query->BindValue(":filetype", static_cast<int>(d->filetype_));
  query->BindLongLongValueOrZero(":filesize", d->filesize_);
  query->BindLongLongValueOrZero(":mtime", d->mtime_);
  query->BindLongLongValueOrZero(":ctime", d->ctime_);
  query->BindBoolValue(":unavailable", d->unavailable_);

  query->BindStringValue(":fingerprint", d->fingerprint_);

  query->BindValue(":playcount", d->playcount_);
  query->BindValue(":skipcount", d->skipcount_);
  query->BindLongLongValue(":lastplayed", d->lastplayed_);
  query->BindLongLongValue(":lastseen", d->lastseen_);

  query->BindBoolValue(":compilation_detected", d->compilation_detected_);
  query->BindBoolValue(":compilation_on", d->compilation_on_);
  query->BindBoolValue(":compilation_off", d->compilation_off_);
  query->BindBoolValue(":compilation_effective", is_compilation());

  query->BindUrlValue(":art_automatic", d->art_automatic_);
  query->BindUrlValue(":art_manual", d->art_manual_);

  query->BindStringValue(":effective_albumartist", effective_albumartist());
  query->BindIntValue(":effective_originalyear", effective_originalyear());

  query->BindValue(":cue_path", d->cue_path_);

  query->BindFloatValue(":rating", d->rating_);

}

void Song::BindToFtsQuery(SqlQuery *query) const {

  query->BindValue(":ftstitle", d->title_);
  query->BindValue(":ftsalbum", d->album_);
  query->BindValue(":ftsartist", d->artist_);
  query->BindValue(":ftsalbumartist", d->albumartist_);
  query->BindValue(":ftscomposer", d->composer_);
  query->BindValue(":ftsperformer", d->performer_);
  query->BindValue(":ftsgrouping", d->grouping_);
  query->BindValue(":ftsgenre", d->genre_);
  query->BindValue(":ftscomment", d->comment_);

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

QString Song::PrettyOriginalYear() const {

  if (effective_originalyear() == -1) return QString();

  return QString::number(effective_originalyear());

}

QString Song::TitleWithCompilationArtist() const {

  QString title(d->title_);

  if (title.isEmpty()) title = d->basefilename_;

  if (is_compilation() && !d->artist_.isEmpty() && !d->artist_.contains("various", Qt::CaseInsensitive)) title = d->artist_ + " - " + title;

  return title;

}

QString Song::SampleRateBitDepthToText() const {

  if (d->samplerate_ == -1) return QString("");
  if (d->bitdepth_ == -1) return QString("%1 hz").arg(d->samplerate_);

  return QString("%1 hz / %2 bit").arg(d->samplerate_).arg(d->bitdepth_);

}

QString Song::PrettyRating() const {

  float rating = d->rating_;

  if (rating == -1.0F) return "0";

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
         d->cue_path_ == other.d->cue_path_;
}

bool Song::IsStatisticsEqual(const Song &other) const {

  return d->playcount_ == other.d->playcount_ &&
         d->skipcount_ == other.d->skipcount_ &&
         d->lastplayed_ == other.d->lastplayed_;

}

bool Song::IsRatingEqual(const Song &other) const {

  return d->rating_ == other.d->rating_;

}

bool Song::IsFingerprintEqual(const Song &other) const {

  return d->fingerprint_ == other.d->fingerprint_;

}

bool Song::IsArtEqual(const Song &other) const {

  return d->art_automatic_ == other.d->art_automatic_ &&
         d->art_manual_ == other.d->art_manual_;

}

bool Song::IsAllMetadataEqual(const Song &other) const {

  return IsMetadataEqual(other) &&
         IsStatisticsEqual(other) &&
         IsRatingEqual(other) &&
         IsFingerprintEqual(other) &&
         IsArtEqual(other);

}

bool Song::IsEditable() const {
  return d->valid_ && d->url_.isValid() && (d->url_.isLocalFile() || d->source_ == Source::Stream) && !has_cue();
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

size_t HashSimilar(const Song &song) {
  // Should compare the same fields as function IsSimilar
  return qHash(song.title().toLower()) ^ qHash(song.artist().toLower()) ^ qHash(song.album().toLower());
}

bool Song::IsOnSameAlbum(const Song &other) const {

  if (is_compilation() != other.is_compilation()) return false;

  if (has_cue() && other.has_cue() && cue_path() == other.cue_path()) {
    return true;
  }

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
  AddMetadata("mpris:length", (length_nanosec() / kNsecPerUsec), map);
  AddMetadata("xesam:trackNumber", track(), map);
  AddMetadataAsList("xesam:genre", genre(), map);
  AddMetadata("xesam:discNumber", disc(), map);
  AddMetadataAsList("xesam:comment", comment(), map);
  AddMetadata("xesam:contentCreated", AsMPRISDateTimeType(ctime()), map);
  AddMetadata("xesam:lastUsed", AsMPRISDateTimeType(lastplayed()), map);
  AddMetadataAsList("xesam:composer", composer(), map);
  AddMetadata("xesam:useCount", static_cast<int>(playcount()), map);

  if (rating() != -1.0) {
    AddMetadata("xesam:userRating", rating(), map);
  }

}

void Song::MergeUserSetData(const Song &other, const bool merge_playcount, const bool merge_rating) {

  if (merge_playcount && other.playcount() > 0) {
    set_playcount(other.playcount());
  }

  if (merge_rating && other.rating() > 0.0F) {
    set_rating(other.rating());
  }

  set_skipcount(other.skipcount());
  set_lastplayed(other.lastplayed());
  set_art_manual(other.art_manual());
  set_compilation_on(other.compilation_on());
  set_compilation_off(other.compilation_off());

}
