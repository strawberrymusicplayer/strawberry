/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QObject>
#include <QtConcurrentMap>
#include <QFuture>
#include <QFutureWatcher>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QRegularExpression>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "constants/timeconstants.h"
#include "engine/chromaprinter.h"
#include "acoustidclient.h"
#include "musicbrainzclient.h"
#include "tagfetcher.h"

namespace {
constexpr int kMinimumAcoustidFingerprintLength = 16;
using FingerprintResult = QPair<QString, QString>;

bool HasUsableFingerprint(const QString &fingerprint) {
  const QString normalized = fingerprint.trimmed();
  return !normalized.isEmpty() && normalized.compare(QStringLiteral("NONE"), Qt::CaseInsensitive) != 0 && normalized.size() >= kMinimumAcoustidFingerprintLength;
}

QString BuildUiErrorDetails(const QString &stage, const QString &reason, const QStringList &extra = QStringList()) {
  QStringList lines;
  lines << QStringLiteral("Stage: %1").arg(stage.trimmed());
  lines << QStringLiteral("Reason: %1").arg(reason.trimmed());
  for (const QString &line : extra) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      lines << trimmed;
    }
  }
  return lines.join(QLatin1Char('\n'));
}

void FillMissingSearchMetadataFromFilename(const Song &song, QString *title, QString *artist) {

  static const QRegularExpression kLeadingTrackNumberRegex(QStringLiteral("^\\d+\\s*[-–—.:]\\s*"));
  static const QRegularExpression kFilenameSeparatorRegex(QStringLiteral("\\s+-\\s+"));

  if (!title || !artist) return;
  if (!title->trimmed().isEmpty() && !artist->trimmed().isEmpty()) return;

  // When embedded tags are empty (common for yt-dlp files), derive search metadata from filename.
  QString base_name = QFileInfo(song.url().toLocalFile()).completeBaseName().trimmed();
  if (base_name.isEmpty()) {
    const QString fallback_name = song.basefilename().trimmed();
    base_name = QFileInfo(fallback_name).completeBaseName().trimmed();
    if (base_name.isEmpty()) {
      base_name = fallback_name;
    }
  }
  if (base_name.isEmpty()) return;

  base_name.replace(u'_', u' ');
  base_name = base_name.simplified();
  base_name.remove(kLeadingTrackNumberRegex);
  base_name = base_name.simplified();
  if (base_name.isEmpty()) return;

  const QStringList parts = base_name.split(kFilenameSeparatorRegex, Qt::SkipEmptyParts);
  if (parts.size() >= 2) {
    if (artist->trimmed().isEmpty()) {
      *artist = parts.first().trimmed();
    }
    if (title->trimmed().isEmpty()) {
      *title = parts.mid(1).join(QStringLiteral(" - ")).trimmed();
    }
  }
  else if (title->trimmed().isEmpty()) {
    *title = base_name.trimmed();
  }

}
}  // namespace

TagFetcher::TagFetcher(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      fingerprint_watcher_(nullptr),
      acoustid_client_(new AcoustidClient(network, this)),
      musicbrainz_client_(new MusicBrainzClient(network, this)) {

  QObject::connect(acoustid_client_, &AcoustidClient::Finished, this, &TagFetcher::PuidsFound);
  QObject::connect(musicbrainz_client_, &MusicBrainzClient::MbIdFinished, this, &TagFetcher::TagsFetched);

}

QPair<QString, QString> TagFetcher::GetFingerprint(const Song &song) {
  Chromaprinter chromaprinter(song.url().toLocalFile());
  const QString fingerprint = chromaprinter.CreateFingerprint();
  const QString fingerprint_error = chromaprinter.LastError().trimmed();
  if (fingerprint.isEmpty()) {
    qLog(Warning) << "Tag fetch fingerprint generation failed for" << song.url().toLocalFile() << ":" << fingerprint_error;
  }
  else {
    qLog(Debug) << "Tag fetch fingerprint generated for" << song.url().toLocalFile() << "length" << fingerprint.size();
  }
  return FingerprintResult(fingerprint, fingerprint_error);
}

void TagFetcher::StartFetch(const SongList &songs) {

  Cancel();

  songs_ = songs;

  bool have_fingerprints = true;
  if (std::any_of(songs.begin(), songs.end(), [](const Song &song) { return !HasUsableFingerprint(song.fingerprint()); })) {
    have_fingerprints = false;
  }

  if (have_fingerprints) {
    for (int i = 0; i < songs_.count(); ++i) {
      const Song song = songs_.value(i);
      Q_EMIT Progress(song, tr("Identifying song"));
      const QString fingerprint = song.fingerprint().trimmed();
      qLog(Debug) << "Tag fetch using cached fingerprint for" << song.url().toLocalFile() << "length" << fingerprint.size();
      acoustid_client_->Start(i, fingerprint, static_cast<int>(song.length_nanosec() / kNsecPerMsec));
    }
  }
  else {
    QFuture<FingerprintResult> future = QtConcurrent::mapped(songs_, GetFingerprint);
    fingerprint_watcher_ = new QFutureWatcher<FingerprintResult>(this);
    QObject::connect(fingerprint_watcher_, &QFutureWatcher<FingerprintResult>::resultReadyAt, this, &TagFetcher::FingerprintFound);
    fingerprint_watcher_->setFuture(future);
    for (const Song &song : std::as_const(songs_)) {
      Q_EMIT Progress(song, tr("Fingerprinting song"));
    }
  }

}

void TagFetcher::Cancel() {

  if (fingerprint_watcher_) {
    fingerprint_watcher_->cancel();

    fingerprint_watcher_->deleteLater();
    fingerprint_watcher_ = nullptr;
  }

  acoustid_client_->CancelAll();
  musicbrainz_client_->CancelAll();
  songs_.clear();

}

void TagFetcher::FingerprintFound(const int index) {

  QFutureWatcher<FingerprintResult> *watcher = static_cast<QFutureWatcher<FingerprintResult>*>(sender());
  if (!watcher || index >= songs_.count()) return;

  const FingerprintResult fingerprint_result = watcher->resultAt(index);
  const QString fingerprint = fingerprint_result.first.trimmed();
  const QString fingerprint_error = fingerprint_result.second.trimmed();
  const Song song = songs_.value(index);

  if (!HasUsableFingerprint(fingerprint)) {
    QString reason = fingerprint_error;
    if (reason.isEmpty()) {
      reason = tr("Generated fingerprint is empty or invalid.");
    }
    const QString result_error = BuildUiErrorDetails(
        tr("Fingerprinting"),
        reason,
        QStringList()
            << QStringLiteral("Fingerprint length: %1").arg(fingerprint.size())
            << QStringLiteral("Minimum required length: %1").arg(kMinimumAcoustidFingerprintLength));
    qLog(Error) << "Tag fetch fingerprint invalid for" << song.url().toLocalFile() << ":" << result_error;
    Q_EMIT ResultAvailable(song, SongList(), result_error);
    return;
  }

  Q_EMIT Progress(song, tr("Identifying song"));
  qLog(Debug) << "Tag fetch sending generated fingerprint for" << song.url().toLocalFile() << "length" << fingerprint.size();
  acoustid_client_->Start(index, fingerprint, static_cast<int>(song.length_nanosec() / kNsecPerMsec));

}

void TagFetcher::PuidsFound(const int index, const QStringList &puid_list, const QString &error) {

  if (index >= songs_.count()) {
    return;
  }

  const Song song = songs_.value(index);

  if (puid_list.isEmpty()) {
    // Fall back to MusicBrainz textual search when AcoustID has no usable match.
    if (error.isEmpty()) {
      qLog(Warning) << "Tag fetch AcoustID returned no matches for" << song.url().toLocalFile() << "- falling back to MusicBrainz metadata search";
    }
    else {
      qLog(Warning) << "Tag fetch AcoustID lookup failed for" << song.url().toLocalFile() << ":" << error << "- falling back to MusicBrainz metadata search";
    }
    Q_EMIT Progress(song, tr("Searching MusicBrainz"));
    QString search_title = song.title().trimmed();
    QString search_artist = song.artist().trimmed();
    FillMissingSearchMetadataFromFilename(song, &search_title, &search_artist);
    qLog(Debug) << "Tag fetch MusicBrainz fallback search metadata for" << song.url().toLocalFile() << "title:" << search_title << "artist:" << search_artist << "album:" << song.album().trimmed();
    musicbrainz_client_->StartSearchRequest(
        index,
        search_title,
        search_artist,
        song.album().trimmed(),
        static_cast<int>(song.length_nanosec() / kNsecPerMsec));
    return;
  }

  Q_EMIT Progress(song, tr("Downloading metadata"));
  musicbrainz_client_->StartMbIdRequest(index, puid_list);

}

void TagFetcher::TagsFetched(const int index, const MusicBrainzClient::ResultList &results, const QString &error) {

  if (index >= songs_.count()) {
    return;
  }

  const Song original_song = songs_.value(index);
  SongList songs_guessed;
  songs_guessed.reserve(results.count());
  for (const MusicBrainzClient::Result &result : results) {
    Song song;
    song.Init(result.title_, result.artist_, result.album_, result.duration_msec_ * kNsecPerMsec);
    song.set_artistsort(result.sort_artist_);
    song.set_track(result.track_);
    song.set_year(result.year_);
    if (!result.album_artist_.isEmpty() && result.album_artist_ != result.artist_) {
      song.set_albumartist(result.album_artist_);
    }
    if (!result.sort_album_artist_.isEmpty() && result.sort_album_artist_ != result.sort_artist_) {
      song.set_albumartistsort(result.sort_album_artist_);
    }
    songs_guessed << song;
  }

  QString result_error = error;
  if (songs_guessed.isEmpty()) {
    QString reason = result_error;
    if (reason.isEmpty()) {
      reason = tr("No MusicBrainz metadata was found for this track.");
    }
    result_error = BuildUiErrorDetails(
        tr("MusicBrainz metadata"),
        reason,
        QStringList() << QStringLiteral("Candidate metadata rows: 0"));
  }
  if (!songs_guessed.isEmpty()) {
    qLog(Debug) << "Tag fetch resolved" << songs_guessed.count() << "candidate(s) for" << original_song.url().toLocalFile();
  }
  else {
    qLog(Warning) << "Tag fetch no MusicBrainz candidates for" << original_song.url().toLocalFile() << ":" << result_error;
  }
  Q_EMIT ResultAvailable(original_song, songs_guessed, result_error);

}
