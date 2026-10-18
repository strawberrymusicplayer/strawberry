/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <functional>
#include <tuple>
#include <utility>

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/settings.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "constants/collectionsettings.h"
#include "collectionbackend.h"
#include "collectionmodel.h"
#include "collectionitem.h"
#include "collectionautoqueue.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kAutoQueueMode[] = "auto_queue_mode";

int CollectionSongId(const PlaylistItemPtr &item) {
  return item && item->IsLocalCollectionItem() ? item->EffectiveMetadata().id() : -1;
}

// Returns the songs of a random track, album or artist which has none of the excluded songs.
SongList PickSongs(const SharedPtr<CollectionBackend> collection_backend, const CollectionAutoQueue::Mode mode, const QStringList &excluded_song_ids) {

  QString where = u"unavailable = 0"_s;
  QString group_by = u"ROWID"_s;
  if (mode == CollectionAutoQueue::Mode::Album) {
    // Compilations are grouped by album only
    where += " AND album != ''"_L1;
    group_by = u"compilation_effective, CASE WHEN compilation_effective = 1 THEN '' ELSE effective_albumartist END, album"_s;
  }
  else if (mode == CollectionAutoQueue::Mode::Artist) {
    where += " AND compilation_effective = 0 AND effective_albumartist != ''"_L1;
    group_by = u"effective_albumartist"_s;
  }

  // Returns one track of the picked sort value, which is used to get all the other songs.
  QString sql = QStringLiteral("SELECT %1 FROM %2 WHERE ROWID = (SELECT MIN(ROWID) FROM %2 WHERE %3 GROUP BY %4").arg(Song::kRowIdColumnSpec, collection_backend->songs_table(), where, group_by);
  if (!excluded_song_ids.isEmpty()) {
    sql += QStringLiteral(" HAVING SUM(ROWID IN (%1)) = 0").arg(excluded_song_ids.join(u','));
  }
  sql += " ORDER BY random() LIMIT 1)"_L1;

  const SongList picked_songs = collection_backend->ExecuteQuery(sql);
  if (picked_songs.isEmpty() || mode == CollectionAutoQueue::Mode::Track) return picked_songs;

  const Song &picked_song = picked_songs.first();
  if (mode == CollectionAutoQueue::Mode::Artist) return collection_backend->GetArtistSongs(picked_song.effective_albumartist());
  return picked_song.is_compilation() ? collection_backend->GetCompilationSongs(picked_song.album()) : collection_backend->GetAlbumSongs(picked_song.effective_albumartist(), picked_song.album());

}

void SortInCollectionOrder(const CollectionModel *collection_model, SongList &songs) {

  QHash<int, QList<CollectionItem*>> paths;
  for (const Song &song : std::as_const(songs)) {
    QList<CollectionItem*> path;
    for (CollectionItem *item = collection_model->IndexToItem(collection_model->IndexOfSong(song.id())); item && item->parent; item = item->parent) {
      path.prepend(item);
    }
    paths.insert(song.id(), path);
  }

  std::stable_sort(songs.begin(), songs.end(), [collection_model, &paths](const Song &song1, const Song &song2) {
    const QList<CollectionItem*> path1 = paths.value(song1.id());
    const QList<CollectionItem*> path2 = paths.value(song2.id());
    if (path1.isEmpty() || path2.isEmpty()) {
      if (!path1.isEmpty() || !path2.isEmpty()) return path2.isEmpty();
      return std::make_tuple(song1.album(), song1.disc(), song1.track()) < std::make_tuple(song2.album(), song2.disc(), song2.track());
    }

    for (qsizetype i = 0; i < std::min(path1.count(), path2.count()); ++i) {
      if (path1[i] == path2[i]) continue;
      if (collection_model->CompareItems(path1[i], path2[i])) return true;
      if (collection_model->CompareItems(path2[i], path1[i])) return false;
      return std::less<CollectionItem*>()(path1[i], path2[i]);
    }
    return path1.count() < path2.count();
  });

}

}  // namespace

CollectionAutoQueue::CollectionAutoQueue(const SharedPtr<PlaylistManager> playlist_manager, CollectionModel *collection_model, QObject *parent)
    : QObject(parent),
      playlist_manager_(playlist_manager),
      collection_model_(collection_model),
      mode_(Mode::Off) {

  Settings s;
  s.beginGroup(CollectionSettings::kSettingsGroup);
  const int mode = s.value(kAutoQueueMode, static_cast<int>(Mode::Off)).toInt();
  s.endGroup();

  if (mode > static_cast<int>(Mode::Off) && mode <= static_cast<int>(Mode::Artist)) {
    mode_ = static_cast<Mode>(mode);
  }

}

void CollectionAutoQueue::CycleMode() {

  mode_ = mode_ == Mode::Artist ? Mode::Off : static_cast<Mode>(static_cast<int>(mode_) + 1);

  Settings s;
  s.beginGroup(CollectionSettings::kSettingsGroup);
  s.setValue(kAutoQueueMode, static_cast<int>(mode_));
  s.endGroup();

}

void CollectionAutoQueue::PlaylistDone() {

  if (mode_ == Mode::Off) return;

  Playlist *playlist = playlist_manager_->active();
  if (!playlist) return;

  QStringList playlist_song_ids;
  for (int row = 0; row < playlist->rowCount(); ++row) {
    const int song_id = CollectionSongId(playlist->item_at(row));
    if (song_id != -1) playlist_song_ids << QString::number(song_id);
  }

  SongList songs = PickSongs(collection_model_->backend(), mode_, playlist_song_ids);
  // Avoid the current shuffle value of the track that played
  if (songs.isEmpty() && !playlist_song_ids.isEmpty()) {
    songs = PickSongs(collection_model_->backend(), mode_, QStringList() << QString::number(CollectionSongId(playlist->current_item())));
  }
  if (songs.isEmpty()) return;

  SortInCollectionOrder(collection_model_, songs);

  // Queued, so the songs play in this order also when the playlist is shuffled.
  playlist->InsertCollectionItems(songs, -1, false, true);

}
