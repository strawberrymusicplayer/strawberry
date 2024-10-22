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
#include <QString>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "constants/timeconstants.h"
#include "engine/chromaprinter.h"
#include "acoustidclient.h"
#include "musicbrainzclient.h"
#include "tagfetcher.h"

TagFetcher::TagFetcher(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      fingerprint_watcher_(nullptr),
      acoustid_client_(new AcoustidClient(network, this)),
      musicbrainz_client_(new MusicBrainzClient(network, this)) {

  QObject::connect(acoustid_client_, &AcoustidClient::Finished, this, &TagFetcher::PuidsFound);
  QObject::connect(musicbrainz_client_, &MusicBrainzClient::Finished, this, &TagFetcher::TagsFetched);

}

QString TagFetcher::GetFingerprint(const Song &song) {
  return Chromaprinter(song.url().toLocalFile()).CreateFingerprint();
}

void TagFetcher::StartFetch(const SongList &songs) {

  Cancel();

  songs_ = songs;

  bool have_fingerprints = true;
  if (std::any_of(songs.begin(), songs.end(), [](const Song &song) { return song.fingerprint().isEmpty(); })) {
    have_fingerprints = false;
  }

  if (have_fingerprints) {
    for (int i = 0; i < songs_.count(); ++i) {
      const Song song = songs_.value(i);
      Q_EMIT Progress(song, tr("Identifying song"));
      acoustid_client_->Start(i, song.fingerprint(), static_cast<int>(song.length_nanosec() / kNsecPerMsec));
    }
  }
  else {
    QFuture<QString> future = QtConcurrent::mapped(songs_, GetFingerprint);
    fingerprint_watcher_ = new QFutureWatcher<QString>(this);
    QObject::connect(fingerprint_watcher_, &QFutureWatcher<QString>::resultReadyAt, this, &TagFetcher::FingerprintFound);
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

  QFutureWatcher<QString> *watcher = reinterpret_cast<QFutureWatcher<QString>*>(sender());
  if (!watcher || index >= songs_.count()) return;

  const QString fingerprint = watcher->resultAt(index);
  const Song song = songs_.value(index);

  if (fingerprint.isEmpty()) {
    Q_EMIT ResultAvailable(song, SongList());
    return;
  }

  Q_EMIT Progress(song, tr("Identifying song"));
  acoustid_client_->Start(index, fingerprint, static_cast<int>(song.length_nanosec() / kNsecPerMsec));

}

void TagFetcher::PuidsFound(const int index, const QStringList &puid_list, const QString &error) {

  if (index >= songs_.count()) {
    return;
  }

  const Song song = songs_.value(index);

  if (puid_list.isEmpty()) {
    Q_EMIT ResultAvailable(song, SongList(), error);
    return;
  }

  Q_EMIT Progress(song, tr("Downloading metadata"));
  musicbrainz_client_->Start(index, puid_list);

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
    song.set_track(result.track_);
    song.set_year(result.year_);
    songs_guessed << song;
  }

  Q_EMIT ResultAvailable(original_song, songs_guessed, error);

}
