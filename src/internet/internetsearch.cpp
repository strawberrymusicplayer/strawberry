/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QTimerEvent>
#include <QSettings>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloader.h"
#include "internet/internetsongmimedata.h"
#include "internetsearch.h"
#include "internetservice.h"
#include "internetservices.h"

const int InternetSearch::kDelayedSearchTimeoutMs = 200;
const int InternetSearch::kArtHeight = 32;

InternetSearch::InternetSearch(Application *app, Song::Source source, QObject *parent)
    : QObject(parent),
      app_(app),
      source_(source),
      service_(app->internet_services()->ServiceBySource(source)),
      searches_next_id_(1),
      art_searches_next_id_(1) {

  cover_loader_options_.desired_height_ = kArtHeight;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;

  connect(app_->album_cover_loader(), SIGNAL(ImageLoaded(quint64, QImage)), SLOT(AlbumArtLoaded(const quint64, const QImage&)));
  connect(this, SIGNAL(SearchAsyncSig(const int, const QString&, const SearchType)), this, SLOT(DoSearchAsync(const int, const QString&, const SearchType)));

  connect(service_, SIGNAL(SearchUpdateStatus(const int, const QString&)), SLOT(UpdateStatusSlot(const int, const QString&)));
  connect(service_, SIGNAL(SearchProgressSetMaximum(const int, const int)), SLOT(ProgressSetMaximumSlot(const int, const int)));
  connect(service_, SIGNAL(SearchUpdateProgress(const int, const int)), SLOT(UpdateProgressSlot(const int, const int)));
  connect(service_, SIGNAL(SearchResults(const int, const SongList&, const QString&)), SLOT(SearchDone(const int, const SongList&, const QString&)));

}

InternetSearch::~InternetSearch() {}

QStringList InternetSearch::TokenizeQuery(const QString &query) {

  QStringList tokens(query.split(QRegExp("\\s+")));

  for (QStringList::iterator it = tokens.begin(); it != tokens.end(); ++it) {
    (*it).remove('(');
    (*it).remove(')');
    (*it).remove('"');

    const int colon = (*it).indexOf(":");
    if (colon != -1) {
      (*it).remove(0, colon + 1);
    }
  }

  return tokens;

}

bool InternetSearch::Matches(const QStringList &tokens, const QString &string) {

  for (const QString &token : tokens) {
    if (!string.contains(token, Qt::CaseInsensitive)) {
      return false;
    }
  }

  return true;

}

int InternetSearch::SearchAsync(const QString &query, const SearchType type) {

  const int id = searches_next_id_++;

  emit SearchAsyncSig(id, query, type);

  return id;

}

void InternetSearch::SearchAsync(const int id, const QString &query, const SearchType type) {

  const int service_id = service_->Search(query, type);
  pending_searches_[service_id] = PendingState(id, TokenizeQuery(query));

}

void InternetSearch::DoSearchAsync(const int id, const QString &query, const SearchType type) {

  int timer_id = startTimer(kDelayedSearchTimeoutMs);
  delayed_searches_[timer_id].id_ = id;
  delayed_searches_[timer_id].query_ = query;
  delayed_searches_[timer_id].type_ = type;

}

void InternetSearch::SearchDone(const int service_id, const SongList &songs, const QString &error) {

  if (!pending_searches_.contains(service_id)) return;

  // Map back to the original id.
  const PendingState state = pending_searches_.take(service_id);
  const int search_id = state.orig_id_;

  if (songs.isEmpty()) {
    emit SearchError(search_id, error);
    return;
  }

  ResultList results;
  for (const Song &song : songs) {
    Result result;
    result.metadata_ = song;
    results << result;
  }

  if (results.isEmpty()) return;

  // Load cached pixmaps into the results
  for (InternetSearch::ResultList::iterator it = results.begin(); it != results.end(); ++it) {
    it->pixmap_cache_key_ = PixmapCacheKey(*it);
  }

  emit AddResults(search_id, results);

  MaybeSearchFinished(search_id);

}

void InternetSearch::MaybeSearchFinished(const int id) {

  if (pending_searches_.keys(PendingState(id, QStringList())).isEmpty()) {
    emit SearchFinished(id);
  }

}

void InternetSearch::CancelSearch(const int id) {

  QMap<int, DelayedSearch>::iterator it;
  for (it = delayed_searches_.begin(); it != delayed_searches_.end(); ++it) {
    if (it.value().id_ == id) {
      killTimer(it.key());
      delayed_searches_.erase(it);
      return;
    }
  }
  service_->CancelSearch();

}

void InternetSearch::timerEvent(QTimerEvent *e) {

  QMap<int, DelayedSearch>::iterator it = delayed_searches_.find(e->timerId());
  if (it != delayed_searches_.end()) {
    SearchAsync(it.value().id_, it.value().query_, it.value().type_);
    delayed_searches_.erase(it);
    return;
  }

  QObject::timerEvent(e);

}

QString InternetSearch::PixmapCacheKey(const InternetSearch::Result &result) const {
  return "internet:" % result.metadata_.url().toString();
}

bool InternetSearch::FindCachedPixmap(const InternetSearch::Result &result, QPixmap *pixmap) const {
  return pixmap_cache_.find(result.pixmap_cache_key_, pixmap);
}

int InternetSearch::LoadArtAsync(const InternetSearch::Result &result) {

  const int id = art_searches_next_id_++;

  pending_art_searches_[id] = result.pixmap_cache_key_;

  quint64 loader_id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, result.metadata_);
  cover_loader_tasks_[loader_id] = id;

  return id;

}

void InternetSearch::AlbumArtLoaded(const quint64 id, const QImage &image) {

  if (!cover_loader_tasks_.contains(id)) return;
  int orig_id = cover_loader_tasks_.take(id);

  const QString key = pending_art_searches_.take(orig_id);

  QPixmap pixmap = QPixmap::fromImage(image);
  pixmap_cache_.insert(key, pixmap);

  emit ArtLoaded(orig_id, pixmap);

}

QImage InternetSearch::ScaleAndPad(const QImage &image) {

  if (image.isNull()) return QImage();

  const QSize target_size = QSize(kArtHeight, kArtHeight);

  if (image.size() == target_size) return image;

  // Scale the image down
  QImage copy;
  copy = image.scaled(target_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Pad the image to kHeight x kHeight
  if (copy.size() == target_size) return copy;

  QImage padded_image(kArtHeight, kArtHeight, QImage::Format_ARGB32);
  padded_image.fill(0);

  QPainter p(&padded_image);
  p.drawImage((kArtHeight - copy.width()) / 2, (kArtHeight - copy.height()) / 2, copy);
  p.end();

  return padded_image;

}

MimeData *InternetSearch::LoadTracks(const ResultList &results) {

  if (results.isEmpty()) {
    return nullptr;
  }

  ResultList results_copy;
  for (const Result &result : results) {
    results_copy << result;
  }

  SongList songs;
  for (const Result &result : results) {
    songs << result.metadata_;
  }

  InternetSongMimeData *internet_song_mime_data = new InternetSongMimeData(service_);
  internet_song_mime_data->songs = songs;
  MimeData *mime_data = internet_song_mime_data;

  QList<QUrl> urls;
  for (const Result &result : results) {
    urls << result.metadata_.url();
  }
  mime_data->setUrls(urls);

  return mime_data;

}

void InternetSearch::UpdateStatusSlot(const int service_id, const QString &text) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  emit UpdateStatus(search_id, text);

}

void InternetSearch::ProgressSetMaximumSlot(const int service_id, const int max) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  emit ProgressSetMaximum(search_id, max);

}

void InternetSearch::UpdateProgressSlot(const int service_id, const int progress) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  emit UpdateProgress(search_id, progress);

}
