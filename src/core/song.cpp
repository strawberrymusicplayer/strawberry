/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <algorithm>

#ifdef HAVE_GPOD
#  include <gdk-pixbuf/gdk-pixbuf.h>
#  include <gpod/itdb.h>
#endif

#ifdef HAVE_MTP
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
#include <QSqlRecord>

#include <taglib/tstring.h>

#include "core/standardpaths.h"
#include "core/iconloader.h"
#include "core/enginemetadata.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/coverutils.h"
#include "constants/timeconstants.h"
#include "utilities/sqlhelper.h"

#include "song.h"
#include "sqlquery.h"
#include "sqlrow.h"
#ifdef HAVE_MPRIS2
#  include "mpris2/mpris_common.h"
#endif

using namespace Qt::Literals::StringLiterals;

const QStringList Song::kColumns = QStringList() << u"title"_s
                                                 << u"titlesort"_s
                                                 << u"album"_s
                                                 << u"albumsort"_s
                                                 << u"artist"_s
                                                 << u"artistsort"_s
                                                 << u"albumartist"_s
                                                 << u"albumartistsort"_s
                                                 << u"track"_s
                                                 << u"disc"_s
                                                 << u"year"_s
                                                 << u"originalyear"_s
                                                 << u"genre"_s
                                                 << u"compilation"_s
                                                 << u"composer"_s
                                                 << u"composersort"_s
                                                 << u"performer"_s
                                                 << u"performersort"_s
                                                 << u"grouping"_s
                                                 << u"comment"_s
                                                 << u"lyrics"_s

                                                 << u"artist_id"_s
                                                 << u"album_id"_s
                                                 << u"song_id"_s

                                                 << u"beginning"_s
                                                 << u"length"_s

                                                 << u"bitrate"_s
                                                 << u"samplerate"_s
                                                 << u"bitdepth"_s

                                                 << u"source"_s
                                                 << u"directory_id"_s
                                                 << u"url"_s
                                                 << u"filetype"_s
                                                 << u"filesize"_s
                                                 << u"mtime"_s
                                                 << u"ctime"_s
                                                 << u"unavailable"_s

                                                 << u"fingerprint"_s

                                                 << u"playcount"_s
                                                 << u"skipcount"_s
                                                 << u"lastplayed"_s
                                                 << u"lastseen"_s

                                                 << u"compilation_detected"_s
                                                 << u"compilation_on"_s
                                                 << u"compilation_off"_s
                                                 << u"compilation_effective"_s

                                                 << u"art_embedded"_s
                                                 << u"art_automatic"_s
                                                 << u"art_manual"_s
                                                 << u"art_unset"_s

                                                 << u"effective_albumartist"_s
                                                 << u"effective_originalyear"_s

                                                 << u"cue_path"_s

                                                 << u"rating"_s
                                                 << u"bpm"_s
                                                 << u"mood"_s
                                                 << u"initial_key"_s

                                                 << u"acoustid_id"_s
                                                 << u"acoustid_fingerprint"_s

                                                 << u"musicbrainz_album_artist_id"_s
                                                 << u"musicbrainz_artist_id"_s
                                                 << u"musicbrainz_original_artist_id"_s
                                                 << u"musicbrainz_album_id"_s
                                                 << u"musicbrainz_original_album_id"_s
                                                 << u"musicbrainz_recording_id"_s
                                                 << u"musicbrainz_track_id"_s
                                                 << u"musicbrainz_disc_id"_s
                                                 << u"musicbrainz_release_group_id"_s
                                                 << u"musicbrainz_work_id"_s

                                                 << u"ebur128_integrated_loudness_lufs"_s
                                                 << u"ebur128_loudness_range_lu"_s

                                                 ;

const QStringList Song::kRowIdColumns = QStringList() << u"ROWID"_s << kColumns;

const QString Song::kColumnSpec = kColumns.join(", "_L1);
const QString Song::kRowIdColumnSpec = kRowIdColumns.join(", "_L1);
const QString Song::kBindSpec = Utilities::Prepend(u":"_s, kColumns).join(", "_L1);
const QString Song::kUpdateSpec = Utilities::Updateify(kColumns).join(", "_L1);

const QStringList Song::kTextSearchColumns = QStringList()      << u"title"_s
                                                                << u"album"_s
                                                                << u"artist"_s
                                                                << u"albumartist"_s
                                                                << u"composer"_s
                                                                << u"performer"_s
                                                                << u"grouping"_s
                                                                << u"genre"_s
                                                                << u"comment"_s
                                                                << u"filename"_s
                                                                << u"url"_s;

const QStringList Song::kIntSearchColumns = QStringList()       << u"track"_s
                                                                << u"year"_s
                                                                << u"samplerate"_s
                                                                << u"bitdepth"_s
                                                                << u"bitrate"_s;

const QStringList Song::kUIntSearchColumns = QStringList()      << u"playcount"_s
                                                                << u"skipcount"_s;

const QStringList Song::kInt64SearchColumns = QStringList()     << u"length"_s;

const QStringList Song::kFloatSearchColumns = QStringList()     << u"rating"_s;

const QStringList Song::kNumericalSearchColumns = QStringList() << kIntSearchColumns
                                                                << kUIntSearchColumns
                                                                << kInt64SearchColumns
                                                                << kFloatSearchColumns;

const QStringList Song::kSearchColumns = QStringList() << kTextSearchColumns
                                                       << kNumericalSearchColumns;

const Song::RegularExpressionList Song::kAlbumDisc = Song::RegularExpressionList()
    << QRegularExpression(u"\\s+-*\\s*(Disc|CD)\\s*([0-9]{1,2})$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\(\\s*(Disc|CD)\\s*([0-9]{1,2})\\)$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\[\\s*(Disc|CD)\\s*([0-9]{1,2})\\]$"_s, QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kRemastered = Song::RegularExpressionList()
    << QRegularExpression(u"\\s+-*\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\(\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*\\)\\s*$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\[\\s*(([0-9]{4})*\\s*Remastered|([0-9]{4})*\\s*Remaster)\\s*(Version)*\\s*\\]\\s*$"_s, QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kExplicit = Song::RegularExpressionList()
    << QRegularExpression(u"\\s+-*\\s*Explicit\\s*$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\(\\s*Explicit\\s*\\)\\s*$"_s, QRegularExpression::CaseInsensitiveOption)
    << QRegularExpression(u"\\s+-*\\s*\\[\\s*Explicit\\s*\\]\\s*$"_s, QRegularExpression::CaseInsensitiveOption);

const Song::RegularExpressionList Song::kAlbumMisc = Song::RegularExpressionList()
    << kRemastered
    << kExplicit;

const Song::RegularExpressionList Song::kTitleMisc = Song::RegularExpressionList()
    << kRemastered
    << kExplicit;

const QStringList Song::kArticles = QStringList() << u"the "_s << u"a "_s << u"an "_s;

const QStringList Song::kAcceptedExtensions = QStringList() << u"wav"_s
                                                            << u"flac"_s
                                                            << u"wv"_s
                                                            << u"ogg"_s
                                                            << u"oga"_s
                                                            << u"opus"_s
                                                            << u"spx"_s
                                                            << u"ape"_s
                                                            << u"mpc"_s
                                                            << u"mp2"_s
                                                            << u"mp3"_s
                                                            << u"m4a"_s
                                                            << u"mp4"_s
                                                            << u"aac"_s
                                                            << u"asf"_s
                                                            << u"asx"_s
                                                            << u"wma"_s
                                                            << u"aif"_s
                                                            << u"aiff"_s
                                                            << u"mka"_s
                                                            << u"tta"_s
                                                            << u"dsf"_s
                                                            << u"dsd"_s
                                                            << u"ac3"_s
                                                            << u"dts"_s
                                                            << u"spc"_s
                                                            << u"vgm"_s;

const QStringList Song::kRejectedExtensions = QStringList() << u"tmp"_s
                                                            << u"tar"_s
                                                            << u"gz"_s
                                                            << u"bz2"_s
                                                            << u"xz"_s
                                                            << u"tbz"_s
                                                            << u"tgz"_s
                                                            << u"z"_s
                                                            << u"zip"_s
                                                            << u"rar"_s
                                                            << u"wvc"_s
                                                            << u"zst"_s;

struct Song::Private : public QSharedData {

  explicit Private(Source source = Source::Unknown);

  int id_;

  bool valid_;

  QString title_;
  QString titlesort_;
  QString album_;
  QString albumsort_;
  QString artist_;
  QString artistsort_;
  QString albumartist_;
  QString albumartistsort_;
  int track_;
  int disc_;
  int year_;
  int originalyear_;
  QString genre_;
  bool compilation_;  // From the file tag
  QString composer_;
  QString composersort_;
  QString performer_;
  QString performersort_;
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
  float bpm_;
  QString mood_;
  QString initial_key_;

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

  QUrl stream_url_;             // Temporary stream URL set by the URL handler.

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
      bpm_(-1),

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
const QString &Song::titlesort() const { return d->titlesort_; }
const QString &Song::album() const { return d->album_; }
const QString &Song::albumsort() const { return d->albumsort_; }
const QString &Song::artist() const { return d->artist_; }
const QString &Song::artistsort() const { return d->artistsort_; }
const QString &Song::albumartist() const { return d->albumartist_; }
const QString &Song::albumartistsort() const { return d->albumartistsort_; }
int Song::track() const { return d->track_; }
int Song::disc() const { return d->disc_; }
int Song::year() const { return d->year_; }
int Song::originalyear() const { return d->originalyear_; }
const QString &Song::genre() const { return d->genre_; }
bool Song::compilation() const { return d->compilation_; }
const QString &Song::composer() const { return d->composer_; }
const QString &Song::composersort() const { return d->composersort_; }
const QString &Song::performer() const { return d->performer_; }
const QString &Song::performersort() const { return d->performersort_; }
const QString &Song::grouping() const { return d->grouping_; }
const QString &Song::comment() const { return d->comment_; }
const QString &Song::lyrics() const { return d->lyrics_; }

QString Song::artist_id() const { return d->artist_id_.isNull() ? ""_L1 : d->artist_id_; }
QString Song::album_id() const { return d->album_id_.isNull() ? ""_L1 : d->album_id_; }
QString Song::song_id() const { return d->song_id_.isNull() ? ""_L1 : d->song_id_; }

qint64 Song::beginning_nanosec() const { return d->beginning_; }
qint64 Song::end_nanosec() const { return d->end_; }
qint64 Song::length_nanosec() const { return d->end_ - d->beginning_; }

int Song::bitrate() const { return d->bitrate_; }
int Song::samplerate() const { return d->samplerate_; }
int Song::bitdepth() const { return d->bitdepth_; }

Song::Source Song::source() const { return d->source_; }
int Song::source_id() const { return static_cast<int>(d->source_); }
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
float Song::bpm() const { return d->bpm_; }
const QString &Song::mood() const { return d->mood_; }
const QString &Song::initial_key() const { return d->initial_key_; }

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

QString *Song::mutable_title() { return &d->title_; }
QString *Song::mutable_album() { return &d->album_; }
QString *Song::mutable_artist() { return &d->artist_; }
QString *Song::mutable_albumartist() { return &d->albumartist_; }
QString *Song::mutable_genre() { return &d->genre_; }
QString *Song::mutable_composer() { return &d->composer_; }
QString *Song::mutable_performer() { return &d->performer_; }
QString *Song::mutable_grouping() { return &d->grouping_; }
QString *Song::mutable_comment() { return &d->comment_; }
QString *Song::mutable_lyrics() { return &d->lyrics_; }
QString *Song::mutable_acoustid_id() { return &d->acoustid_id_; }
QString *Song::mutable_acoustid_fingerprint() { return &d->acoustid_fingerprint_; }
QString *Song::mutable_musicbrainz_album_artist_id() { return &d->musicbrainz_album_artist_id_; }
QString *Song::mutable_musicbrainz_artist_id() { return &d->musicbrainz_artist_id_; }
QString *Song::mutable_musicbrainz_original_artist_id() { return &d->musicbrainz_original_artist_id_; }
QString *Song::mutable_musicbrainz_album_id() { return &d->musicbrainz_album_id_; }
QString *Song::mutable_musicbrainz_original_album_id() { return &d->musicbrainz_original_album_id_; }
QString *Song::mutable_musicbrainz_recording_id() { return &d->musicbrainz_recording_id_; }
QString *Song::mutable_musicbrainz_track_id() { return &d->musicbrainz_track_id_; }
QString *Song::mutable_musicbrainz_disc_id() { return &d->musicbrainz_disc_id_; }
QString *Song::mutable_musicbrainz_release_group_id() { return &d->musicbrainz_release_group_id_; }
QString *Song::mutable_musicbrainz_work_id() { return &d->musicbrainz_work_id_; }

bool Song::init_from_file() const { return d->init_from_file_; }

const QUrl &Song::stream_url() const { return d->stream_url_; }

void Song::set_id(const int id) { d->id_ = id; }
void Song::set_valid(const bool v) { d->valid_ = v; }

void Song::set_title(const QString &v) { d->title_ = v; }
void Song::set_titlesort(const QString &v) { d->titlesort_ = v; }
void Song::set_album(const QString &v) { d->album_ = v; }
void Song::set_albumsort(const QString &v) { d->albumsort_ = v; }
void Song::set_artist(const QString &v) { d->artist_ = v; }
void Song::set_artistsort(const QString &v) { d->artistsort_ = v; }
void Song::set_albumartist(const QString &v) { d->albumartist_ = v; }
void Song::set_albumartistsort(const QString &v) { d->albumartistsort_ = v; }
void Song::set_track(const int v) { d->track_ = v; }
void Song::set_disc(const int v) { d->disc_ = v; }
void Song::set_year(const int v) { d->year_ = v; }
void Song::set_originalyear(const int v) { d->originalyear_ = v; }
void Song::set_genre(const QString &v) { d->genre_ = v; }
void Song::set_compilation(const bool v) { d->compilation_ = v; }
void Song::set_composer(const QString &v) { d->composer_ = v; }
void Song::set_composersort(const QString &v) { d->composersort_ = v; }
void Song::set_performer(const QString &v) { d->performer_ = v; }
void Song::set_performersort(const QString &v) { d->performersort_ = v; }
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
void Song::set_bpm(const float v) { d->bpm_ = v; }
void Song::set_mood(const QString &v) { d->mood_ = v; }
void Song::set_initial_key(const QString &v) { d->initial_key_ = v; }

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

void Song::set_init_from_file(const bool v) { d->init_from_file_ = v; }

void Song::set_stream_url(const QUrl &v) { d->stream_url_ = v; }

void Song::set_title(const TagLib::String &v) { d->title_ = TagLibStringToQString(v); }
void Song::set_titlesort(const TagLib::String &v) { d->titlesort_ = TagLibStringToQString(v); }
void Song::set_album(const TagLib::String &v) { d->album_ = TagLibStringToQString(v); }
void Song::set_albumsort(const TagLib::String &v) { d->albumsort_ = TagLibStringToQString(v); }
void Song::set_artist(const TagLib::String &v) { d->artist_ = TagLibStringToQString(v); }
void Song::set_artistsort(const TagLib::String &v) { d->artistsort_ = TagLibStringToQString(v); }
void Song::set_albumartist(const TagLib::String &v) { d->albumartist_ = TagLibStringToQString(v); }
void Song::set_albumartistsort(const TagLib::String &v) { d->albumartistsort_ = TagLibStringToQString(v); }
void Song::set_genre(const TagLib::String &v) { d->genre_ = TagLibStringToQString(v); }
void Song::set_composer(const TagLib::String &v) { d->composer_ = TagLibStringToQString(v); }
void Song::set_composersort(const TagLib::String &v) { d->composersort_ = TagLibStringToQString(v); }
void Song::set_performer(const TagLib::String &v) { d->performer_ = TagLibStringToQString(v); }
void Song::set_performersort(const TagLib::String &v) { d->performersort_ = TagLibStringToQString(v); }
void Song::set_grouping(const TagLib::String &v) { d->grouping_ = TagLibStringToQString(v); }
void Song::set_comment(const TagLib::String &v) { d->comment_ = TagLibStringToQString(v); }
void Song::set_lyrics(const TagLib::String &v) { d->lyrics_ = TagLibStringToQString(v); }
void Song::set_artist_id(const TagLib::String &v) { d->artist_id_ = TagLibStringToQString(v); }
void Song::set_album_id(const TagLib::String &v) { d->album_id_ = TagLibStringToQString(v); }
void Song::set_song_id(const TagLib::String &v) { d->song_id_ = TagLibStringToQString(v); }
void Song::set_acoustid_id(const TagLib::String &v) { d->acoustid_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_acoustid_fingerprint(const TagLib::String &v) { d->acoustid_fingerprint_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_album_artist_id(const TagLib::String &v) { d->musicbrainz_album_artist_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_artist_id(const TagLib::String &v) { d->musicbrainz_artist_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_original_artist_id(const TagLib::String &v) { d->musicbrainz_original_artist_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_album_id(const TagLib::String &v) { d->musicbrainz_album_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_original_album_id(const TagLib::String &v) { d->musicbrainz_original_album_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_recording_id(const TagLib::String &v) { d->musicbrainz_recording_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_track_id(const TagLib::String &v) { d->musicbrainz_track_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_disc_id(const TagLib::String &v) { d->musicbrainz_disc_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_release_group_id(const TagLib::String &v) { d->musicbrainz_release_group_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_musicbrainz_work_id(const TagLib::String &v) { d->musicbrainz_work_id_ = TagLibStringToQString(v).remove(u' ').replace(u';', u'/'); }
void Song::set_mood(const TagLib::String &v) { d->mood_ = TagLibStringToQString(v); }
void Song::set_initial_key(const TagLib::String &v) { d->initial_key_ = TagLibStringToQString(v); }

const QUrl &Song::effective_url() const { return !d->stream_url_.isEmpty() && d->stream_url_.isValid() ? d->stream_url_ : d->url_; }
const QString &Song::effective_titlesort() const { return d->titlesort_.isEmpty() ? d->title_ : d->titlesort_; }
const QString &Song::effective_albumartist() const { return d->albumartist_.isEmpty() ? d->artist_ : d->albumartist_; }
const QString &Song::effective_albumartistsort() const { return !d->albumartistsort_.isEmpty() ? d->albumartistsort_ : !d->albumartist_.isEmpty() ? d->albumartist_ : effective_artistsort(); }
const QString &Song::effective_artistsort() const { return d->artistsort_.isEmpty() ? d->artist_ : d->artistsort_; }
const QString &Song::effective_album() const { return d->album_.isEmpty() ? d->title_ : d->album_; }
const QString &Song::effective_albumsort() const { return d->albumsort_.isEmpty() ? d->album_ : d->albumsort_; }
const QString &Song::effective_composersort() const { return d->composersort_.isEmpty() ? d->composer_ : d->composersort_; }
const QString &Song::effective_performersort() const { return d->performersort_.isEmpty() ? d->performer_ : d->performersort_; }
int Song::effective_originalyear() const { return d->originalyear_ < 0 ? d->year_ : d->originalyear_; }
const QString &Song::playlist_effective_albumartist() const { return is_compilation() ? d->albumartist_ : effective_albumartist(); }
const QString &Song::playlist_effective_albumartistsort() const { return is_compilation() ? (!d->albumartistsort_.isEmpty() ? d->albumartistsort_ : d->albumartist_) : effective_albumartistsort(); }

bool Song::is_metadata_good() const { return !d->url_.isEmpty() && !d->artist_.isEmpty() && !d->title_.isEmpty(); }
bool Song::is_local_collection_song() const { return d->source_ == Source::Collection; }
bool Song::is_linked_collection_song() const { return IsLinkedCollectionSource(d->source_); }
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
    d->filetype_ == FileType::WAV ||
    d->filetype_ == FileType::AIFF;

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
    d->filetype_ == FileType::WAV ||
    d->filetype_ == FileType::AIFF;

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
    d->filetype_ == FileType::APE ||
    d->filetype_ == FileType::WAV ||
    d->filetype_ == FileType::AIFF;

}

bool Song::comment_supported() const {
  return additional_tags_supported();
}

bool Song::lyrics_supported() const {
  return additional_tags_supported() || d->filetype_ == FileType::ASF;
}

bool Song::albumartistsort_supported() const {
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis || d->filetype_ == FileType::MPEG;
}

bool Song::albumsort_supported() const {
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis || d->filetype_ == FileType::MPEG;
}

bool Song::artistsort_supported() const {
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis || d->filetype_ == FileType::MPEG;
}

bool Song::composersort_supported() const {
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis || d->filetype_ == FileType::MPEG;
}

bool Song::performersort_supported() const {
  // Performer sort is a rare custom field even in vorbis comments, no write support in MPEG formats
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis;
}

bool Song::titlesort_supported() const {
  return d->filetype_ == FileType::FLAC || d->filetype_ == FileType::OggFlac || d->filetype_ == FileType::OggVorbis || d->filetype_ == FileType::MPEG;
}

bool Song::save_embedded_cover_supported(const FileType filetype) {

  return filetype == FileType::FLAC ||
    filetype == FileType::OggVorbis ||
    filetype == FileType::OggOpus ||
    filetype == FileType::MPEG ||
    filetype == FileType::MP4 ||
    filetype == FileType::WAV ||
    filetype == FileType::AIFF;

}

int Song::ColumnIndex(const QString &field) {

  return static_cast<int>(kRowIdColumns.indexOf(field));

}

QString Song::JoinSpec(const QString &table) {
  return Utilities::Prepend(table + QLatin1Char('.'), kRowIdColumns).join(", "_L1);
}

QString Song::PrettyTitle() const {

  QString title(d->title_);

  if (title.isEmpty()) title = d->basefilename_;
  if (title.isEmpty()) title = d->url_.toString();

  return title;

}

QString Song::PrettyTitleWithArtist() const {

  QString title(PrettyTitle());

  if (!d->artist_.isEmpty()) title = d->artist_ + u" - "_s + title;

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

  if (is_compilation() && !d->artist_.isEmpty() && !d->artist_.contains("various"_L1, Qt::CaseInsensitive)) title = d->artist_ + u" - "_s + title;

  return title;

}

QString Song::SampleRateBitDepthToText() const {

  if (d->samplerate_ == -1) return ""_L1;
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

  if (rating == -1.0F) return u"0"_s;

  return QString::number(static_cast<int>(rating * 100));

}

bool Song::IsEditable() const {
  return d->valid_ && d->url_.isValid() && ((d->url_.isLocalFile() && write_tags_supported() && !has_cue()) || d->source_ == Source::Stream);
}

bool Song::IsFileInfoEqual(const Song &other) const {

  return d->beginning_ == other.d->beginning_ &&
         d->end_ == other.d->end_ &&
         d->url_ == other.d->url_ &&
         d->basefilename_ == other.d->basefilename_ &&
         d->filetype_ == other.d->filetype_ &&
         d->filesize_ == other.d->filesize_ &&
         d->mtime_ == other.d->mtime_ &&
         d->ctime_ == other.d->ctime_ &&
         d->mtime_ == other.d->mtime_ &&
         d->stream_url_ == other.d->stream_url_;

}

bool Song::IsMetadataEqual(const Song &other) const {

  return d->title_ == other.d->title_ &&
         d->titlesort_ == other.d->titlesort_ &&
         d->album_ == other.d->album_ &&
         d->albumsort_ == other.d->albumsort_ &&
         d->artist_ == other.d->artist_ &&
         d->artistsort_ == other.d->artistsort_ &&
         d->albumartist_ == other.d->albumartist_ &&
         d->albumartistsort_ == other.d->albumartistsort_ &&
         d->track_ == other.d->track_ &&
         d->disc_ == other.d->disc_ &&
         d->year_ == other.d->year_ &&
         d->originalyear_ == other.d->originalyear_ &&
         d->genre_ == other.d->genre_ &&
         d->compilation_ == other.d->compilation_ &&
         d->composer_ == other.d->composer_ &&
         d->composersort_ == other.d->composersort_ &&
         d->performer_ == other.d->performer_ &&
         d->performersort_ == other.d->performersort_ &&
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
         d->bpm_ == other.d->bpm_ &&
         d->mood_ == other.d->mood_ &&
         d->initial_key_ == other.d->initial_key_ &&
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

bool Song::IsCompilationEqual(const Song &other) const {

  return d->compilation_ == other.d->compilation_ &&
         d->compilation_detected_ == other.d->compilation_detected_ &&
         d->compilation_on_ == other.d->compilation_on_ &&
         d->compilation_off_ == other.d->compilation_off_;

}

bool Song::IsSettingsEqual(const Song &other) const {

  return d->source_ == other.d->source_ &&
         d->directory_id_ == other.d->directory_id_ &&
         d->unavailable_ == other.d->unavailable_;

}

bool Song::IsAllMetadataEqual(const Song &other) const {

  return IsMetadataEqual(other) &&
         IsPlayStatisticsEqual(other) &&
         IsRatingEqual(other) &&
         IsAcoustIdEqual(other) &&
         IsMusicBrainzEqual(other) &&
         IsArtEqual(other) &&
         IsEBUR128Equal(other);

}

bool Song::IsEqual(const Song &other) const {

  return IsFileInfoEqual(other) &&
         IsSettingsEqual(other) &&
         IsAllMetadataEqual(other) &&
         IsFingerprintEqual(other) &&
         IsCompilationEqual(other);

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
         album().compare(other.album(), Qt::CaseInsensitive) == 0 &&
         fingerprint().compare(other.fingerprint()) == 0 &&
         acoustid_fingerprint().compare(other.acoustid_fingerprint()) == 0;
}

Song::Source Song::SourceFromURL(const QUrl &url) {

  if (url.isLocalFile()) return Source::LocalFile;
  if (url.scheme() == u"cdda"_s) return Source::CDDA;
  if (url.scheme() == u"subsonic"_s) return Source::Subsonic;
  if (url.scheme() == u"tidal"_s) return Source::Tidal;
  if (url.scheme() == u"spotify"_s) return Source::Spotify;
  if (url.scheme() == u"qobuz"_s) return Source::Qobuz;
  if (url.scheme() == u"http"_s || url.scheme() == u"https"_s || url.scheme() == u"rtsp"_s) {
    if (url.host().endsWith("tidal.com"_L1, Qt::CaseInsensitive)) { return Source::Tidal; }
    if (url.host().endsWith("spotify.com"_L1, Qt::CaseInsensitive)) { return Source::Spotify; }
    if (url.host().endsWith("qobuz.com"_L1, Qt::CaseInsensitive)) { return Source::Qobuz; }
    if (url.host().endsWith("somafm.com"_L1, Qt::CaseInsensitive)) { return Source::SomaFM; }
    if (url.host().endsWith("radioparadise.com"_L1, Qt::CaseInsensitive)) { return Source::RadioParadise; }
    return Source::Stream;
  }
  else return Source::Unknown;

}

QString Song::TextForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return u"file"_s;
    case Source::Collection:    return u"collection"_s;
    case Source::CDDA:          return u"cd"_s;
    case Source::Device:        return u"device"_s;
    case Source::Stream:        return u"stream"_s;
    case Source::Subsonic:      return u"subsonic"_s;
    case Source::Tidal:         return u"tidal"_s;
    case Source::Spotify:       return u"spotify"_s;
    case Source::Qobuz:         return u"qobuz"_s;
    case Source::SomaFM:        return u"somafm"_s;
    case Source::RadioParadise: return u"radioparadise"_s;
    case Source::Unknown:       return u"unknown"_s;
  }
  return u"unknown"_s;

}

QString Song::DescriptionForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return u"File"_s;
    case Source::Collection:    return u"Collection"_s;
    case Source::CDDA:          return u"CD"_s;
    case Source::Device:        return u"Device"_s;
    case Source::Stream:        return u"Stream"_s;
    case Source::Subsonic:      return u"Subsonic"_s;
    case Source::Tidal:         return u"Tidal"_s;
    case Source::Spotify:       return u"Spotify"_s;
    case Source::Qobuz:         return u"Qobuz"_s;
    case Source::SomaFM:        return u"SomaFM"_s;
    case Source::RadioParadise: return u"Radio Paradise"_s;
    case Source::Unknown:       return u"Unknown"_s;
  }
  return u"unknown"_s;

}

Song::Source Song::SourceFromText(const QString &source) {

  if (source.compare("file"_L1, Qt::CaseInsensitive) == 0) return Source::LocalFile;
  if (source.compare("collection"_L1, Qt::CaseInsensitive) == 0) return Source::Collection;
  if (source.compare("cd"_L1, Qt::CaseInsensitive) == 0) return Source::CDDA;
  if (source.compare("device"_L1, Qt::CaseInsensitive) == 0) return Source::Device;
  if (source.compare("stream"_L1, Qt::CaseInsensitive) == 0) return Source::Stream;
  if (source.compare("subsonic"_L1, Qt::CaseInsensitive) == 0) return Source::Subsonic;
  if (source.compare("tidal"_L1, Qt::CaseInsensitive) == 0) return Source::Tidal;
  if (source.compare("spotify"_L1, Qt::CaseInsensitive) == 0) return Source::Spotify;
  if (source.compare("qobuz"_L1, Qt::CaseInsensitive) == 0) return Source::Qobuz;
  if (source.compare("somafm"_L1, Qt::CaseInsensitive) == 0) return Source::SomaFM;
  if (source.compare("radioparadise"_L1, Qt::CaseInsensitive) == 0) return Source::RadioParadise;

  return Source::Unknown;

}

QIcon Song::IconForSource(const Source source) {

  switch (source) {
    case Source::LocalFile:     return IconLoader::Load(u"folder-sound"_s);
    case Source::Collection:    return IconLoader::Load(u"library-music"_s);
    case Source::CDDA:          return IconLoader::Load(u"media-optical"_s);
    case Source::Device:        return IconLoader::Load(u"device"_s);
    case Source::Stream:        return IconLoader::Load(u"applications-internet"_s);
    case Source::Subsonic:      return IconLoader::Load(u"subsonic"_s);
    case Source::Tidal:         return IconLoader::Load(u"tidal"_s);
    case Source::Spotify:       return IconLoader::Load(u"spotify"_s);
    case Source::Qobuz:         return IconLoader::Load(u"qobuz"_s);
    case Source::SomaFM:        return IconLoader::Load(u"somafm"_s);
    case Source::RadioParadise: return IconLoader::Load(u"radioparadise"_s);
    case Source::Unknown:       return IconLoader::Load(u"edit-delete"_s);
  }
  return IconLoader::Load(u"edit-delete"_s);

}

// Convert a source to a music service domain name, for ListenBrainz.
// See the "Music service names" note on https://listenbrainz.readthedocs.io/en/latest/users/json.html.

QString Song::DomainForSource(const Source source) {

  switch (source) {
    case Song::Source::Tidal:         return u"tidal.com"_s;
    case Song::Source::Qobuz:         return u"qobuz.com"_s;
    case Song::Source::SomaFM:        return u"somafm.com"_s;
    case Song::Source::RadioParadise: return u"radioparadise.com"_s;
    case Song::Source::Spotify:       return u"spotify.com"_s;
    default: return QString();
  }

}

QString Song::TextForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return u"Wav"_s;
    case FileType::FLAC:        return u"FLAC"_s;
    case FileType::WavPack:     return u"WavPack"_s;
    case FileType::OggFlac:     return u"Ogg FLAC"_s;
    case FileType::OggVorbis:   return u"Ogg Vorbis"_s;
    case FileType::OggOpus:     return u"Ogg Opus"_s;
    case FileType::OggSpeex:    return u"Ogg Speex"_s;
    case FileType::MPEG:        return u"MPEG"_s;
    case FileType::MP4:         return u"MP4 AAC"_s;
    case FileType::ASF:         return u"Windows Media audio"_s;
    case FileType::AIFF:        return u"AIFF"_s;
    case FileType::MPC:         return u"MPC"_s;
    case FileType::TrueAudio:   return u"TrueAudio"_s;
    case FileType::DSF:         return u"DSF"_s;
    case FileType::DSDIFF:      return u"DSDIFF"_s;
    case FileType::PCM:         return u"PCM"_s;
    case FileType::APE:         return u"Monkey's Audio"_s;
    case FileType::MOD:
    case FileType::S3M:
    case FileType::XM:
    case FileType::IT:          return u"Module Music Format"_s;
    case FileType::CDDA:        return u"CDDA"_s;
    case FileType::SPC:         return u"SNES SPC700"_s;
    case FileType::VGM:         return u"VGM"_s;
    case FileType::ALAC:        return u"ALAC"_s;
    case FileType::Stream:      return u"Stream"_s;
    case FileType::Unknown:
    default:                    return QObject::tr("Unknown");
  }

}

QString Song::ExtensionForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return u"wav"_s;
    case FileType::FLAC:        return u"flac"_s;
    case FileType::WavPack:     return u"wv"_s;
    case FileType::OggFlac:     return u"flac"_s;
    case FileType::OggVorbis:   return u"ogg"_s;
    case FileType::OggOpus:     return u"opus"_s;
    case FileType::OggSpeex:    return u"spx"_s;
    case FileType::MPEG:        return u"mp3"_s;
    case FileType::MP4:         return u"mp4"_s;
    case FileType::ASF:         return u"wma"_s;
    case FileType::AIFF:        return u"aiff"_s;
    case FileType::MPC:         return u"mpc"_s;
    case FileType::TrueAudio:   return u"tta"_s;
    case FileType::DSF:         return u"dsf"_s;
    case FileType::DSDIFF:      return u"dsd"_s;
    case FileType::APE:         return u"ape"_s;
    case FileType::MOD:         return u"mod"_s;
    case FileType::S3M:         return u"s3m"_s;
    case FileType::XM:          return u"xm"_s;
    case FileType::IT:          return u"it"_s;
    case FileType::SPC:         return u"spc"_s;
    case FileType::VGM:         return u"vgm"_s;
    case FileType::ALAC:        return u"m4a"_s;
    case FileType::Unknown:
    default:                    return u"dat"_s;
  }

}

QIcon Song::IconForFiletype(const FileType filetype) {

  switch (filetype) {
    case FileType::WAV:         return IconLoader::Load(u"wav"_s);
    case FileType::FLAC:        return IconLoader::Load(u"flac"_s);
    case FileType::WavPack:     return IconLoader::Load(u"wavpack"_s);
    case FileType::OggFlac:     return IconLoader::Load(u"flac"_s);
    case FileType::OggVorbis:   return IconLoader::Load(u"vorbis"_s);
    case FileType::OggOpus:     return IconLoader::Load(u"opus"_s);
    case FileType::OggSpeex:    return IconLoader::Load(u"speex"_s);
    case FileType::MPEG:        return IconLoader::Load(u"mp3"_s);
    case FileType::MP4:         return IconLoader::Load(u"mp4"_s);
    case FileType::ASF:         return IconLoader::Load(u"wma"_s);
    case FileType::AIFF:        return IconLoader::Load(u"aiff"_s);
    case FileType::MPC:         return IconLoader::Load(u"mpc"_s);
    case FileType::TrueAudio:   return IconLoader::Load(u"trueaudio"_s);
    case FileType::DSF:         return IconLoader::Load(u"dsf"_s);
    case FileType::DSDIFF:      return IconLoader::Load(u"dsd"_s);
    case FileType::PCM:         return IconLoader::Load(u"pcm"_s);
    case FileType::APE:         return IconLoader::Load(u"ape"_s);
    case FileType::MOD:         return IconLoader::Load(u"mod"_s);
    case FileType::S3M:         return IconLoader::Load(u"s3m"_s);
    case FileType::XM:          return IconLoader::Load(u"xm"_s);
    case FileType::IT:          return IconLoader::Load(u"it"_s);
    case FileType::CDDA:        return IconLoader::Load(u"cd"_s);
    case FileType::Stream:      return IconLoader::Load(u"applications-internet"_s);
    case FileType::ALAC:        return IconLoader::Load(u"alac"_s);
    case FileType::Unknown:
    default:                    return IconLoader::Load(u"edit-delete"_s);
  }

}

// Get a URL usable for sharing this song with another user.
// This is only applicable when streaming from a streaming service, since we can't link to local content.
// Returns a web URL which points to the current streaming track or live stream, or an empty string if that is not applicable.

QString Song::ShareURL() const {

  switch (source()) {
    case Song::Source::Stream:
    case Song::Source::SomaFM:  return url().toString();
    case Song::Source::Tidal:   return "https://tidal.com/track/%1"_L1.arg(song_id());
    case Song::Source::Qobuz:   return "https://open.qobuz.com/track/%1"_L1.arg(song_id());
    case Song::Source::Spotify: return "https://open.spotify.com/track/%1"_L1.arg(song_id());
    default:                    return QString();
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
    case FileType::ALAC:
      return true;
    default:
      return false;
  }

}

Song::FileType Song::FiletypeByMimetype(const QString &mimetype) {

  if (mimetype.compare("audio/wav"_L1, Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-wav"_L1, Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (mimetype.compare("audio/x-flac"_L1, Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (mimetype.compare("audio/x-wavpack"_L1, Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (mimetype.compare("audio/x-vorbis"_L1, Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  if (mimetype.compare("audio/x-opus"_L1, Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (mimetype.compare("audio/x-speex"_L1, Qt::CaseInsensitive) == 0)  return FileType::OggSpeex;
  // Gstreamer returns audio/mpeg for both MP3 and MP4/AAC.
  // if (mimetype.compare("audio/mpeg", Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (mimetype.compare("audio/aac"_L1, Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (mimetype.compare("audio/x-wma"_L1, Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (mimetype.compare("audio/aiff"_L1, Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-aiff"_L1, Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (mimetype.compare("audio/x-musepack"_L1, Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (mimetype.compare("application/x-project"_L1, Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (mimetype.compare("audio/x-dsf"_L1, Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (mimetype.compare("audio/x-dsd"_L1, Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (mimetype.compare("audio/x-ape"_L1, Qt::CaseInsensitive) == 0 || mimetype.compare("application/x-ape"_L1, Qt::CaseInsensitive) == 0 || mimetype.compare("audio/x-ffmpeg-parsed-ape"_L1, Qt::CaseInsensitive) == 0) return FileType::APE;
  if (mimetype.compare("audio/x-mod"_L1, Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (mimetype.compare("audio/x-s3m"_L1, Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (mimetype.compare("audio/x-spc"_L1, Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (mimetype.compare("audio/x-vgm"_L1, Qt::CaseInsensitive) == 0) return FileType::VGM;
  if (mimetype.compare("audio/x-alac"_L1, Qt::CaseInsensitive) == 0) return FileType::ALAC;

  return FileType::Unknown;

}

Song::FileType Song::FiletypeByDescription(const QString &text) {

  if (text.compare("WAV"_L1, Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (text.compare("Free Lossless Audio Codec (FLAC)"_L1, Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (text.compare("Wavpack"_L1, Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (text.compare("Vorbis"_L1, Qt::CaseInsensitive) == 0) return FileType::OggVorbis;
  if (text.compare("Opus"_L1, Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (text.compare("Speex"_L1, Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  if (text.compare("MPEG-1 Layer 2 (MP2)"_L1, Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (text.compare("MPEG-1 Layer 3 (MP3)"_L1, Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (text.compare("MPEG-4 AAC"_L1, Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (text.compare("WMA"_L1, Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (text.compare("Audio Interchange File Format"_L1, Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (text.compare("MPC"_L1, Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (text.compare("Musepack (MPC)"_L1, Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (text.compare("audio/x-dsf"_L1, Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (text.compare("audio/x-dsd"_L1, Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (text.compare("audio/x-ffmpeg-parsed-ape"_L1, Qt::CaseInsensitive) == 0) return FileType::APE;
  if (text.compare("Module Music Format (MOD)"_L1, Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (text.compare("Module Music Format (MOD)"_L1, Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (text.compare("SNES SPC700"_L1, Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (text.compare("VGM"_L1, Qt::CaseInsensitive) == 0) return FileType::VGM;
  if (text.compare("Apple Lossless Audio Codec (ALAC)"_L1, Qt::CaseInsensitive) == 0) return FileType::ALAC;

  return FileType::Unknown;

}

Song::FileType Song::FiletypeByExtension(const QString &ext) {

  if (ext.compare("wav"_L1, Qt::CaseInsensitive) == 0 || ext.compare("wave"_L1, Qt::CaseInsensitive) == 0) return FileType::WAV;
  if (ext.compare("flac"_L1, Qt::CaseInsensitive) == 0) return FileType::FLAC;
  if (ext.compare("wavpack"_L1, Qt::CaseInsensitive) == 0 || ext.compare("wv"_L1, Qt::CaseInsensitive) == 0) return FileType::WavPack;
  if (ext.compare("opus"_L1, Qt::CaseInsensitive) == 0) return FileType::OggOpus;
  if (ext.compare("speex"_L1, Qt::CaseInsensitive) == 0 || ext.compare("spx"_L1, Qt::CaseInsensitive) == 0) return FileType::OggSpeex;
  if (ext.compare("mp2"_L1, Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (ext.compare("mp3"_L1, Qt::CaseInsensitive) == 0) return FileType::MPEG;
  if (ext.compare("mp4"_L1, Qt::CaseInsensitive) == 0 || ext.compare("m4a"_L1, Qt::CaseInsensitive) == 0 || ext.compare("aac"_L1, Qt::CaseInsensitive) == 0) return FileType::MP4;
  if (ext.compare("asf"_L1, Qt::CaseInsensitive) == 0 || ext.compare("wma"_L1, Qt::CaseInsensitive) == 0) return FileType::ASF;
  if (ext.compare("aiff"_L1, Qt::CaseInsensitive) == 0 || ext.compare("aif"_L1, Qt::CaseInsensitive) == 0 || ext.compare("aifc"_L1, Qt::CaseInsensitive) == 0) return FileType::AIFF;
  if (ext.compare("mpc"_L1, Qt::CaseInsensitive) == 0 || ext.compare("mp+"_L1, Qt::CaseInsensitive) == 0 || ext.compare("mpp"_L1, Qt::CaseInsensitive) == 0) return FileType::MPC;
  if (ext.compare("dsf"_L1, Qt::CaseInsensitive) == 0) return FileType::DSF;
  if (ext.compare("dsd"_L1, Qt::CaseInsensitive) == 0 || ext.compare("dff"_L1, Qt::CaseInsensitive) == 0) return FileType::DSDIFF;
  if (ext.compare("ape"_L1, Qt::CaseInsensitive) == 0) return FileType::APE;
  if (ext.compare("mod"_L1, Qt::CaseInsensitive) == 0 ||
      ext.compare("module"_L1, Qt::CaseInsensitive) == 0 ||
      ext.compare("nst"_L1, Qt::CaseInsensitive) == 0||
      ext.compare("wow"_L1, Qt::CaseInsensitive) == 0) return FileType::MOD;
  if (ext.compare("s3m"_L1, Qt::CaseInsensitive) == 0) return FileType::S3M;
  if (ext.compare("xm"_L1, Qt::CaseInsensitive) == 0) return FileType::XM;
  if (ext.compare("it"_L1, Qt::CaseInsensitive) == 0) return FileType::IT;
  if (ext.compare("spc"_L1, Qt::CaseInsensitive) == 0) return FileType::SPC;
  if (ext.compare("vgm"_L1, Qt::CaseInsensitive) == 0) return FileType::VGM;

  return FileType::Unknown;

}

bool Song::IsLinkedCollectionSource(const Source source) {

  return source == Source::Collection;

}

QString Song::ImageCacheDir(const Source source) {

  switch (source) {
    case Source::Collection:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/collectionalbumcovers"_s;
    case Source::Subsonic:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/subsonicalbumcovers"_s;
    case Source::Tidal:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/tidalalbumcovers"_s;
    case Source::Spotify:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/spotifyalbumcovers"_s;
    case Source::Qobuz:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/qobuzalbumcovers"_s;
    case Source::Device:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/devicealbumcovers"_s;
    case Source::LocalFile:
    case Source::CDDA:
    case Source::Stream:
    case Source::SomaFM:
    case Source::RadioParadise:
    case Source::Unknown:
      return StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/albumcovers"_s;
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

void Song::InitFromQuery(const QSqlRecord &r, const bool reliable_metadata, const int col) {

  Q_ASSERT(kRowIdColumns.count() + col <= r.count());

  d->id_ = SqlHelper::ValueToInt(r, ColumnIndex(u"ROWID"_s) + col);

  set_title(SqlHelper::ValueToString(r, ColumnIndex(u"title"_s) + col));
  set_titlesort(SqlHelper::ValueToString(r, ColumnIndex(u"titlesort"_s) + col));
  set_album(SqlHelper::ValueToString(r, ColumnIndex(u"album"_s) + col));
  set_albumsort(SqlHelper::ValueToString(r, ColumnIndex(u"albumsort"_s) + col));
  set_artist(SqlHelper::ValueToString(r, ColumnIndex(u"artist"_s) + col));
  set_artistsort(SqlHelper::ValueToString(r, ColumnIndex(u"artistsort"_s) + col));
  set_albumartist(SqlHelper::ValueToString(r, ColumnIndex(u"albumartist"_s) + col));
  set_albumartistsort(SqlHelper::ValueToString(r, ColumnIndex(u"albumartistsort"_s) + col));
  d->track_ = SqlHelper::ValueToInt(r, ColumnIndex(u"track"_s) + col);
  d->disc_ = SqlHelper::ValueToInt(r, ColumnIndex(u"disc"_s) + col);
  d->year_ = SqlHelper::ValueToInt(r, ColumnIndex(u"year"_s) + col);
  d->originalyear_ = SqlHelper::ValueToInt(r, ColumnIndex(u"originalyear"_s) + col);
  d->genre_ = SqlHelper::ValueToString(r, ColumnIndex(u"genre"_s) + col);
  d->compilation_ = r.value(ColumnIndex(u"compilation"_s) + col).toBool();
  d->composer_ = SqlHelper::ValueToString(r, ColumnIndex(u"composer"_s) + col);
  d->composersort_ = SqlHelper::ValueToString(r, ColumnIndex(u"composersort"_s) + col);
  d->performer_ = SqlHelper::ValueToString(r, ColumnIndex(u"performer"_s) + col);
  d->performersort_ = SqlHelper::ValueToString(r, ColumnIndex(u"performersort"_s) + col);
  d->grouping_ = SqlHelper::ValueToString(r, ColumnIndex(u"grouping"_s) + col);
  d->comment_ = SqlHelper::ValueToString(r, ColumnIndex(u"comment"_s) + col);
  d->lyrics_ = SqlHelper::ValueToString(r, ColumnIndex(u"lyrics"_s) + col);
  d->artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"artist_id"_s) + col);
  d->album_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"album_id"_s) + col);
  d->song_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"song_id"_s) + col);
  d->beginning_ = r.value(ColumnIndex(u"beginning"_s) + col).isNull() ? 0 : r.value(ColumnIndex(u"beginning"_s) + col).toLongLong();
  set_length_nanosec(SqlHelper::ValueToLongLong(r, ColumnIndex(u"length"_s) + col));
  d->bitrate_ = SqlHelper::ValueToInt(r, ColumnIndex(u"bitrate"_s) + col);
  d->samplerate_ = SqlHelper::ValueToInt(r, ColumnIndex(u"samplerate"_s) + col);
  d->bitdepth_ = SqlHelper::ValueToInt(r, ColumnIndex(u"bitdepth"_s) + col);
  if (!r.value(ColumnIndex(u"ebur128_integrated_loudness_lufs"_s) + col).isNull()) {
    d->ebur128_integrated_loudness_lufs_ = r.value(ColumnIndex(u"ebur128_integrated_loudness_lufs"_s) + col).toDouble();
  }
  if (!r.value(ColumnIndex(u"ebur128_loudness_range_lu"_s) + col).isNull()) {
    d->ebur128_loudness_range_lu_ = r.value(ColumnIndex(u"ebur128_loudness_range_lu"_s) + col).toDouble();
  }
  d->source_ = static_cast<Source>(r.value(ColumnIndex(u"source"_s) + col).isNull() ? 0 : r.value(ColumnIndex(u"source"_s) + col).toInt());
  d->directory_id_ = SqlHelper::ValueToInt(r, ColumnIndex(u"directory_id"_s) + col);
  set_url(QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(u"url"_s) + col).toUtf8()));
  d->basefilename_ = QFileInfo(d->url_.toLocalFile()).fileName();
  d->filetype_ = FileType(r.value(ColumnIndex(u"filetype"_s) + col).isNull() ? 0 : r.value(ColumnIndex(u"filetype"_s) + col).toInt());
  d->filesize_ = SqlHelper::ValueToLongLong(r, ColumnIndex(u"filesize"_s) + col);
  d->mtime_ = SqlHelper::ValueToLongLong(r, ColumnIndex(u"mtime"_s) + col);
  d->ctime_ = SqlHelper::ValueToLongLong(r, ColumnIndex(u"ctime"_s) + col);
  d->unavailable_ = r.value(ColumnIndex(u"unavailable"_s) + col).toBool();
  d->fingerprint_ = SqlHelper::ValueToString(r, ColumnIndex(u"fingerprint"_s) + col);
  d->playcount_ = SqlHelper::ValueToUInt(r, ColumnIndex(u"playcount"_s) + col);
  d->skipcount_ = SqlHelper::ValueToUInt(r, ColumnIndex(u"skipcount"_s) + col);
  d->lastplayed_ = SqlHelper::ValueToLongLong(r, ColumnIndex(u"lastplayed"_s) + col);
  d->lastseen_ = SqlHelper::ValueToLongLong(r, ColumnIndex(u"lastseen"_s) + col);
  d->compilation_detected_ = SqlHelper::ValueToBool(r, ColumnIndex(u"compilation_detected"_s) + col);
  d->compilation_on_ = SqlHelper::ValueToBool(r, ColumnIndex(u"compilation_on"_s) + col);
  d->compilation_off_ = SqlHelper::ValueToBool(r, ColumnIndex(u"compilation_off"_s) + col);

  d->art_embedded_ = SqlHelper::ValueToBool(r, ColumnIndex(u"art_embedded"_s) + col);
  d->art_automatic_ = QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(u"art_automatic"_s) + col).toUtf8());
  d->art_manual_ = QUrl::fromEncoded(SqlHelper::ValueToString(r, ColumnIndex(u"art_manual"_s) + col).toUtf8());
  d->art_unset_ = SqlHelper::ValueToBool(r, ColumnIndex(u"art_unset"_s) + col);

  d->cue_path_ = SqlHelper::ValueToString(r, ColumnIndex(u"cue_path"_s) + col);

  d->rating_ = SqlHelper::ValueToFloat(r, ColumnIndex(u"rating"_s) + col);
  d->bpm_ = SqlHelper::ValueToFloat(r, ColumnIndex(u"bpm"_s) + col);
  d->mood_ = SqlHelper::ValueToString(r, ColumnIndex(u"mood"_s) + col);
  d->initial_key_ = SqlHelper::ValueToString(r, ColumnIndex(u"initial_key"_s) + col);

  d->acoustid_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"acoustid_id"_s) + col);
  d->acoustid_fingerprint_ = SqlHelper::ValueToString(r, ColumnIndex(u"acoustid_fingerprint"_s) + col);

  d->musicbrainz_album_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_album_artist_id"_s) + col);
  d->musicbrainz_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_artist_id"_s) + col);
  d->musicbrainz_original_artist_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_original_artist_id"_s) + col);
  d->musicbrainz_album_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_album_id"_s) + col);
  d->musicbrainz_original_album_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_original_album_id"_s) + col);
  d->musicbrainz_recording_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_recording_id"_s) + col);
  d->musicbrainz_track_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_track_id"_s) + col);
  d->musicbrainz_disc_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_disc_id"_s) + col);
  d->musicbrainz_release_group_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_release_group_id"_s) + col);
  d->musicbrainz_work_id_ = SqlHelper::ValueToString(r, ColumnIndex(u"musicbrainz_work_id"_s) + col);

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
    QString filename = QString::fromLatin1(CoverUtils::Sha1CoverHash(effective_albumartist(), effective_album()).toHex()) + u".jpg"_s;
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
    QStringList files = dir.entryList(QStringList() << u"*.jpg"_s << u"*.png"_s << u"*.gif"_s << u"*.jpeg"_s, QDir::Files|QDir::Readable, QDir::Name);
    if (files.count() > 0) {
      d->art_automatic_ = QUrl::fromLocalFile(file.path() + QDir::separator() + files.first());
    }
  }

}

#ifdef HAVE_GPOD
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
  filename.replace(u':', u'/');
  if (prefix.contains("://"_L1)) {
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
      QString cover_file = cover_path + QLatin1Char('/') + QString::fromLatin1(CoverUtils::Sha1CoverHash(effective_albumartist(), effective_album()).toHex()) + u".jpg"_s;
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

#ifdef HAVE_MTP
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

  query->BindStringValue(u":title"_s, d->title_);
  query->BindStringValue(u":titlesort"_s, d->titlesort_);
  query->BindStringValue(u":album"_s, d->album_);
  query->BindStringValue(u":albumsort"_s, d->albumsort_);
  query->BindStringValue(u":artist"_s, d->artist_);
  query->BindStringValue(u":artistsort"_s, d->artistsort_);
  query->BindStringValue(u":albumartist"_s, d->albumartist_);
  query->BindStringValue(u":albumartistsort"_s, d->albumartistsort_);
  query->BindIntValue(u":track"_s, d->track_);
  query->BindIntValue(u":disc"_s, d->disc_);
  query->BindIntValue(u":year"_s, d->year_);
  query->BindIntValue(u":originalyear"_s, d->originalyear_);
  query->BindStringValue(u":genre"_s, d->genre_);
  query->BindBoolValue(u":compilation"_s, d->compilation_);
  query->BindStringValue(u":composer"_s, d->composer_);
  query->BindStringValue(u":composersort"_s, d->composersort_);
  query->BindStringValue(u":performer"_s, d->performer_);
  query->BindStringValue(u":performersort"_s, d->performersort_);
  query->BindStringValue(u":grouping"_s, d->grouping_);
  query->BindStringValue(u":comment"_s, d->comment_);
  query->BindStringValue(u":lyrics"_s, d->lyrics_);

  query->BindStringValue(u":artist_id"_s, d->artist_id_);
  query->BindStringValue(u":album_id"_s, d->album_id_);
  query->BindStringValue(u":song_id"_s, d->song_id_);

  query->BindValue(u":beginning"_s, d->beginning_);
  query->BindLongLongValue(u":length"_s, length_nanosec());

  query->BindIntValue(u":bitrate"_s, d->bitrate_);
  query->BindIntValue(u":samplerate"_s, d->samplerate_);
  query->BindIntValue(u":bitdepth"_s, d->bitdepth_);

  query->BindValue(u":source"_s, static_cast<int>(d->source_));
  query->BindNotNullIntValue(u":directory_id"_s, d->directory_id_);
  query->BindUrlValue(u":url"_s, d->url_);
  query->BindValue(u":filetype"_s, static_cast<int>(d->filetype_));
  query->BindLongLongValueOrZero(u":filesize"_s, d->filesize_);
  query->BindLongLongValueOrZero(u":mtime"_s, d->mtime_);
  query->BindLongLongValueOrZero(u":ctime"_s, d->ctime_);
  query->BindBoolValue(u":unavailable"_s, d->unavailable_);

  query->BindStringValue(u":fingerprint"_s, d->fingerprint_);

  query->BindValue(u":playcount"_s, d->playcount_);
  query->BindValue(u":skipcount"_s, d->skipcount_);
  query->BindLongLongValue(u":lastplayed"_s, d->lastplayed_);
  query->BindLongLongValue(u":lastseen"_s, d->lastseen_);

  query->BindBoolValue(u":compilation_detected"_s, d->compilation_detected_);
  query->BindBoolValue(u":compilation_on"_s, d->compilation_on_);
  query->BindBoolValue(u":compilation_off"_s, d->compilation_off_);
  query->BindBoolValue(u":compilation_effective"_s, is_compilation());

  query->BindBoolValue(u":art_embedded"_s, d->art_embedded_);
  query->BindUrlValue(u":art_automatic"_s, d->art_automatic_);
  query->BindUrlValue(u":art_manual"_s, d->art_manual_);
  query->BindBoolValue(u":art_unset"_s, d->art_unset_);

  query->BindStringValue(u":effective_albumartist"_s, effective_albumartist());
  query->BindIntValue(u":effective_originalyear"_s, effective_originalyear());

  query->BindValue(u":cue_path"_s, d->cue_path_);

  query->BindFloatValue(u":rating"_s, d->rating_);
  query->BindFloatValue(u":bpm"_s, d->bpm_);
  query->BindStringValue(u":mood"_s, d->mood_);
  query->BindStringValue(u":initial_key"_s, d->initial_key_);

  query->BindStringValue(u":acoustid_id"_s, d->acoustid_id_);
  query->BindStringValue(u":acoustid_fingerprint"_s, d->acoustid_fingerprint_);

  query->BindStringValue(u":musicbrainz_album_artist_id"_s, d->musicbrainz_album_artist_id_);
  query->BindStringValue(u":musicbrainz_artist_id"_s, d->musicbrainz_artist_id_);
  query->BindStringValue(u":musicbrainz_original_artist_id"_s, d->musicbrainz_original_artist_id_);
  query->BindStringValue(u":musicbrainz_album_id"_s, d->musicbrainz_album_id_);
  query->BindStringValue(u":musicbrainz_original_album_id"_s, d->musicbrainz_original_album_id_);
  query->BindStringValue(u":musicbrainz_recording_id"_s, d->musicbrainz_recording_id_);
  query->BindStringValue(u":musicbrainz_track_id"_s, d->musicbrainz_track_id_);
  query->BindStringValue(u":musicbrainz_disc_id"_s, d->musicbrainz_disc_id_);
  query->BindStringValue(u":musicbrainz_release_group_id"_s, d->musicbrainz_release_group_id_);
  query->BindStringValue(u":musicbrainz_work_id"_s, d->musicbrainz_work_id_);

  query->BindDoubleOrNullValue(u":ebur128_integrated_loudness_lufs"_s, d->ebur128_integrated_loudness_lufs_);
  query->BindDoubleOrNullValue(u":ebur128_loudness_range_lu"_s, d->ebur128_loudness_range_lu_);

}

#ifdef HAVE_MPRIS2
void Song::ToXesam(QVariantMap *map) const {

  using mpris::AddMetadata;
  using mpris::AddMetadataAsList;
  using mpris::AsMPRISDateTimeType;

  AddMetadata(u"xesam:url"_s, effective_url().toString(), map);
  AddMetadata(u"xesam:title"_s, PrettyTitle(), map);
  AddMetadataAsList(u"xesam:artist"_s, artist(), map);
  AddMetadata(u"xesam:album"_s, album(), map);
  AddMetadataAsList(u"xesam:albumArtist"_s, albumartist(), map);
  AddMetadata(u"mpris:length"_s, (length_nanosec() / kNsecPerUsec), map);
  AddMetadata(u"xesam:trackNumber"_s, track(), map);
  AddMetadataAsList(u"xesam:genre"_s, genre(), map);
  AddMetadata(u"xesam:discNumber"_s, disc(), map);
  AddMetadataAsList(u"xesam:comment"_s, comment(), map);
  AddMetadata(u"xesam:contentCreated"_s, AsMPRISDateTimeType(ctime()), map);
  AddMetadata(u"xesam:lastUsed"_s, AsMPRISDateTimeType(lastplayed()), map);
  AddMetadataAsList(u"xesam:composer"_s, composer(), map);
  AddMetadata(u"xesam:useCount"_s, static_cast<int>(playcount()), map);

  if (rating() != -1.0) {
    AddMetadata(u"xesam:userRating"_s, rating(), map);
  }

}
#endif

bool Song::MergeFromEngineMetadata(const EngineMetadata &engine_metadata) {

  d->valid_ = true;

  bool minor = true;

  if (d->init_from_file_ || is_local_collection_song() || d->url_.isLocalFile()) {
    // This Song was already loaded using TagLib. Our tags are probably better than the engine's.
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
  return QStringLiteral("%1|%2|%3").arg(is_compilation() ? u"_compilation"_s : effective_albumartist(), has_cue() ? cue_path() : ""_L1, effective_album());
}

size_t qHash(const Song &song) {
  // Should compare the same fields as operator==
  return qHash(song.url().toString()) ^ qHash(song.beginning_nanosec());
}

size_t HashSimilar(const Song &song) {
  // Should compare the same fields as function IsSimilar
  return qHash(song.title().toLower()) ^ qHash(song.artist().toLower()) ^ qHash(song.album().toLower()) ^ qHash(song.fingerprint()) ^ qHash(song.acoustid_fingerprint());
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

QString Song::GetNameForNewPlaylist(const SongList &songs) {

  if (songs.isEmpty()) {
    return QObject::tr("Playlist");
  }

  QSet<QString> artists;
  QSet<QString> albums;
  artists.reserve(songs.count());
  albums.reserve(songs.count());
  for (const Song &song : songs) {
    artists << (song.effective_albumartist().isEmpty() ? QObject::tr("Unknown") : song.effective_albumartist());
    albums << (song.album().isEmpty() ? QObject::tr("Unknown") : song.album());

    if (artists.size() > 1) {
      break;
    }
  }

  bool various_artists = artists.size() > 1;

  QString result;
  if (various_artists) {
    result = QObject::tr("Various artists");
  }
  else {
    QStringList artist_names = artists.values();
    result = artist_names.first();
  }

  if (!various_artists && albums.size() == 1) {
    QStringList album_names = albums.values();
    result += " - "_L1 + album_names.first();
  }

  return result;

}

