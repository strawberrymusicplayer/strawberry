/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QSqlRecord>

#include "core/iconloader.h"
#include "engine/enginemetadata.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/coverutils.h"
#include "utilities/timeconstants.h"
#include "utilities/sqlhelper.h"
#include "song.h"
#include "sqlquery.h"
#include "sqlrow.h"
#ifdef HAVE_DBUS
#  include "mpris_common.h"
#endif
#include "tagreadermessages.pb.h"

const QStringList Song::kColumns = QStringList() << QStringLiteral("title")
                                                 << QStringLiteral("album")
                                                 << QStringLiteral("artist")
                                                 << QStringLiteral("albumartist")
                                                 << QStringLiteral("track")
                                                 << QStringLiteral("disc")
                                                 << QStringLiteral("year")
                                                 << QStringLiteral("originalyear")
                                                 << QStringLiteral("genre")
                                                 << QStringLiteral("compilation")
                                                 << QStringLiteral("composer")
                                                 << QStringLiteral("performer")
                                                 << QStringLiteral("grouping")
                                                 << QStringLiteral("comment")
                                                 << QStringLiteral("lyrics")

                                                 << QStringLiteral("artist_id")
                                                 << QStringLiteral("album_id")
                                                 << QStringLiteral("song_id")

                                                 << QStringLiteral("beginning")
                                                 << QStringLiteral("length")

                                                 << QStringLiteral("bitrate")
                                                 << QStringLiteral("samplerate")
                                                 << QStringLiteral("bitdepth")

                                                 << QStringLiteral("source")
                                                 << QStringLiteral("directory_id")
                                                 << QStringLiteral("url")
                                                 << QStringLiteral("filetype")
                                                 << QStringLiteral("filesize")
                                                 << QStringLiteral("mtime")
                                                 << QStringLiteral("ctime")
                                                 << QStringLiteral("unavailable")

                                                 << QStringLiteral("fingerprint")

                                                 << QStringLiteral("playcount")
                                                 << QStringLiteral("skipcount")
                                                 << QStringLiteral("lastplayed")
                                                 << QStringLiteral("lastseen")

                                                 << QStringLiteral("compilation_detected")
                                                 << QStringLiteral("compilation_on")
                                                 << QStringLiteral("compilation_off")
                                                 << QStringLiteral("compilation_effective")

                                                 << QStringLiteral("art_embedded")
                                                 << QStringLiteral("art_automatic")
                                                 << QStringLiteral("art_manual")
                                                 << QStringLiteral("art_unset")

                                                 << QStringLiteral("effective_albumartist")
                                                 << QStringLiteral("effective_originalyear")

                                                 << QStringLiteral("cue_path")

                                                 << QStringLiteral("rating")

                                                 << QStringLiteral("acoustid_id")
                                                 << QStringLiteral("acoustid_fingerprint")

                                                 << QStringLiteral("musicbrainz_album_artist_id")
                                                 << QStringLiteral("musicbrainz_artist_id")
                                                 << QStringLiteral("musicbrainz_original_artist_id")
                                                 << QStringLiteral("musicbrainz_album_id")
                                                 << QStringLiteral("musicbrainz_original_album_id")
                                                 << QStringLiteral("musicbrainz_recording_id")
                                                 << QStringLiteral("musicbrainz_track_id")
                                                 << QStringLiteral("musicbrainz_disc_id")
                                                 << QStringLiteral("musicbrainz_release_group_id")
                                                 << QStringLiteral("musicbrainz_work_id")

                                                 << QStringLiteral("ebur128_integrated_loudness_lufs")
                                                 << QStringLiteral("ebur128_loudness_range_lu")

                                                 ;

const QStringList Song::kRowIdColumns = QStringList() << QStringLiteral("ROWID") << kColumns;

const QString Song::kColumnSpec = kColumns.join(QLatin1String(", "));
const QString Song::kRowIdColumnSpec = kRowIdColumns.join(QLatin1String(", "));
const QString Song::kBindSpec = Utilities::Prepend(QStringLiteral(":"), kColumns).join(QLatin1String(", "));
const QString Song::kUpdateSpec = Utilities::Updateify(kColumns).join(QLatin1String(", "));

const QStringList Song::kTextSearchColumns = QStringList()      << QStringLiteral("title")
                                                                << QStringLiteral("album")
                                                                << QStringLiteral("artist")
                                                                << QStringLiteral("albumartist")
                                                                << QStringLiteral("composer")
                                                                << QStringLiteral("performer")
                                                                << QStringLiteral("grouping")
                                                                << QStringLiteral("genre")
                                                                << QStringLiteral("comment");

const QStringList Song::kIntSearchColumns = QStringList()       << QStringLiteral("track")
                                                                << QStringLiteral("year")
                                                                << QStringLiteral("samplerate")
                                                                << QStringLiteral("bitdepth")
                                                                << QStringLiteral("bitrate");

const QStringList Song::kUIntSearchColumns = QStringList()      << QStringLiteral("playcount")
                                                                << QStringLiteral("skipcount");

const QStringList Song::kInt64SearchColumns = QStringList()     << QStringLiteral("length");

const QStringList Song::kFloatSearchColumns = QStringList()     << QStringLiteral("rating");

const QStringList Song::kNumericalSearchColumns = QStringList() << kIntSearchColumns
                                                                << kUIntSearchColumns
                                                                << kInt64SearchColumns
                                                                << kFloatSearchColumns;

const QStringList Song::kSearchColumns = QStringList() << kTextSearchColumns
                                                       << kNumericalSearchColumns;

const Song::RegularExpressionList Song::kAlbumDisc = Song::RegularExpressionList()
    << QRegularExpression(QStringLiteral("\\s+-*\\s*(Disc|CD)\\s*([0-9]{1,2})$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\(\\s*(Disc|CD)\\s*([0-9]{1,2})\\)$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\[\\s*(Disc|CD)\\s*([0-9]{1,2})\\]$"), QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kRemastered = Song::RegularExpressionList()
    << QRegularExpression(QStringLiteral("\\s+-*\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\(\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*\\)\\s*$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\[\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*\\]\\s*$"), QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kExplicit = Song::RegularExpressionList()
    << QRegularExpression(QStringLiteral("\\s+-*\\s*Explicit\\s*$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\(\\s*Explicit\\s*\\)\\s*$"), QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(QStringLiteral("\\s+-*\\s*\\[\\s*Explicit\\s*\\]\\s*$"), QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kAlbumMisc = Song::RegularExpressionList()
    << kRemastered
    << kExplicit;

const Song::RegularExpressionList Song::kTitleMisc = Song::RegularExpressionList()
    << kRemastered
    << kExplicit;

const QStringList Song::kArticles = QStringList() << QStringLiteral("the ") << QStringLiteral("a ") << QStringLiteral("an ");

const QStringList Song::kAcceptedExtensions = QStringList() << QStringLiteral("wav")
                                                            << QStringLiteral("flac")
                                                            << QStringLiteral("wv")
                                                            << QStringLiteral("ogg")
                                                            << QStringLiteral("oga")
                                                            << QStringLiteral("opus")
                                                            << QStringLiteral("spx")
                                                            << QStringLiteral("ape")
                                                            << QStringLiteral("mpc")
                                                            << QStringLiteral("mp2")
                                                            << QStringLiteral("mp3")
                                                            << QStringLiteral("m4a")
                                                            << QStringLiteral("mp4")
                                                            << QStringLiteral("aac")
                                                            << QStringLiteral("asf")
                                                            << QStringLiteral("asx")
                                                            << QStringLiteral("wma")
                                                            << QStringLiteral("aif")
                                                            << QStringLiteral("aiff")
                                                            << QStringLiteral("mka")
                                                            << QStringLiteral("tta")
                                                            << QStringLiteral("dsf")
                                                            << QStringLiteral("dsd")
                                                            << QStringLiteral("ac3")
                                                            << QStringLiteral("dts")
                                                            << QStringLiteral("spc")
                                                            << QStringLiteral("vgm");

struct Song::Private : public QSharedData {

  explicit Private(Source source = Source::Unknown);

  int id_;

  bool valid_;

  QString title_;
  QString album_;
  QString artist_;
  QString albumartist_;
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

  bool art_embedded_;           // if the song has embedded album cover art.
  QUrl art_automatic_;          // Guessed by CollectionWatcher.
  QUrl art_manual_;             // Set by the user - should take priority.
  bool art_unset_;              // If the art was unset by the user.

  QString cue_path_;            // If the song has a CUE, this contains it's path.

  float rating_;                // Database rating, initial rating read from tag.

  QString acoustid_id_;
  QString acoustid_fingerprint_;

  QString musicbrainz_album_artist_id_;
  QString musicbrainz_artist_id_;
  QString musicbrainz_original_artist_id_;
  QString musicbrainz_album_id_;
  QString musicbrainz_original_album_id_;
  QString musicbrainz_recording_id_;
  QString musicbrainz_track_id_;
  QString musicbrainz_disc_id_;
  QString musicbrainz_release_group_id_;
  QString musicbrainz_work_id_;

  std::optional<double> ebur128_integrated_loudness_lufs_;
  std::optional<double> ebur128_loudness_range_lu_;

  bool init_from_file_;         // Whether this song was loaded from a file using taglib.
  bool suspicious_tags_;        // Whether our encoding guesser thinks these tags might be incorrectly encoded.

  QString title_sortable_;
  QString album_sortable_;
  QString artist_sortable_;
  QString albumartist_sortable_;

  QUrl stream_url_;             // Temporary stream url set by the URL handler.

};

Song::Private::Private(const Source source)
    : id_(-1),
      valid_(false),

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

      art_embedded_(false),
      art_unset_(false),

      rating_(-1),

      init_from_file_(false),
      suspicious_tags_(false)

      {}

Song::Song(const Source source) : d(new Private(source)) {}
Song::Song(const Song &other) = default;
Song::~Song() = default;

bool Song::operator==(const Song &other) const {
  return source() == other.source() && url() == other.url() && beginning_nanosec() == other.beginning_nanosec();
}

bool Song::operator!=(const Song &other) const {
  return source() != other.source() || url() != other.url() || beginning_nanosec() != other.beginning_nanosec();
}

Song &Song::operator=(const Song &other) {
  d = other.d;
  return *this;
}

int Song::id() const { return d->id_; }
bool Song::is_valid() const { return d->valid_; }

const QString &Song::title() const { return d->title_; }
const QString &Song::album() const { return d->album_; }
const QString &Song::artist() const { return d->artist_; }
const QString &Song::albumartist() const { return d->albumartist_; }
int Song::track() const { return d->track_; }
int Song::disc() const { return d->disc_; }
int Song::year() const { return d->year_; }
int Song::originalyear() const { return d->originalyear_; }
const QString &Song::genre() const { return d->genre_; }
bool Song::compilation() const { return d->compilation_; }
const QString &Song::composer() const { return d->composer_; }
const QString &Song::performer() const { return d->performer_; }
const QString &Song::grouping() const { return d->grouping_; }
const QString &Song::comment() const { return d->comment_; }
const QString &Song::lyrics() const { return d->lyrics_; }

QString Song::artist_id() const { return d->artist_id_.isNull() ? QLatin1String("") : d->artist_id_; }
QString Song::album_id() const { return d->album_id_.isNull() ? QLatin1String("") : d->album_id_; }
QString Song::song_id() const { return d->song_id_.isNull() ? QLatin1String("") : d->song_id_; }

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
bool Song::unavailable() const { return d->unavailable_; }

QString Song::fingerprint() const { return d->fingerprint_; }

uint Song::playcount() const { return d->playcount_; }
uint Song::skipcount() const { return d->skipcount_; }
qint64 Song::lastplayed() const { return d->lastplayed_; }
qint64 Song::lastseen() const { return d->lastseen_; }

bool Song::compilation_detected() const { return d->compilation_detected_; }
bool Song::compilation_off() const { return d->compilation_off_; }
bool Song::compilation_on() const { return d->compilation_on_; }

bool Song::art_embedded() const { return d->art_embedded_; }
const QUrl &Song::art_automatic() const { return d->art_automatic_; }
const QUrl &Song::art_manual() const { return d->art_manual_; }
bool Song::art_unset() const { return d->art_unset_; }

const QString &Song::cue_path() const { return d->cue_path_; }

float Song::rating() const { return d->rating_; }

const QString &Song::acoustid_id() const { return d->acoustid_id_; }
const QString &Song::acoustid_fingerprint() const { return d->acoustid_fingerprint_; }

const QString &Song::musicbrainz_album_artist_id() const { return d->musicbrainz_album_artist_id_; }
const QString &Song::musicbrainz_artist_id() const { return d->musicbrainz_artist_id_; }
const QString &Song::musicbrainz_original_artist_id() const { return d->musicbrainz_original_artist_id_; }
const QString &Song::musicbrainz_album_id() const { return d->musicbrainz_album_id_; }
const QString &Song::musicbrainz_original_album_id() const { return d->musicbrainz_original_album_id_; }
const QString &Song::musicbrainz_recording_id() const { return d->musicbrainz_recording_id_; }
const QString &Song::musicbrainz_track_id() const { return d->musicbrainz_track_id_; }
const QString &Song::musicbrainz_disc_id() const { return d->musicbrainz_disc_id_; }
const QString &Song::musicbrainz_release_group_id() const { return d->musicbrainz_release_group_id_; }
const QString &Song::musicbrainz_work_id() const { return d->musicbrainz_work_id_; }

std::optional<double> Song::ebur128_integrated_loudness_lufs() const { return d->ebur128_integrated_loudness_lufs_; }
std::optional<double> Song::ebur128_loudness_range_lu() const { return d->ebur128_loudness_range_lu_; }

bool Song::init_from_file() const { return d->init_from_file_; }

const QString &Song::title_sortable() const { return d->title_sortable_; }
const QString &Song::album_sortable() const { return d->album_sortable_; }
const QString &Song::artist_sortable() const { return d->artist_sortable_; }
const QString &Song::albumartist_sortable() const { return d->albumartist_sortable_; }

const QUrl &Song::stream_url() const { return d->stream_url_; }

void Song::set_id(const int id) { d->id_ = id; }
void Song::set_valid(const bool v) { d->valid_ = v; }

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

void Song::set_artist_id(const QString &v) { d->artist_id_ = v; }
void Song::set_album_id(const QString &v) { d->album_id_ = v; }
void Song::set_song_id(const QString &v) { d->song_id_ = v; }

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

void Song::set_art_embedded(const bool v) { d->art_embedded_ = v; }
void Song::set_art_automatic(const QUrl &v) { d->art_automatic_ = v; }
void Song::set_art_manual(const QUrl &v) { d->art_manual_ = v; }
void Song::set_art_unset(const bool v) { d->art_unset_ = v; }

void Song::set_cue_path(const QString &v) { d->cue_path_ = v; }

void Song::set_rating(const float v) { d->rating_ = v; }

void Song::set_acoustid_id(const QString &v) { d->acoustid_id_ = v; }
void Song::set_acoustid_fingerprint(const QString &v) { d->acoustid_fingerprint_ = v; }

void Song::set_musicbrainz_album_artist_id(const QString &v) { d->musicbrainz_album_artist_id_ = v; }
void Song::set_musicbrainz_artist_id(const QString &v) { d->musicbrainz_artist_id_ = v; }
void Song::set_musicbrainz_original_artist_id(const QString &v) { d->musicbrainz_original_artist_id_ = v; }
void Song::set_musicbrainz_album_id(const QString &v) { d->musicbrainz_album_id_ = v; }
void Song::set_musicbrainz_original_album_id(const QString &v) { d->musicbrainz_original_album_id_ = v; }
void Song::set_musicbrainz_recording_id(const QString &v) { d->musicbrainz_recording_id_ = v; }
void Song::set_musicbrainz_track_id(const QString &v) { d->musicbrainz_track_id_ = v; }
void Song::set_musicbrainz_disc_id(const QString &v) { d->musicbrainz_disc_id_ = v; }
void Song::set_musicbrainz_release_group_id(const QString &v) { d->musicbrainz_release_group_id_ = v; }
void Song::set_musicbrainz_work_id(const QString &v) { d->musicbrainz_work_id_ = v; }

void Song::set_ebur128_integrated_loudness_lufs(const std::optional<double> v) { d->ebur128_integrated_loudness_lufs_ = v; }
void Song::set_ebur128_loudness_range_lu(const std::optional<double> v) { d->ebur128_loudness_range_lu_ = v; }

void Song::set_stream_url(const QUrl &v) { d->stream_url_ = v; }

const QUrl &Song::effective_stream_url() const { return !d->stream_url_.isEmpty() && d->stream_url_.isValid() ? d->stream_url_ : d->url_; }
const QString &Song::effective_albumartist() const { return d->albumartist_.isEmpty() ? d->artist_ : d->albumartist_; }
const QString &Song::effective_albumartist_sortable() const { return d->albumartist_.isEmpty() ? d->artist_sortable_ : d->albumartist_sortable_; }
const QString &Song::effective_album() const { return d->album_.isEmpty() ? d->title_ : d->album_; }
int Song::effective_originalyear() const { return d->originalyear_ < 0 ? d->year_ : d->originalyear_; }
const QString &Song::playlist_albumartist() const { return is_compilation() ? d->albumartist_ : effective_albumartist(); }
const QString &Song::playlist_albumartist_sortable() const { return is_compilation() ? d->albumartist_sortable_ : effective_albumartist_sortable(); }

bool Song::is_metadata_good() const { return !d->url_.isEmpty() && !d->artist_.isEmpty() && !d->title_.isEmpty(); }
bool Song::is_collection_song() const { return d->source_ == Source::Collection; }
bool Song::is_stream() const { return is_radio() || d->source_ == Source::Tidal || d->source_ == Source::Subsonic || d->source_ == Source::Qobuz || d->source_ == Source::Spotify; }
bool Song::is_radio() const { return d->source_ == Source::Stream || d->source_ == Source::SomaFM || d->source_ == Source::RadioParadise; }
bool Song::is_cdda() const { return d->source_ == Source::CDDA; }
bool Song::is_compilation() const { return (d->compilation_ || d->compilation_detected_ || d->compilation_on_) && !d->compilation_off_; }
bool Song::stream_url_can_expire() const { return d->source_ == Source::Tidal || d->source_ == Source::Qobuz || d->source_ == Source::Spotify; }
bool Song::is_module_music() const { return d->filetype_ == FileType::MOD || d->filetype_ == FileType::S3M || d->filetype_ == FileType::XM || d->filetype_ == FileType::IT; }
bool Song::has_cue() const { return !d->cue_path_.isEmpty(); }

bool Song::art_automatic_is_valid() const {
  return !d->art_automatic_.isEmpty() && d->art_automatic_.isValid() && (!d->art_automatic_.isLocalFile() || (d->art_automatic_.isLocalFile() && QFile::exists(d->art_automatic_.toLocalFile())));
}
bool Song::art_manual_is_valid() const {
  return !d->art_manual_.isEmpty() && d->art_manual_.isValid() && (!d->art_manual_.isLocalFile() || (d->art_manual_.isLocalFile() && QFile::exists(d->art_manual_.toLocalFile())));
}
bool Song::has_valid_art() const { return art_embedded() || art_automatic_is_valid() || art_manual_is_valid(); }
void Song::clear_art_automatic() { d->art_automatic_.clear(); }
void Song::clear_art_manual() { d->art_manual_.clear(); }

bool Song::write_tags_supported() const {

  return d->filetype_ == FileType::FLAC ||
         d->filetype_ == FileType::WavPack ||
         d->filetype_ == FileType::OggFlac ||
         d->filetype_ == FileType::OggVorbis ||
         d->filetype_ == FileType::OggOpus ||
         d->filetype_ == FileType::OggSpeex ||
         d->filetype_ == FileType::MPEG ||
         d->filetype_ == FileType::MP4 ||
         d->filetype_ == FileType::ASF ||
         d->filetype_ == FileType::AIFF ||
         d->filetype_ == FileType::MPC ||
         d->filetype_ == FileType::TrueAudio ||
         d->filetype_ == FileType::APE ||
         d->filetype_ == FileType::DSF ||
         d->filetype_ == FileType::DSDIFF ||
         d->filetype_ == FileType::WAV;

}

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
    d->filetype_ == FileType::APE ||
    d->filetype_ == FileType::WAV;

}

bool Song::albumartist_supported() const {
  return additional_tags_supported() || d->filetype_ == FileType::ASF;
}

bool Song::composer_supported() const {
  return additional_tags_supported() || d->filetype_ == FileType::ASF;
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
    d->filetype_ == FileType::APE ||
    d->filetype_ == FileType::WAV;

}

bool Song::grouping_supported() const {
  return additional_tags_supported();
}

bool Song::genre_supported() const {
  return additional_tags_supported() || d->filetype_ == FileType::ASF;
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
  return additional_tags_supported() || d->filetype_ == FileType::ASF;
}

bool Song::save_embedded_cover_supported(const FileType filetype) {

  return filetype == FileType::FLAC ||
    filetype == FileType::OggVorbis ||
    filetype == FileType::OggOpus ||
    filetype == FileType::MPEG ||
    filetype == FileType::MP4;

}

QString Song::sortable(const QString &v) {

  QString copy = v.toLower();

  for (const auto &i : kArticles) {
    if (copy.startsWith(i)) {
      qint64 ilen = i.length();
      return copy.right(copy.length() - ilen) + QStringLiteral(", ") + copy.left(ilen - 1);
    }
  }

  return copy;

}

int Song::ColumnIndex(const QString &field) {

  return static_cast<int>(kRowIdColumns.indexOf(field));

}

QString Song::JoinSpec(const QString &table) {
  return Utilities::Prepend(table + QLatin1Char('.'), kRowIdColumns).join(QLatin1String(", "));
}

QString Song::PrettyTitle() const {

  QString title(d->title_);

  if (title.isEmpty()) title = d->basefilename_;
  if (title.isEmpty()) title = d->url_.toString();

  return title;

}

QString Song::PrettyTitleWithArtist() const {

  QString title(PrettyTitle());

  if (!d->artist_.isEmpty()) title = d->artist_ + QStringLiteral(" - ") + title;

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

  if (is_compilation() && !d->artist_.isEmpty() && !d->artist_.contains(QLatin1String("various"), Qt::CaseInsensitive)) title = d->artist_ + QStringLiteral(" - ") + title;

  return title;

}

QString Song::SampleRateBitDepthToText() const {

  if (d->samplerate_ == -1) return QLatin1String("");
  if (d->bitdepth_ == -1) return QStringLiteral("%1 hz").arg(d->samplerate_);

  return QStringLiteral("%1 hz / %2 bit").arg(d->samplerate_).arg(d->bitdepth_);

}

QString Song::Ebur128LoudnessLUFSToText(const std::optional<double> v) {

  if (!v) return QObject::tr("Unknown");

  return QString::asprintf("%+.2f ", *v) + QObject::tr("LUFS");

}

QString Song::Ebur128LoudnessLUFSToText() const {
  return Ebur128LoudnessLUFSToText(d->ebur128_integrated_loudness_lufs_);
}

QString Song::Ebur128LoudnessRangeLUToText(const std::optional<double> v) {

  if (!v) return QObject::tr("Unknown");

  return QString::asprintf("%.2f ", *v) + QObject::tr("LU");

}

QString Song::Ebur128LoudnessRangeLUToText() const {
  return Ebur128LoudnessRangeLUToText(d->ebur128_loudness_range_lu_);
}

QString Song::PrettyRating() const {

  float rating = d->rating_;

  if (rating == -1.0F) return QStringLiteral("0");

  return QString::number(static_cast<int>(rating * 100));

}

bool Song::IsEditable() const {
  return d->valid_ && d->url_.isValid() && ((d->url_.isLocalFile() && write_tags_supported() && !has_cue()) || d->source_ == Source::Stream);
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

bool Song::IsPlayStatisticsEqual(const Song &other) const {

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

bool Song::IsAcoustIdEqual(const Song &other) const {

  return d->acoustid_id_ == other.d->acoustid_id_ && d->acoustid_fingerprint_ == other.d->acoustid_fingerprint_;

}

bool Song::IsMusicBrainzEqual(const Song &other) const {

  return d->musicbrainz_album_artist_id_ == other.d->musicbrainz_album_artist_id_ &&
    d->musicbrainz_artist_id_ == other.d->musicbrainz_artist_id_ &&
    d->musicbrainz_original_artist_id_ == other.d->musicbrainz_original_artist_id_ &&
    d->musicbrainz_album_id_ == other.d->musicbrainz_album_id_ &&
    d->musicbrainz_original_album_id_ == other.d->musicbrainz_original_album_id_ &&
    d->musicbrainz_recording_id_ == other.d->musicbrainz_recording_id_ &&
    d->musicbrainz_track_id_ == other.d->musicbrainz_track_id_ &&
    d->musicbrainz_disc_id_ == other.d->musicbrainz_disc_id_ &&
    d->musicbrainz_release_group_id_ == other.d->musicbrainz_release_group_id_ &&
    d->musicbrainz_work_id_ == other.d->musicbrainz_work_id_;

}

bool Song::IsEBUR128Equal(const Song &other) const {

  return d->ebur128_integrated_loudness_lufs_ == other.d->ebur128_integrated_loudness_lufs_ &&
    d->ebur128_loudness_range_lu_ == other.d->ebur128_loudness_range_lu_;

}

bool Song::IsArtEqual(const Song &other) const {

  return d->art_embedded_ == other.d->art_embedded_ &&
    d->art_automatic_ == other.d->art_automatic_ &&
    d->art_manual_ == other.d->art_manual_ &&
    d->art_unset_ == other.d->art_unset_;

}

bool Song::IsAllMetadataEqual(const Song &other) const {

  return IsMetadataEqual(other) &&
    IsPlayStatisticsEqual(other) &&
    IsRatingEqual(other) &&
    IsAcoustIdEqual(other) &&
    IsMusicBrainzEqual(other) &&
    IsArtEqual(other);

}

bool Song::IsOnSameAlbum(const Song &other) const {

  if (is_compilation() != other.is_compilation()) return false;

  if (has_cue() && other.has_cue() && cue_path() == other.cue_path()) {
    return true;
  }

  if (is_compilation() && album() == other.album()) return true;

  return effective_album() == other.effective_album() && effective_albumartist() == other.effective_albumartist();

}

bool Song::IsSimilar(const Song &other) const {
  return title().compare(other.title(), Qt::CaseInsensitive) == 0 &&
    artist().compare(other.artist(), Qt::CaseInsensitive) == 0 &&
    album().compare(other.album(), Qt::CaseInsensitive) == 0;
}

Song::Source Song::SourceFromURL(const QUrl &url) {

  if (url.isLocalFile()) return Source::LocalFile;
  if (url.scheme() == QStringLiteral("cdda")) return Source::CDDA;
  if (url.scheme() == QStringLiteral("subsonic")) return Source::Subsonic;
  if (url.scheme() == QStringLiteral("tidal")) return Source::Tidal;
  if (url.scheme() == QStringLiteral("spotify")) return Source::Spotify;
  if (url.scheme() == QStringLiteral("qobuz")) return Source::Qobuz;
  if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("rtsp")) {
    if (url.host().endsWith(QLatin1String("tidal.com"), Qt::CaseInsensitive)) { return Source::Tidal; }
    if (url.host().endsWith(QLatin1String("spotify.com"), Qt::CaseInsensitive)) { return Source::Spotify; }
    if (url.host().endsWith(QLatin1String("qobuz.com"), Qt::CaseInsensitive)) { return Source::Qobuz; }
    if (url.host().endsWith(QLatin1String("somafm.com"), Qt::CaseInsensitive)) { return Source::SomaFM; }
    if (url.host().endsWith(QLatin1String("radioparadise.com"), Qt::CaseInsensitive)) { return Source::RadioParadise; }
    return Source::Stream;
  }
  else return Source::Unknown;

}

QString Song::TextForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return QStringLiteral("file");
    case Source::Collection:    return QStringLiteral("collection");
    case Source::CDDA:          return QStringLiteral("cd");
    case Source::Device:        return QStringLiteral("device");
    case Source::Stream:        return QStringLiteral("stream");
    case Source::Subsonic:      return QStringLiteral("subsonic");
    case Source::Tidal:         return QStringLiteral("tidal");
    case Source::Spotify:       return QStringLiteral("spotify");
    case Source::Qobuz:         return QStringLiteral("qobuz");
    case Source::SomaFM:        return QStringLiteral("somafm");
    case Source::RadioParadise: return QStringLiteral("radioparadise");
    case Source::Unknown:       return QStringLiteral("unknown");
  }
  return QStringLiteral("unknown");

}

QString Song::DescriptionForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return QStringLiteral("File");
    case Source::Collection:    return QStringLiteral("Collection");
    case Source::CDDA:          return QStringLiteral("CD");
    case Source::Device:        return QStringLiteral("Device");
    case Source::Stream:        return QStringLiteral("Stream");
    case Source::Subsonic:      return QStringLiteral("Subsonic");
    case Source::Tidal:         return QStringLiteral("Tidal");
    case Source::Spotify:       return QStringLiteral("Spotify");
    case Source::Qobuz:         return QStringLiteral("Qobuz");
    case Source::SomaFM:        return QStringLiteral("SomaFM");
    case Source::RadioParadise: return QStringLiteral("Radio Paradise");
    case Source::Unknown:       return QStringLiteral("Unknown");
  }
  return QStringLiteral("unknown");

}

Song::Source Song::SourceFromText(const QString &source) {

  if (source.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0) return Source::LocalFile;
  if (source.compare(QLatin1String("collection"), Qt::CaseInsensitive) == 0) return Source::Collection;
  if (source.compare(QLatin1String("cd"), Qt::CaseInsensitive) == 0) return Source::CDDA;
  if (source.compare(QLatin1String("device"), Qt::CaseInsensitive) == 0) return Source::Device;
  if (source.compare(QLatin1String("stream"), Qt::CaseInsensitive) == 0) return Source::Stream;
  if (source.compare(QLatin1String("subsonic"), Qt::CaseInsensitive) == 0) return Source::Subsonic;
  if (source.compare(QLatin1String("tidal"), Qt::CaseInsensitive) == 0) return Source::Tidal;
  if (source.compare(QLatin1String("spotify"), Qt::CaseInsensitive) == 0) return Source::Spotify;
  if (source.compare(QLatin1String("qobuz"), Qt::CaseInsensitive) == 0) return Source::Qobuz;
  if (source.compare(QLatin1String("somafm"), Qt::CaseInsensitive) == 0) return Source::SomaFM;
  if (source.compare(QLatin1String("radioparadise"), Qt::CaseInsensitive) == 0) return Source::RadioParadise;

  return Source::Unknown;

}

QIcon Song::IconForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return IconLoader::Load(QStringLiteral("folder-sound"));
    case Source::Collection:    return IconLoader::Load(QStringLiteral("library-music"));
    case Source::CDDA:          return IconLoader::Load(QStringLiteral("media-optical"));
    case Source::Device:        return IconLoader::Load(QStringLiteral("device"));
    case Source::Stream:        return IconLoader::Load(QStringLiteral("applications-internet"));
    case Source::Subsonic:      return IconLoader::Load(QStringLiteral("subsonic"));
    case Source::Tidal:         return IconLoader::Load(QStringLiteral("tidal"));
    case Source::Spotify:       return IconLoader::Load(QStringLiteral("spotify"));
    case Source::Qobuz:         return IconLoader::Load(QStringLiteral("qobuz"));
    case Source::SomaFM:        return IconLoader::Load(QStringLiteral("somafm"));
    case Source::RadioParadise: return IconLoader::Load(QStringLiteral("radioparadise"));
    case Source::Unknown:       return IconLoader::Load(QStringLiteral("edit-delete"));
  }
  return IconLoader::Load(QStringLiteral("edit-delete"));

}

QString Song::TextForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return QStringLiteral("Wav");
    case FileType::FLAC:        return QStringLiteral("FLAC");
    case FileType::WavPack:     return QStringLiteral("WavPack");
    case FileType::OggFlac:     return QStringLiteral("Ogg FLAC");
    case FileType::OggVorbis:   return QStringLiteral("Ogg Vorbis");
    case FileType::OggOpus:     return QStringLiteral("Ogg Opus");
    case FileType::OggSpeex:    return QStringLiteral("Ogg Speex");
    case FileType::MPEG:        return QStringLiteral("MPEG");
    case FileType::MP4:         return QStringLiteral("MP4 AAC");
    case FileType::ASF:         return QStringLiteral("Windows Media audio");
    case FileType::AIFF:        return QStringLiteral("AIFF");
    case FileType::MPC:         return QStringLiteral("MPC");
    case FileType::TrueAudio:   return QStringLiteral("TrueAudio");
    case FileType::DSF:         return QStringLiteral("DSF");
    case FileType::DSDIFF:      return QStringLiteral("DSDIFF");
    case FileType::PCM:         return QStringLiteral("PCM");
    case FileType::APE:         return QStringLiteral("Monkey's Audio");
    case FileType::MOD:
    case FileType::S3M:
    case FileType::XM:
    case FileType::IT:          return QStringLiteral("Module Music Format");
    case FileType::CDDA:        return QStringLiteral("CDDA");
    case FileType::SPC:         return QStringLiteral("SNES SPC700");
    case FileType::VGM:         return QStringLiteral("VGM");
    case FileType::Stream:      return QStringLiteral("Stream");
    case FileType::Unknown:
    default:                         return QObject::tr("Unknown");
  }

}

QString Song::ExtensionForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return QStringLiteral("wav");
    case FileType::FLAC:        return QStringLiteral("flac");
    case FileType::WavPack:     return QStringLiteral("wv");
    case FileType::OggFlac:     return QStringLiteral("flac");
    case FileType::OggVorbis:   return QStringLiteral("ogg");
    case FileType::OggOpus:     return QStringLiteral("opus");
    case FileType::OggSpeex:    return QStringLiteral("spx");
    case FileType::MPEG:        return QStringLiteral("mp3");
    case FileType::MP4:         return QStringLiteral("mp4");
    case FileType::ASF:         return QStringLiteral("wma");
    case FileType::AIFF:        return QStringLiteral("aiff");
    case FileType::MPC:         return QStringLiteral("mpc");
    case FileType::TrueAudio:   return QStringLiteral("tta");
    case FileType::DSF:         return QStringLiteral("dsf");
    case FileType::DSDIFF:      return QStringLiteral("dsd");
    case FileType::APE:         return QStringLiteral("ape");
    case FileType::MOD:         return QStringLiteral("mod");
    case FileType::S3M:         return QStringLiteral("s3m");
    case FileType::XM:          return QStringLiteral("xm");
    case FileType::IT:          return QStringLiteral("it");
    case FileType::SPC:         return QStringLiteral("spc");
    case FileType::VGM:         return QStringLiteral("vgm");
    case FileType::Unknown:
    default:                         return QStringLiteral("dat");
  }

}

QIcon Song::IconForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return IconLoader::Load(QStringLiteral("wav"));
    case FileType::FLAC:        return IconLoader::Load(QStringLiteral("flac"));
    case FileType::WavPack:     return IconLoader::Load(QStringLiteral("wavpack"));
    case FileType::OggFlac:     return IconLoader::Load(QStringLiteral("flac"));
    case FileType::OggVorbis:   return IconLoader::Load(QStringLiteral("vorbis"));
    case FileType::OggOpus:     return IconLoader::Load(QStringLiteral("opus"));
    case FileType::OggSpeex:    return IconLoader::Load(QStringLiteral("speex"));
    case FileType::MPEG:        return IconLoader::Load(QStringLiteral("mp3"));
    case FileType::MP4:         return IconLoader::Load(QStringLiteral("mp4"));
    case FileType::ASF:         return IconLoader::Load(QStringLiteral("wma"));
    case FileType::AIFF:        return IconLoader::Load(QStringLiteral("aiff"));
    case FileType::MPC:         return IconLoader::Load(QStringLiteral("mpc"));
    case FileType::TrueAudio:   return IconLoader::Load(QStringLiteral("trueaudio"));
    case FileType::DSF:         return IconLoader::Load(QStringLiteral("dsf"));
    case FileType::DSDIFF:      return IconLoader::Load(QStringLiteral("dsd"));
    case FileType::PCM:         return IconLoader::Load(QStringLiteral("pcm"));
    case FileType::APE:         return IconLoader::Load(QStringLiteral("ape"));
    case FileType::MOD:         return IconLoader::Load(QStringLiteral("mod"));
    case FileType::S3M:         return IconLoader::Load(QStringLiteral("s3m"));
    case FileType::XM:          return IconLoader::Load(QStringLiteral("xm"));
    case FileType::IT:          return IconLoader::Load(QStringLiteral("it"));
    case FileType::CDDA:        return IconLoader::Load(QStringLiteral("cd"));
    case FileType::Stream:      return IconLoader::Load(QStringLiteral("applications-internet"));
    case FileType::Unknown:
    default:                    return IconLoader::Load(QStringLiteral("edit-delete"));
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

  if (mimetype.compare(QLatin1String("audio/wav"), Qt::CaseInsensitive) == 0 || mimetype.compare(QLatin1String("audio/x-wav"), Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (mimetype.compare(QLatin1String("audio/x-flac"), Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (mimetype.compare(QLatin1String("audio/x-wavpack"), Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (mimetype.compare(QLatin1String("audio/x-vorbis"), Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  if (mimetype.compare(QLatin1String("audio/x-opus"), Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (mimetype.compare(QLatin1String("audio/x-speex"), Qt::CaseInsensitive) == 0)  return FileType::OggSpeex;
  // Gstreamer returns audio/mpeg for both MP3 and MP4/AAC.
  // if (mimetype.compare("audio/mpeg", Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (mimetype.compare(QLatin1String("audio/aac"), Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (mimetype.compare(QLatin1String("audio/x-wma"), Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (mimetype.compare(QLatin1String("audio/aiff"), Qt::CaseInsensitive) == 0 || mimetype.compare(QLatin1String("audio/x-aiff"), Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (mimetype.compare(QLatin1String("audio/x-musepack"), Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (mimetype.compare(QLatin1String("application/x-project"), Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (mimetype.compare(QLatin1String("audio/x-dsf"), Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (mimetype.compare(QLatin1String("audio/x-dsd"), Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (mimetype.compare(QLatin1String("audio/x-ape"), Qt::CaseInsensitive) == 0 || mimetype.compare(QLatin1String("application/x-ape"), Qt::CaseInsensitive) == 0 || mimetype.compare(QLatin1String("audio/x-ffmpeg-parsed-ape"), Qt::CaseInsensitive) == 0) return FileType::APE;
  if (mimetype.compare(QLatin1String("audio/x-mod"), Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (mimetype.compare(QLatin1String("audio/x-s3m"), Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (mimetype.compare(QLatin1String("audio/x-spc"), Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (mimetype.compare(QLatin1String("audio/x-vgm"), Qt::CaseInsensitive) == 0) return FileType::VGM;

  return FileType::Unknown;

}

Song::FileType Song::FiletypeByDescription(const QString &text) {

  if (text.compare(QLatin1String("WAV"), Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (text.compare(QLatin1String("Free Lossless Audio Codec (FLAC)"), Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (text.compare(QLatin1String("Wavpack"), Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (text.compare(QLatin1String("Vorbis"), Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  if (text.compare(QLatin1String("Opus"), Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (text.compare(QLatin1String("Speex"), Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  if (text.compare(QLatin1String("MPEG-1 Layer 2 (MP2)"), Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (text.compare(QLatin1String("MPEG-1 Layer 3 (MP3)"), Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (text.compare(QLatin1String("MPEG-4 AAC"), Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (text.compare(QLatin1String("WMA"), Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (text.compare(QLatin1String("Audio Interchange File Format"), Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (text.compare(QLatin1String("MPC"), Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (text.compare(QLatin1String("Musepack (MPC)"), Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (text.compare(QLatin1String("audio/x-dsf"), Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (text.compare(QLatin1String("audio/x-dsd"), Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (text.compare(QLatin1String("audio/x-ffmpeg-parsed-ape"), Qt::CaseInsensitive) == 0) return FileType::APE;
  if (text.compare(QLatin1String("Module Music Format (MOD)"), Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (text.compare(QLatin1String("Module Music Format (MOD)"), Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (text.compare(QLatin1String("SNES SPC700"), Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (text.compare(QLatin1String("VGM"), Qt::CaseInsensitive) == 0) return FileType::VGM;

  return FileType::Unknown;

}

Song::FileType Song::FiletypeByExtension(const QString &ext) {

  if (ext.compare(QLatin1String("wav"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("wave"), Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (ext.compare(QLatin1String("flac"), Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (ext.compare(QLatin1String("wavpack"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("wv"), Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (ext.compare(QLatin1String("opus"), Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (ext.compare(QLatin1String("speex"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("spx"), Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  if (ext.compare(QLatin1String("mp2"), Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (ext.compare(QLatin1String("mp3"), Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (ext.compare(QLatin1String("mp4"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("m4a"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("aac"), Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (ext.compare(QLatin1String("asf"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("wma"), Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (ext.compare(QLatin1String("aiff"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("aif"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("aifc"), Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (ext.compare(QLatin1String("mpc"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("mp+"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("mpp"), Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (ext.compare(QLatin1String("dsf"), Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (ext.compare(QLatin1String("dsd"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("dff"), Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (ext.compare(QLatin1String("ape"), Qt::CaseInsensitive) == 0) return FileType::APE;
  if (ext.compare(QLatin1String("mod"), Qt::CaseInsensitive) == 0 ||
      ext.compare(QLatin1String("module"), Qt::CaseInsensitive) == 0 ||
      ext.compare(QLatin1String("nst"), Qt::CaseInsensitive) == 0||
      ext.compare(QLatin1String("wow"), Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (ext.compare(QLatin1String("s3m"), Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (ext.compare(QLatin1String("xm"), Qt::CaseInsensitive) == 0) return FileType::XM;
  if (ext.compare(QLatin1String("it"), Qt::CaseInsensitive) == 0) return FileType::IT;
  if (ext.compare(QLatin1String("spc"), Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (ext.compare(QLatin1String("vgm"), Qt::CaseInsensitive) == 0) return FileType::VGM;

  return FileType::Unknown;

}

QString Song::ImageCacheDir(const Source source) {

  switch (source) {
    case Source::Collection:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/collectionalbumcovers");
    case Source::Subsonic:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/subsonicalbumcovers");
    case Source::Tidal:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/tidalalbumcovers");
    case Source::Spotify:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/spotifyalbumcovers");
    case Source::Qobuz:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/qobuzalbumcovers");
    case Source::Device:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/devicealbumcovers");
    case Source::LocalFile:
    case Source::CDDA:
    case Source::Stream:
    case Source::SomaFM:
    case Source::RadioParadise:
    case Source::Unknown:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/albumcovers");
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

void Song::Init(const QString &title, const QString &artist, const QString &album, const qint64 length_nanosec) {

  d->valid_ = true;

  set_title(title);
  set_artist(artist);
  set_album(album);

  set_length_nanosec(length_nanosec);

}

void Song::Init(const QString &title, const QString &artist, const QString &album, const qint64 beginning, const qint64 end) {

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
  set_title(QString::fromStdString(pb.title()));
  set_album(QString::fromStdString(pb.album()));
  set_artist(QString::fromStdString(pb.artist()));
  set_albumartist(QString::fromStdString(pb.albumartist()));
  d->track_ = pb.track();
  d->disc_ = pb.disc();
  d->year_ = pb.year();
  d->originalyear_ = pb.originalyear();
  d->genre_ = QString::fromStdString(pb.genre());
  d->compilation_ = pb.compilation();
  d->composer_ = QString::fromStdString(pb.composer());
  d->performer_ = QString::fromStdString(pb.performer());
  d->grouping_ = QString::fromStdString(pb.grouping());
  d->comment_ = QString::fromStdString(pb.comment());
  d->lyrics_ = QString::fromStdString(pb.lyrics());
  set_length_nanosec(static_cast<qint64>(pb.length_nanosec()));
  d->bitrate_ = pb.bitrate();
  d->samplerate_ = pb.samplerate();
  d->bitdepth_ = pb.bitdepth();
  set_url(QUrl::fromEncoded(QString::fromStdString(pb.url()).toUtf8()));
  d->basefilename_ = QString::fromStdString(pb.basefilename());
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

  d->art_embedded_ = pb.has_art_embedded();

  d->acoustid_id_ = QString::fromStdString(pb.acoustid_id());
  d->acoustid_fingerprint_ = QString::fromStdString(pb.acoustid_fingerprint());

  d->musicbrainz_album_artist_id_ = QString::fromStdString(pb.musicbrainz_album_artist_id());
  d->musicbrainz_artist_id_ = QString::fromStdString(pb.musicbrainz_artist_id().data());
  d->musicbrainz_original_artist_id_ = QString::fromStdString(pb.musicbrainz_original_artist_id());
  d->musicbrainz_album_id_ = QString::fromStdString(pb.musicbrainz_album_id());
  d->musicbrainz_original_album_id_ = QString::fromStdString(pb.musicbrainz_original_album_id());
  d->musicbrainz_recording_id_ = QString::fromStdString(pb.musicbrainz_recording_id());
  d->musicbrainz_track_id_ = QString::fromStdString(pb.musicbrainz_track_id());
  d->musicbrainz_disc_id_ = QString::fromStdString(pb.musicbrainz_disc_id());
  d->musicbrainz_release_group_id_ = QString::fromStdString(pb.musicbrainz_release_group_id());
  d->musicbrainz_work_id_ = QString::fromStdString(pb.musicbrainz_work_id());

  d->suspicious_tags_ = pb.suspicious_tags();

  InitArtManual();

}

void Song::ToProtobuf(spb::tagreader::SongMetadata *pb) const {

  const QByteArray url(d->url_.toEncoded());

  pb->set_valid(d->valid_);
  pb->set_title(d->title_.toStdString());
  pb->set_album(d->album_.toStdString());
  pb->set_artist(d->artist_.toStdString());
  pb->set_albumartist(d->albumartist_.toStdString());
  pb->set_track(d->track_);
  pb->set_disc(d->disc_);
  pb->set_year(d->year_);
  pb->set_originalyear(d->originalyear_);
  pb->set_genre(d->genre_.toStdString());
  pb->set_compilation(d->compilation_);
  pb->set_composer(d->composer_.toStdString());
  pb->set_performer(d->performer_.toStdString());
  pb->set_grouping(d->grouping_.toStdString());
  pb->set_comment(d->comment_.toStdString());
  pb->set_lyrics(d->lyrics_.toStdString());
  pb->set_length_nanosec(length_nanosec());
  pb->set_bitrate(d->bitrate_);
  pb->set_samplerate(d->samplerate_);
  pb->set_bitdepth(d->bitdepth_);
  pb->set_url(url.constData(), url.size());
  pb->set_basefilename(d->basefilename_.toStdString());
  pb->set_filetype(static_cast<spb::tagreader::SongMetadata_FileType>(d->filetype_));
  pb->set_filesize(d->filesize_);
  pb->set_mtime(d->mtime_);
  pb->set_ctime(d->ctime_);
  pb->set_playcount(d->playcount_);
  pb->set_skipcount(d->skipcount_);
  pb->set_lastplayed(d->lastplayed_);
  pb->set_lastseen(d->lastseen_);
  pb->set_art_embedded(d->art_embedded_);
  pb->set_rating(d->rating_);

  pb->set_acoustid_id(d->acoustid_id_.toStdString());
  pb->set_acoustid_fingerprint(d->acoustid_fingerprint_.toStdString());

  pb->set_musicbrainz_album_artist_id(d->musicbrainz_album_artist_id_.toStdString());
  pb->set_musicbrainz_artist_id(d->musicbrainz_artist_id_.toStdString());
  pb->set_musicbrainz_original_artist_id(d->musicbrainz_original_artist_id_.toStdString());
  pb->set_musicbrainz_album_id(d->musicbrainz_album_id_.toStdString());
  pb->set_musicbrainz_original_album_id(d->musicbrainz_original_album_id_.toStdString());
  pb->set_musicbrainz_recording_id(d->musicbrainz_recording_id_.toStdString());
  pb->set_musicbrainz_track_id(d->musicbrainz_track_id_.toStdString());
  pb->set_musicbrainz_disc_id(d->musicbrainz_disc_id_.toStdString());
  pb->set_musicbrainz_release_group_id(d->musicbrainz_release_group_id_.toStdString());
  pb->set_musicbrainz_work_id(d->musicbrainz_work_id_.toStdString());

  pb->set_suspicious_tags(d->suspicious_tags_);

}

void Song::InitFromQuery(const QSqlRecord &r, const bool reliable_metadata, const int col) {

  Q_ASSERT(kRowIdColumns.count() + col <= r.count());

  d->id_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("ROWID")) + col);

  set_title(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("title")) + col));
  set_album(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("album")) + col));
  set_artist(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("artist")) + col));
  set_albumartist(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("albumartist")) + col));
  d->track_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("track")) + col);
  d->disc_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("disc")) + col);
  d->year_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("year")) + col);
  d->originalyear_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("originalyear")) + col);
  d->genre_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("genre")) + col);
  d->compilation_ = r.value(ColumnIndex(QStringLiteral("compilation")) + col).toBool();
  d->composer_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("composer")) + col);
  d->performer_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("performer")) + col);
  d->grouping_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("grouping")) + col);
  d->comment_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("comment")) + col);
  d->lyrics_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("lyrics")) + col);
  d->artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("artist_id")) + col);
  d->album_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("album_id")) + col);
  d->song_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("song_id")) + col);
  d->beginning_ = r.value(ColumnIndex(QStringLiteral("beginning")) + col).isNull() ? 0 : r.value(ColumnIndex(QStringLiteral("beginning")) + col).toLongLong();
  set_length_nanosec(SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("length")) + col));
  d->bitrate_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("bitrate")) + col);
  d->samplerate_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("samplerate")) + col);
  d->bitdepth_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("bitdepth")) + col);
  if (!r.value(ColumnIndex(QStringLiteral("ebur128_integrated_loudness_lufs")) + col).isNull()) {
    d->ebur128_integrated_loudness_lufs_ = r.value(ColumnIndex(QStringLiteral("ebur128_integrated_loudness_lufs")) + col).toDouble();
  }
  if (!r.value(ColumnIndex(QStringLiteral("ebur128_loudness_range_lu")) + col).isNull()) {
    d->ebur128_loudness_range_lu_ = r.value(ColumnIndex(QStringLiteral("ebur128_loudness_range_lu")) + col).toDouble();
  }
  d->source_ = static_cast<Source>(r.value(ColumnIndex(QStringLiteral("source")) + col).isNull() ? 0 : r.value(ColumnIndex(QStringLiteral("source")) + col).toInt());
  d->directory_id_ = SqlHelper::ValueToInt(r, ColumnIndex(QStringLiteral("directory_id")) + col);
  set_url(QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("url")) + col).toUtf8()));
  d->basefilename_ = QFileInfo(d->url_.toLocalFile()).fileName();
  d->filetype_ = FileType(r.value(ColumnIndex(QStringLiteral("filetype")) + col).isNull() ? 0 : r.value(ColumnIndex(QStringLiteral("filetype")) + col).toInt());
  d->filesize_ = SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("filesize")) + col);
  d->mtime_ = SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("mtime")) + col);
  d->ctime_ = SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("ctime")) + col);
  d->unavailable_ = r.value(ColumnIndex(QStringLiteral("unavailable")) + col).toBool();
  d->fingerprint_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("fingerprint")) + col);
  d->playcount_ = SqlHelper::ValueToUInt(r, ColumnIndex(QStringLiteral("playcount")) + col);
  d->skipcount_ = SqlHelper::ValueToUInt(r, ColumnIndex(QStringLiteral("skipcount")) + col);
  d->lastplayed_ = SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("lastplayed")) + col);
  d->lastseen_ = SqlHelper::ValueToLongLong(r, ColumnIndex(QStringLiteral("lastseen")) + col);
  d->compilation_detected_ = SqlHelper::ValueToBool(r, ColumnIndex(QStringLiteral("compilation_detected")) + col);
  d->compilation_on_ = SqlHelper::ValueToBool(r, ColumnIndex(QStringLiteral("compilation_on")) + col);
  d->compilation_off_ = SqlHelper::ValueToBool(r, ColumnIndex(QStringLiteral("compilation_off")) + col);

  d->art_embedded_ = SqlHelper::ValueToBool(r, ColumnIndex(QStringLiteral("art_embedded")) + col);
  d->art_automatic_ = QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("art_automatic")) + col).toUtf8());
  d->art_manual_ = QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("art_manual")) + col).toUtf8());
  d->art_unset_ = SqlHelper::ValueToBool(r, ColumnIndex(QStringLiteral("art_unset")) + col);

  d->cue_path_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("cue_path")) + col);
  d->rating_ = SqlHelper::ValueToFloat(r, ColumnIndex(QStringLiteral("rating")) + col);

  d->acoustid_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("acoustid_id")) + col);
  d->acoustid_fingerprint_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("acoustid_fingerprint")) + col);

  d->musicbrainz_album_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_album_artist_id")) + col);
  d->musicbrainz_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_artist_id")) + col);
  d->musicbrainz_original_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_original_artist_id")) + col);
  d->musicbrainz_album_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_album_id")) + col);
  d->musicbrainz_original_album_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_original_album_id")) + col);
  d->musicbrainz_recording_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_recording_id")) + col);
  d->musicbrainz_track_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_track_id")) + col);
  d->musicbrainz_disc_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_disc_id")) + col);
  d->musicbrainz_release_group_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_release_group_id")) + col);
  d->musicbrainz_work_id_ = SqlHelper::ValueToString(r, ColumnIndex(QStringLiteral("musicbrainz_work_id")) + col);

  d->valid_ = true;
  d->init_from_file_ = reliable_metadata;

  InitArtManual();

}

void Song::InitFromQuery(const SqlQuery &query, const bool reliable_metadata, const int col) {

  InitFromQuery(query.record(), reliable_metadata, col);

}

void Song::InitFromQuery(const SqlRow &row, const bool reliable_metadata, const int col) {

  InitFromQuery(row.record(), reliable_metadata, col);

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

  // If we don't have cover art, check if we have one in the cache
  if (d->art_manual_.isEmpty() && !effective_albumartist().isEmpty() && !effective_album().isEmpty()) {
    QString filename = QString::fromLatin1(CoverUtils::Sha1CoverHash(effective_albumartist(), effective_album()).toHex()) + QStringLiteral(".jpg");
    QString path(ImageCacheDir(d->source_) + QLatin1Char('/') + filename);
    if (QFile::exists(path)) {
      d->art_manual_ = QUrl::fromLocalFile(path);
    }
  }

}

void Song::InitArtAutomatic() {

  if (d->art_automatic_.isEmpty() && d->source_ == Source::LocalFile && d->url_.isLocalFile()) {
    // Pick the first image file in the album directory.
    QFileInfo file(d->url_.toLocalFile());
    QDir dir(file.path());
    QStringList files = dir.entryList(QStringList() << QStringLiteral("*.jpg") << QStringLiteral("*.png") << QStringLiteral("*.gif") << QStringLiteral("*.jpeg"), QDir::Files|QDir::Readable, QDir::Name);
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
  filename.replace(QLatin1Char(':'), QLatin1Char('/'));
  if (prefix.contains(QLatin1String("://"))) {
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
      QString cover_file = cover_path + QLatin1Char('/') + QString::fromLatin1(CoverUtils::Sha1CoverHash(effective_albumartist(), effective_album()).toHex()) + QStringLiteral(".jpg");
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

  d->url_ = QUrl(QStringLiteral("mtp://%1/%2").arg(host, QString::number(track->item_id)));
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
    case LIBMTP_FILETYPE_OGG:  d->filetype_ = FileType::OggVorbis; break;
    case LIBMTP_FILETYPE_FLAC: d->filetype_ = FileType::OggFlac;   break;
    case LIBMTP_FILETYPE_MP2:
    case LIBMTP_FILETYPE_MP3:  d->filetype_ = FileType::MPEG;      break;
    case LIBMTP_FILETYPE_M4A:
    case LIBMTP_FILETYPE_MP4:
    case LIBMTP_FILETYPE_AAC:  d->filetype_ = FileType::MP4;       break;
    case LIBMTP_FILETYPE_WMA:  d->filetype_ = FileType::ASF;       break;
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

void Song::BindToQuery(SqlQuery *query) const {

  // Remember to bind these in the same order as kBindSpec

  query->BindStringValue(QStringLiteral(":title"), d->title_);
  query->BindStringValue(QStringLiteral(":album"), d->album_);
  query->BindStringValue(QStringLiteral(":artist"), d->artist_);
  query->BindStringValue(QStringLiteral(":albumartist"), d->albumartist_);
  query->BindIntValue(QStringLiteral(":track"), d->track_);
  query->BindIntValue(QStringLiteral(":disc"), d->disc_);
  query->BindIntValue(QStringLiteral(":year"), d->year_);
  query->BindIntValue(QStringLiteral(":originalyear"), d->originalyear_);
  query->BindStringValue(QStringLiteral(":genre"), d->genre_);
  query->BindBoolValue(QStringLiteral(":compilation"), d->compilation_);
  query->BindStringValue(QStringLiteral(":composer"), d->composer_);
  query->BindStringValue(QStringLiteral(":performer"), d->performer_);
  query->BindStringValue(QStringLiteral(":grouping"), d->grouping_);
  query->BindStringValue(QStringLiteral(":comment"), d->comment_);
  query->BindStringValue(QStringLiteral(":lyrics"), d->lyrics_);

  query->BindStringValue(QStringLiteral(":artist_id"), d->artist_id_);
  query->BindStringValue(QStringLiteral(":album_id"), d->album_id_);
  query->BindStringValue(QStringLiteral(":song_id"), d->song_id_);

  query->BindValue(QStringLiteral(":beginning"), d->beginning_);
  query->BindLongLongValue(QStringLiteral(":length"), length_nanosec());

  query->BindIntValue(QStringLiteral(":bitrate"), d->bitrate_);
  query->BindIntValue(QStringLiteral(":samplerate"), d->samplerate_);
  query->BindIntValue(QStringLiteral(":bitdepth"), d->bitdepth_);

  query->BindValue(QStringLiteral(":source"), static_cast<int>(d->source_));
  query->BindNotNullIntValue(QStringLiteral(":directory_id"), d->directory_id_);
  query->BindUrlValue(QStringLiteral(":url"), d->url_);
  query->BindValue(QStringLiteral(":filetype"), static_cast<int>(d->filetype_));
  query->BindLongLongValueOrZero(QStringLiteral(":filesize"), d->filesize_);
  query->BindLongLongValueOrZero(QStringLiteral(":mtime"), d->mtime_);
  query->BindLongLongValueOrZero(QStringLiteral(":ctime"), d->ctime_);
  query->BindBoolValue(QStringLiteral(":unavailable"), d->unavailable_);

  query->BindStringValue(QStringLiteral(":fingerprint"), d->fingerprint_);

  query->BindValue(QStringLiteral(":playcount"), d->playcount_);
  query->BindValue(QStringLiteral(":skipcount"), d->skipcount_);
  query->BindLongLongValue(QStringLiteral(":lastplayed"), d->lastplayed_);
  query->BindLongLongValue(QStringLiteral(":lastseen"), d->lastseen_);

  query->BindBoolValue(QStringLiteral(":compilation_detected"), d->compilation_detected_);
  query->BindBoolValue(QStringLiteral(":compilation_on"), d->compilation_on_);
  query->BindBoolValue(QStringLiteral(":compilation_off"), d->compilation_off_);
  query->BindBoolValue(QStringLiteral(":compilation_effective"), is_compilation());

  query->BindBoolValue(QStringLiteral(":art_embedded"), d->art_embedded_);
  query->BindUrlValue(QStringLiteral(":art_automatic"), d->art_automatic_);
  query->BindUrlValue(QStringLiteral(":art_manual"), d->art_manual_);
  query->BindBoolValue(QStringLiteral(":art_unset"), d->art_unset_);

  query->BindStringValue(QStringLiteral(":effective_albumartist"), effective_albumartist());
  query->BindIntValue(QStringLiteral(":effective_originalyear"), effective_originalyear());

  query->BindValue(QStringLiteral(":cue_path"), d->cue_path_);

  query->BindFloatValue(QStringLiteral(":rating"), d->rating_);

  query->BindStringValue(QStringLiteral(":acoustid_id"), d->acoustid_id_);
  query->BindStringValue(QStringLiteral(":acoustid_fingerprint"), d->acoustid_fingerprint_);

  query->BindStringValue(QStringLiteral(":musicbrainz_album_artist_id"), d->musicbrainz_album_artist_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_artist_id"), d->musicbrainz_artist_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_original_artist_id"), d->musicbrainz_original_artist_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_album_id"), d->musicbrainz_album_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_original_album_id"), d->musicbrainz_original_album_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_recording_id"), d->musicbrainz_recording_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_track_id"), d->musicbrainz_track_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_disc_id"), d->musicbrainz_disc_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_release_group_id"), d->musicbrainz_release_group_id_);
  query->BindStringValue(QStringLiteral(":musicbrainz_work_id"), d->musicbrainz_work_id_);

  query->BindDoubleOrNullValue(QStringLiteral(":ebur128_integrated_loudness_lufs"), d->ebur128_integrated_loudness_lufs_);
  query->BindDoubleOrNullValue(QStringLiteral(":ebur128_loudness_range_lu"), d->ebur128_loudness_range_lu_);

}

#ifdef HAVE_DBUS
void Song::ToXesam(QVariantMap *map) const {

  using mpris::AddMetadata;
  using mpris::AddMetadataAsList;
  using mpris::AsMPRISDateTimeType;

  AddMetadata(QStringLiteral("xesam:url"), effective_stream_url().toString(), map);
  AddMetadata(QStringLiteral("xesam:title"), PrettyTitle(), map);
  AddMetadataAsList(QStringLiteral("xesam:artist"), artist(), map);
  AddMetadata(QStringLiteral("xesam:album"), album(), map);
  AddMetadataAsList(QStringLiteral("xesam:albumArtist"), albumartist(), map);
  AddMetadata(QStringLiteral("mpris:length"), (length_nanosec() / kNsecPerUsec), map);
  AddMetadata(QStringLiteral("xesam:trackNumber"), track(), map);
  AddMetadataAsList(QStringLiteral("xesam:genre"), genre(), map);
  AddMetadata(QStringLiteral("xesam:discNumber"), disc(), map);
  AddMetadataAsList(QStringLiteral("xesam:comment"), comment(), map);
  AddMetadata(QStringLiteral("xesam:contentCreated"), AsMPRISDateTimeType(ctime()), map);
  AddMetadata(QStringLiteral("xesam:lastUsed"), AsMPRISDateTimeType(lastplayed()), map);
  AddMetadataAsList(QStringLiteral("xesam:composer"), composer(), map);
  AddMetadata(QStringLiteral("xesam:useCount"), static_cast<int>(playcount()), map);

  if (rating() != -1.0) {
    AddMetadata(QStringLiteral("xesam:userRating"), rating(), map);
  }

}
#endif

bool Song::MergeFromEngineMetadata(const EngineMetadata &engine_metadata) {

  d->valid_ = true;

  bool minor = true;

  if (d->init_from_file_ || is_collection_song() || d->url_.isLocalFile()) {
    // This Song was already loaded using taglib. Our tags are probably better than the engine's.
    if (title() != engine_metadata.title && title().isEmpty() && !engine_metadata.title.isEmpty()) {
      set_title(engine_metadata.title);
      minor = false;
    }
    if (artist() != engine_metadata.artist && artist().isEmpty() && !engine_metadata.artist.isEmpty()) {
      set_artist(engine_metadata.artist);
      minor = false;
    }
    if (album() != engine_metadata.album && album().isEmpty() && !engine_metadata.album.isEmpty()) {
      set_album(engine_metadata.album);
      minor = false;
    }
    if (comment().isEmpty() && !engine_metadata.comment.isEmpty()) set_comment(engine_metadata.comment);
    if (genre().isEmpty() && !engine_metadata.genre.isEmpty()) set_genre(engine_metadata.genre);
    if (lyrics().isEmpty() && !engine_metadata.lyrics.isEmpty()) set_lyrics(engine_metadata.lyrics);
  }
  else {
    if (title() != engine_metadata.title && !engine_metadata.title.isEmpty()) {
      set_title(engine_metadata.title);
      minor = false;
    }
    if (artist() != engine_metadata.artist && !engine_metadata.artist.isEmpty()) {
      set_artist(engine_metadata.artist);
      minor = false;
    }
    if (album() != engine_metadata.album && !engine_metadata.album.isEmpty()) {
      set_album(engine_metadata.album);
      minor = false;
    }
    if (!engine_metadata.comment.isEmpty()) set_comment(engine_metadata.comment);
    if (!engine_metadata.genre.isEmpty()) set_genre(engine_metadata.genre);
    if (!engine_metadata.lyrics.isEmpty()) set_lyrics(engine_metadata.lyrics);
  }

  if (engine_metadata.length > 0) set_length_nanosec(engine_metadata.length);
  if (engine_metadata.year > 0) d->year_ = engine_metadata.year;
  if (engine_metadata.track > 0) d->track_ = engine_metadata.track;
  if (engine_metadata.filetype != FileType::Unknown) d->filetype_ = engine_metadata.filetype;
  if (engine_metadata.samplerate > 0) d->samplerate_ = engine_metadata.samplerate;
  if (engine_metadata.bitdepth > 0) d->bitdepth_ = engine_metadata.bitdepth;
  if (engine_metadata.bitrate > 0) d->bitrate_ = engine_metadata.bitrate;

  return minor;

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
  set_art_unset(other.art_unset());
  set_compilation_on(other.compilation_on());
  set_compilation_off(other.compilation_off());

}

QString Song::AlbumKey() const {
  return QStringLiteral("%1|%2|%3").arg(is_compilation() ? QLatin1String("_compilation") : effective_albumartist(), has_cue() ? cue_path() : QLatin1String(""), effective_album());
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const Song &song) {
#else
uint qHash(const Song &song) {
#endif
  // Should compare the same fields as operator==
  return qHash(song.url().toString()) ^ qHash(song.beginning_nanosec());
}

size_t HashSimilar(const Song &song) {
  // Should compare the same fields as function IsSimilar
  return qHash(song.title().toLower()) ^ qHash(song.artist().toLower()) ^ qHash(song.album().toLower());
}

bool Song::ContainsRegexList(const QString &str, const RegularExpressionList &regex_list) {

  for (const QRegularExpression &regex : regex_list) {
    if (str.contains(regex)) return true;
  }

  return false;

}

QString Song::StripRegexList(QString str, const RegularExpressionList &regex_list) {

  for (const QRegularExpression &regex : regex_list) {
    str = str.remove(regex);
  }

  return str;

}

bool Song::AlbumContainsDisc(const QString &album) {

  return ContainsRegexList(album, kAlbumDisc);

}

QString Song::AlbumRemoveDisc(const QString &album) {

  return StripRegexList(album, kAlbumDisc);

}

QString Song::AlbumRemoveMisc(const QString &album) {

  return StripRegexList(album, kAlbumMisc);

}

QString Song::AlbumRemoveDiscMisc(const QString &album) {

  return StripRegexList(album, RegularExpressionList() << kAlbumDisc << kAlbumMisc);

}

QString Song::TitleRemoveMisc(const QString &title) {

  return StripRegexList(title, kTitleMisc);

}
