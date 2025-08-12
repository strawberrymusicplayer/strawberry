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

#include <cstdlib>
#include <algorithm>
#include <utility>
#include <memory>
#include <functional>
#include <unordered_map>
#include <random>
#include <chrono>

#include <QObject>
#include <QCoreApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QIODevice>
#include <QDataStream>
#include <QBuffer>
#include <QFile>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QMimeData>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QFont>
#include <QBrush>
#include <QUndoStack>
#include <QUndoCommand>
#include <QAbstractListModel>
#include <QMutableListIterator>
#include <QFlags>
#include <QSettings>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/mimedata.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/songmimedata.h"
#include "constants/timeconstants.h"
#include "constants/playlistsettings.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionlibrary.h"
#include "collection/collectionbackend.h"
#include "collection/collectionplaylistitem.h"
#include "covermanager/albumcoverloaderresult.h"
#include "queue/queue.h"
#include "playlist.h"
#include "playlistitem.h"
#include "playlistview.h"
#include "playlistsequence.h"
#include "playlistbackend.h"
#include "playlistfilter.h"
#include "playlistitemmimedata.h"
#include "songloaderinserter.h"
#include "songplaylistitem.h"
#include "playlistundocommandinsertitems.h"
#include "playlistundocommandremoveitems.h"
#include "playlistundocommandmoveitems.h"
#include "playlistundocommandreorderitems.h"
#include "playlistundocommandsortitems.h"
#include "playlistundocommandshuffleitems.h"

#include "smartplaylists/playlistgenerator.h"
#include "smartplaylists/playlistgeneratorinserter.h"
#include "smartplaylists/playlistgeneratormimedata.h"

#include "streaming/streamserviceplaylistitem.h"
#include "streaming/streamsongmimedata.h"
#include "streaming/streamingservice.h"

#include "radios/radiomimedata.h"
#include "radios/radiostreamplaylistitem.h"

using std::make_shared;
using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

const char *Playlist::kCddaMimeType = "x-content/audio-cdda";
const char *Playlist::kRowsMimetype = "application/x-strawberry-playlist-rows";
const char *Playlist::kPlayNowMimetype = "application/x-strawberry-play-now";
const int Playlist::kUndoStackSize = 20;
const int Playlist::kUndoItemLimit = 500;

namespace {

constexpr int kInvalidSongPriority = 200;
constexpr QRgb kInvalidSongColor = qRgb(0xC0, 0xC0, 0xC0);

constexpr int kDynamicHistoryPriority = 100;
constexpr QRgb kDynamicHistoryColor = qRgb(0x80, 0x80, 0x80);

constexpr qint64 kMinScrobblePointNsecs = 31LL * kNsecPerSec;
constexpr qint64 kMaxScrobblePointNsecs = 240LL * kNsecPerSec;

constexpr int kMaxPlayedIndexes = 100;

} // namespace

Playlist::Playlist(const SharedPtr<TaskManager> task_manager,
                   const SharedPtr<UrlHandlers> url_handlers,
                   const SharedPtr<PlaylistBackend> playlist_backend,
                   const SharedPtr<CollectionBackend> collection_backend,
                   const SharedPtr<TagReaderClient> tagreader_client,
                   const int id,
                   const QString &special_type,
                   const bool favorite,
                   QObject *parent)
    : QAbstractListModel(parent),
      is_loading_(false),
      filter_(new PlaylistFilter(this)),
      queue_(new Queue(this, this)),
      timer_save_(new QTimer(this)),
      task_manager_(task_manager),
      url_handlers_(url_handlers),
      playlist_backend_(playlist_backend),
      collection_backend_(collection_backend),
      tagreader_client_(tagreader_client),
      id_(id),
      favorite_(favorite),
      current_is_paused_(false),
      current_virtual_index_(-1),
      playlist_sequence_(nullptr),
      ignore_sorting_(false),
      undo_stack_(new QUndoStack(this)),
      special_type_(special_type),
      cancel_restore_(false),
      scrobbled_(false),
      scrobble_point_(-1),
      auto_sort_(false),
      sort_column_(Column::Title),
      sort_order_(Qt::AscendingOrder) {

  undo_stack_->setUndoLimit(kUndoStackSize);

  QObject::connect(this, &Playlist::rowsInserted, this, &Playlist::PlaylistChanged);
  QObject::connect(this, &Playlist::rowsRemoved, this, &Playlist::PlaylistChanged);

  Restore();

  filter_->setSourceModel(this);
  queue_->setSourceModel(this);

  QObject::connect(queue_, &Queue::rowsAboutToBeRemoved, this, &Playlist::TracksAboutToBeDequeued);
  QObject::connect(queue_, &Queue::rowsRemoved, this, &Playlist::TracksDequeued);

  QObject::connect(queue_, &Queue::rowsInserted, this, &Playlist::TracksEnqueued);

  QObject::connect(queue_, &Queue::layoutChanged, this, &Playlist::QueueLayoutChanged);

  QObject::connect(timer_save_, &QTimer::timeout, this, &Playlist::Save);

  column_alignments_ = PlaylistView::DefaultColumnAlignment();

  timer_save_->setSingleShot(true);
  timer_save_->setInterval(900ms);

}

Playlist::~Playlist() {
  items_.clear();
  ClearCollectionItems();
}

template<typename T>
void Playlist::InsertSongItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemPtrList items;
  items.reserve(songs.count());
  for (const Song &song : songs) {
    items << make_shared<T>(song);
  }

  InsertItems(items, pos, play_now, enqueue, enqueue_next);

}

QVariant Playlist::headerData(const int section, Qt::Orientation orientation, const int role) const {

  Q_UNUSED(orientation)

  if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return QVariant();

  const QString name = column_name(static_cast<Playlist::Column>(section));
  if (!name.isEmpty()) return name;

  return QVariant();

}

bool Playlist::column_is_editable(const Playlist::Column column) {

  switch (column) {
    case Column::Title:
    case Column::TitleSort:
    case Column::Artist:
    case Column::ArtistSort:
    case Column::Album:
    case Column::AlbumSort:
    case Column::AlbumArtist:
    case Column::AlbumArtistSort:
    case Column::Composer:
    case Column::ComposerSort:
    case Column::Performer:
    case Column::PerformerSort:
    case Column::Grouping:
    case Column::Track:
    case Column::Disc:
    case Column::Year:
    case Column::Genre:
    case Column::Comment:
      return true;
    default:
      break;
  }

  return false;

}

bool Playlist::set_column_value(Song &song, const Playlist::Column column, const QVariant &value) {

  if (!song.IsEditable()) return false;

  switch (column) {
    case Column::Title:
      song.set_title(value.toString());
      break;
    case Column::TitleSort:
      song.set_titlesort(value.toString());
      break;
    case Column::Artist:
      song.set_artist(value.toString());
      break;
    case Column::ArtistSort:
      song.set_artistsort(value.toString());
      break;
    case Column::Album:
      song.set_album(value.toString());
      break;
    case Column::AlbumSort:
      song.set_albumsort(value.toString());
      break;
    case Column::AlbumArtist:
      song.set_albumartist(value.toString());
      break;
    case Column::AlbumArtistSort:
      song.set_albumartistsort(value.toString());
      break;
    case Column::Composer:
      song.set_composer(value.toString());
      break;
    case Column::ComposerSort:
      song.set_composersort(value.toString());
      break;
    case Column::Performer:
      song.set_performer(value.toString());
      break;
    case Column::PerformerSort:
      song.set_performersort(value.toString());
      break;
    case Column::Grouping:
      song.set_grouping(value.toString());
      break;
    case Column::Track:
      song.set_track(value.toInt());
      break;
    case Column::Disc:
      song.set_disc(value.toInt());
      break;
    case Column::Year:
      song.set_year(value.toInt());
      break;
    case Column::Genre:
      song.set_genre(value.toString());
      break;
    case Column::Comment:
      song.set_comment(value.toString());
      break;
    default:
      break;
  }

  return true;

}

QVariant Playlist::data(const QModelIndex &idx, const int role) const {

  if (!idx.isValid()) {
    return QVariant();
  }

  switch (role) {
    case Role_IsCurrent:
      return current_item_index_.isValid() && idx.row() == current_item_index_.row();

    case Role_IsPaused:
      return current_is_paused_;

    case Role_StopAfter:
      return stop_after_.isValid() && stop_after_.row() == idx.row();

    case Role_QueuePosition:
      return queue_->PositionOf(idx);

    case Role_CanSetRating:
      return static_cast<Column>(idx.column()) == Column::Rating && items_[idx.row()]->IsLocalCollectionItem() && items_[idx.row()]->EffectiveMetadata().id() != -1;

    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::DisplayRole:{
      const PlaylistItemPtr item = items_[idx.row()];
      const Song song = item->EffectiveMetadata();

      // Don't forget to change Playlist::CompareItems when adding new columns
      switch (static_cast<Column>(idx.column())) {
        case Column::Title:              return song.PrettyTitle();
        case Column::TitleSort:          return song.titlesort();
        case Column::Artist:             return song.artist();
        case Column::ArtistSort:         return song.artistsort();
        case Column::Album:              return song.album();
        case Column::AlbumSort:          return song.albumsort();
        case Column::Length:             return song.length_nanosec();
        case Column::Track:              return song.track();
        case Column::Disc:               return song.disc();
        case Column::Year:               return song.year();
        case Column::OriginalYear:       return song.effective_originalyear();
        case Column::Genre:              return song.genre();
        case Column::AlbumArtist:        return song.playlist_effective_albumartist();
        case Column::AlbumArtistSort:    return song.albumartistsort();
        case Column::Composer:           return song.composer();
        case Column::ComposerSort:       return song.composersort();
        case Column::Performer:          return song.performer();
        case Column::PerformerSort:      return song.performersort();
        case Column::Grouping:           return song.grouping();

        case Column::PlayCount:          return song.playcount();
        case Column::SkipCount:          return song.skipcount();
        case Column::LastPlayed:         return song.lastplayed();

        case Column::Samplerate:         return song.samplerate();
        case Column::Bitdepth:           return song.bitdepth();
        case Column::Bitrate:            return song.bitrate();

        case Column::URL:                return song.effective_url();
        case Column::BaseFilename:       return song.basefilename();
        case Column::Filesize:           return song.filesize();
        case Column::Filetype:           return QVariant::fromValue(song.filetype());
        case Column::DateModified:       return song.mtime();
        case Column::DateCreated:        return song.ctime();

        case Column::Comment:
          if (role == Qt::DisplayRole)   return song.comment().simplified();
          return song.comment();

        case Column::EBUR128IntegratedLoudness: return song.ebur128_integrated_loudness_lufs().has_value() ? song.ebur128_integrated_loudness_lufs().value() : QVariant();

        case Column::EBUR128LoudnessRange:      return song.ebur128_loudness_range_lu().has_value() ? song.ebur128_loudness_range_lu().value() : QVariant();

        case Column::Source:             return QVariant::fromValue(song.source());

        case Column::Rating:             return song.rating();

        case Column::HasCUE:             return song.has_cue();

        case Column::BPM:                return song.bpm();
        case Column::Mood:               return song.mood();
        case Column::InitialKey:         return song.initial_key();

        case Column::Moodbar:
        case Column::ColumnCount:
          break;

      }

      return QVariant();
    }

    case Qt::TextAlignmentRole:
      return QVariant(column_alignments_.value(idx.column(), (Qt::AlignLeft | Qt::AlignVCenter)));

    case Qt::ForegroundRole:
      if (data(idx, Role_IsCurrent).toBool()) {
        // Ignore any custom colours for the currently playing item - they might clash with the glowing current track indicator.
        return QVariant();
      }

      if (items_[idx.row()]->HasCurrentForegroundColor()) {
        return QBrush(items_[idx.row()]->GetCurrentForegroundColor());
      }
      if (idx.row() < dynamic_history_length() - 1) {
        return QBrush(kDynamicHistoryColor);
      }

      return QVariant();

    case Qt::BackgroundRole:
      if (data(idx, Role_IsCurrent).toBool()) {
        // Ignore any custom colours for the currently playing item - they might clash with the glowing current track indicator.
        return QVariant();
      }

      if (items_[idx.row()]->HasCurrentBackgroundColor()) {
        return QBrush(items_[idx.row()]->GetCurrentBackgroundColor());
      }
      return QVariant();

    case Qt::FontRole:
      if (items_[idx.row()]->GetShouldSkip()) {
        QFont track_font;
        track_font.setStrikeOut(true);
        return track_font;
      }
      return QVariant();

    default:
      return QVariant();
  }

}

#ifdef HAVE_MOODBAR
void Playlist::MoodbarUpdated(const QModelIndex &idx) {
  Q_EMIT dataChanged(idx.sibling(idx.row(), static_cast<int>(Column::Moodbar)), idx.sibling(idx.row(), static_cast<int>(Column::Moodbar)));
}
#endif

bool Playlist::setData(const QModelIndex &idx, const QVariant &value, const int role) {

  Q_UNUSED(role);

  const int row = idx.row();
  const PlaylistItemPtr item = item_at(row);
  Song song = item->OriginalMetadata();

  if (idx.data() == value) return false;

  if (!set_column_value(song, static_cast<Column>(idx.column()), value)) return false;

  if (song.url().isLocalFile()) {
    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(song.url().toLocalFile(), song);
    QPersistentModelIndex persistent_index = QPersistentModelIndex(idx);
    SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, persistent_index, item, connection]() {
      SongSaveComplete(reply, persistent_index, item->OriginalMetadata());
      QObject::disconnect(*connection);
    }, Qt::QueuedConnection);
  }
  else if (song.is_radio()) {
    item->SetOriginalMetadata(song);
    ScheduleSave();
  }

  return true;

}

void Playlist::SongSaveComplete(TagReaderReplyPtr reply, const QPersistentModelIndex &idx, const Song &old_metadata) {

  if (reply->success() && idx.isValid()) {
    if (reply->success()) {
      ItemReload(idx, old_metadata, true);
    }
    else {
      if (reply->error().isEmpty()) {
        Q_EMIT Error(tr("Could not write metadata to %1").arg(reply->filename()));
      }
      else {
        Q_EMIT Error(tr("Could not write metadata to %1: %2").arg(reply->filename(), reply->error()));
      }
    }
  }

}

void Playlist::ItemReload(const QPersistentModelIndex &idx, const Song &old_metadata, const bool metadata_edit) {

  if (idx.isValid()) {
    PlaylistItemPtr item = item_at(idx.row());
    if (item) {
      QFuture<void> future = item->BackgroundReload();
      QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
      QObject::connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, idx, old_metadata, metadata_edit]() {
        ItemReloadComplete(idx, old_metadata, metadata_edit);
        watcher->deleteLater();
      });
      watcher->setFuture(future);
    }
  }

}

void Playlist::ItemReloadComplete(const QPersistentModelIndex &idx, const Song &old_metadata, const bool metadata_edit) {

  if (idx.isValid()) {
    const PlaylistItemPtr item = item_at(idx.row());
    if (item) {
      RowDataChanged(idx.row(), ChangedColumns(old_metadata, item->EffectiveMetadata()));
      if (idx.row() == current_row()) {
        if (MinorMetadataChange(old_metadata, item->EffectiveMetadata())) {
          Q_EMIT CurrentSongMetadataChanged(item->EffectiveMetadata());
        }
        else {
          Q_EMIT CurrentSongChanged(item->EffectiveMetadata());
        }
      }
      if (metadata_edit) {
        Q_EMIT EditingFinished(id_, idx);
      }
      ScheduleSaveAsync();
    }
  }

}

int Playlist::current_row() const {
  return current_item_index_.isValid() ? current_item_index_.row() : -1;
}

QModelIndex Playlist::current_index() const {
  return current_item_index_;
}

int Playlist::last_played_row() const {
  return last_played_item_index_.isValid() ? last_played_item_index_.row() : -1;
}

void Playlist::ShuffleModeChanged(const PlaylistSequence::ShuffleMode shuffle_mode) {
  Q_UNUSED(shuffle_mode)
  ReshuffleIndices();
}

bool Playlist::FilterContainsVirtualIndex(const int i) const {
  if (i < 0 || i >= virtual_items_.count()) return false;

  return filter_->filterAcceptsRow(virtual_items_[i], QModelIndex());
}

int Playlist::NextVirtualIndex(int i, const bool ignore_repeat_track) const {

  const PlaylistSequence::RepeatMode repeat_mode = RepeatMode();
  const bool album_only = repeat_mode == PlaylistSequence::RepeatMode::Album || ShuffleMode() == PlaylistSequence::ShuffleMode::InsideAlbum;

  // This one's easy - if we have to repeat the current track then just return i
  if (repeat_mode == PlaylistSequence::RepeatMode::Track && !ignore_repeat_track) {
    if (!FilterContainsVirtualIndex(i)) {
      return static_cast<int>(virtual_items_.count());  // It's not in the filter any more
    }
    return i;
  }

  // If we're not bothered about whether a song is on the same album then return the next virtual index, whatever it is.
  if (!album_only) {
    ++i;

    // Advance i until we find any track that is in the filter, skipping the selected to be skipped
    while (i < virtual_items_.count() && (!FilterContainsVirtualIndex(i) || item_at(virtual_items_[i])->GetShouldSkip())) {
      ++i;
    }
    return i;
  }

  // We need to advance i until we get something else on the same album
  const Song last_song = current_item_metadata();
  for (int j = i + 1; j < virtual_items_.count(); ++j) {
    if (item_at(virtual_items_[j])->GetShouldSkip()) {
      continue;
    }
    const Song this_song = item_at(virtual_items_[j])->EffectiveMetadata();
    if (((last_song.is_compilation() && this_song.is_compilation()) ||
         last_song.effective_albumartist() == this_song.effective_albumartist()) &&
        last_song.album() == this_song.album() &&
        FilterContainsVirtualIndex(j)) {
      return j;  // Found one
    }
  }

  // Couldn't find one - return past the end of the list
  return static_cast<int>(virtual_items_.count());

}

int Playlist::PreviousVirtualIndex(int i, const bool ignore_repeat_track) const {

  const PlaylistSequence::RepeatMode repeat_mode = RepeatMode();
  const bool album_only = repeat_mode == PlaylistSequence::RepeatMode::Album || ShuffleMode() == PlaylistSequence::ShuffleMode::InsideAlbum;

  // This one's easy - if we have to repeat the current track then just return i
  if (repeat_mode == PlaylistSequence::RepeatMode::Track && !ignore_repeat_track) {
    if (!FilterContainsVirtualIndex(i)) return -1;
    return i;
  }

  // If we're not bothered about whether a song is on the same album then return the previous virtual index, whatever it is.
  if (!album_only) {
    --i;

    // Decrement i until we find any track that is in the filter
    while (i >= 0 && (!FilterContainsVirtualIndex(i) || item_at(virtual_items_[i])->GetShouldSkip())) --i;
    return i;
  }

  // We need to decrement i until we get something else on the same album
  Song last_song = current_item_metadata();
  for (int j = i - 1; j >= 0; --j) {
    if (item_at(virtual_items_[j])->GetShouldSkip()) {
      continue;
    }
    Song this_song = item_at(virtual_items_[j])->EffectiveMetadata();
    if (((last_song.is_compilation() && this_song.is_compilation()) || last_song.artist() == this_song.artist()) && last_song.album() == this_song.album() && FilterContainsVirtualIndex(j)) {
      return j;  // Found one
    }
  }

  // Couldn't find one - return before the start of the list
  return -1;

}

int Playlist::next_row(const bool ignore_repeat_track) {

  // Any queued items take priority
  if (!queue_->is_empty()) {
    return queue_->PeekNext();
  }

  int next_virtual_index = NextVirtualIndex(current_virtual_index_, ignore_repeat_track);
  if (next_virtual_index >= virtual_items_.count()) {
    // We've gone off the end of the playlist.

    switch (RepeatMode()) {
      case PlaylistSequence::RepeatMode::Off:
      case PlaylistSequence::RepeatMode::Intro:
        return -1;
      case PlaylistSequence::RepeatMode::Track:
        next_virtual_index = current_virtual_index_;
        break;

      default:
        ReshuffleIndices();
        next_virtual_index = NextVirtualIndex(-1, ignore_repeat_track);
        break;
    }
  }

  // Still off the end?  Then just give up
  if (next_virtual_index < 0 || next_virtual_index >= virtual_items_.count()) return -1;

  return virtual_items_.value(next_virtual_index);

}

int Playlist::previous_row(const bool ignore_repeat_track) {

  while (!played_indexes_.isEmpty()) {
    const QPersistentModelIndex idx = played_indexes_.takeLast();
    if (idx.isValid() && idx != current_item_index_) return idx.row();
  }

  int prev_virtual_index = PreviousVirtualIndex(current_virtual_index_, ignore_repeat_track);
  if (prev_virtual_index < 0) {
    // We've gone off the beginning of the playlist.

    switch (RepeatMode()) {
      case PlaylistSequence::RepeatMode::Off:
        return -1;
      case PlaylistSequence::RepeatMode::Track:
        prev_virtual_index = current_virtual_index_;
        break;

      default:
        prev_virtual_index = PreviousVirtualIndex(static_cast<int>(virtual_items_.count()), ignore_repeat_track);
        break;
    }
  }

  // Still off the beginning?  Then just give up
  if (prev_virtual_index < 0) return -1;

  return virtual_items_.value(prev_virtual_index);

}

void Playlist::set_current_row(const int i, const AutoScroll autoscroll, const bool is_stopping, const bool force_inform) {

  const QPersistentModelIndex old_current_item_index = current_item_index_;
  QPersistentModelIndex new_current_item_index;
  if (i != -1) new_current_item_index = QPersistentModelIndex(index(i, 0, QModelIndex()));

  if (new_current_item_index != current_item_index_) ClearStreamMetadata();

  const int nextrow = next_row();
  if (nextrow != -1 && nextrow != i) {
    PlaylistItemPtr next_item = item_at(nextrow);
    if (next_item) {
      next_item->ClearStreamMetadata();
      Q_EMIT dataChanged(index(nextrow, 0), index(nextrow, ColumnCount - 1));
    }
  }

  current_item_index_ = new_current_item_index;

  // If the given item is the first in the queue, remove it from the queue
  if (current_item_index_.isValid() && current_item_index_.row() == queue_->PeekNext()) {
    queue_->TakeNext();
  }

  if (current_item_index_ == old_current_item_index && !force_inform) {
    UpdateScrobblePoint();
    return;
  }

  if (old_current_item_index.isValid()) {
    Q_EMIT dataChanged(old_current_item_index, old_current_item_index.sibling(old_current_item_index.row(), ColumnCount - 1));
  }

  // Update the virtual index
  if (i == -1) {
    current_virtual_index_ = -1;
  }
  else if (ShuffleMode() != PlaylistSequence::ShuffleMode::Off && current_virtual_index_ == -1) {
    // This is the first thing we're playing so we want to make sure the array is shuffled
    ReshuffleIndices();

    // Bring the one we've been asked to play to the start of the list
    virtual_items_.takeAt(virtual_items_.indexOf(i));
    virtual_items_.prepend(i);
    current_virtual_index_ = 0;
  }
  else if (ShuffleMode() != PlaylistSequence::ShuffleMode::Off) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(i));
  }
  else {
    current_virtual_index_ = i;
  }

  if (current_item_index_.isValid() && !is_stopping) {
    InformOfCurrentSongChange(false);
    Q_EMIT dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount - 1));
    Q_EMIT MaybeAutoscroll(autoscroll);
  }

  // The structure of a dynamic playlist is as follows:
  //   history - active song - future
  // We have to ensure that this invariant is maintained.
  if (dynamic_playlist_ && current_item_index_.isValid()) {

    // When advancing to the next track
    if (old_current_item_index.isValid() && i > old_current_item_index.row()) {
      // Move the new item one position ahead of the last item in the history.
      MoveItemWithoutUndo(current_item_index_.row(), dynamic_history_length());

      // Compute the number of new items that have to be inserted
      // This is not necessarily 1 because the user might have added or removed items manually.
      // Note that the future excludes the current item.
      const int count = static_cast<int>(dynamic_history_length() + 1 + dynamic_playlist_->GetDynamicFuture() - items_.count());
      if (count > 0) {
        InsertDynamicItems(count);
      }

      // Shrink the history, again this is not necessarily by 1, because the user might have moved items by hand.
      const int remove_count = dynamic_history_length() - dynamic_playlist_->GetDynamicHistory();
      if (0 < remove_count) RemoveItemsWithoutUndo(0, remove_count);
    }

    // The above actions make all commands on the undo stack invalid, so we better clear it.
    undo_stack_->clear();
  }

  if (current_item_index_.isValid()) {
    last_played_item_index_ = current_item_index_;
    played_indexes_.append(current_item_index_);
    if (played_indexes_.count() > kMaxPlayedIndexes) {
      played_indexes_.remove(0, played_indexes_.count() - kMaxPlayedIndexes);
    }
    ScheduleSave();
  }

  UpdateScrobblePoint();

}

void Playlist::InsertDynamicItems(const int count) {

  PlaylistGeneratorInserter *inserter = new PlaylistGeneratorInserter(task_manager_, collection_backend_, this);
  QObject::connect(inserter, &PlaylistGeneratorInserter::Error, this, &Playlist::Error);
  QObject::connect(inserter, &PlaylistGeneratorInserter::PlayRequested, this, &Playlist::PlayRequested);

  inserter->Load(this, -1, false, false, false, dynamic_playlist_, count);

}

Qt::ItemFlags Playlist::flags(const QModelIndex &idx) const {

  if (idx.isValid()) {
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (item_at(idx.row())->EffectiveMetadata().IsEditable() && column_is_editable(static_cast<Column>(idx.column()))) flags |= Qt::ItemIsEditable;
    return flags;
  }

  return Qt::ItemIsDropEnabled;

}

QStringList Playlist::mimeTypes() const {

  return QStringList() << u"text/uri-list"_s << QLatin1String(kRowsMimetype);

}

Qt::DropActions Playlist::supportedDropActions() const {
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
}

bool Playlist::dropMimeData(const QMimeData *data, Qt::DropAction action, const int row, const int column, const QModelIndex &parent_index) {

  Q_UNUSED(column)
  Q_UNUSED(parent_index)

  if (action == Qt::IgnoreAction) return false;

  bool play_now = false;
  bool enqueue_now = false;
  bool enqueue_next_now = false;

  if (const MimeData *mime_data = qobject_cast<const MimeData*>(data)) {
    if (mime_data->clear_first_) {
      Clear();
    }
    play_now = mime_data->play_now_;
    enqueue_now = mime_data->enqueue_now_;
    enqueue_next_now = mime_data->enqueue_next_now_;
  }

  if (const SongMimeData *song_data = qobject_cast<const SongMimeData*>(data)) {
    // Dragged from a collection
    if (song_data->backend && Song::IsLinkedCollectionSource(song_data->backend->source())) {
      InsertSongItems<CollectionPlaylistItem>(song_data->songs, row, play_now, enqueue_now, enqueue_next_now);
    }
    else {
      InsertSongItems<SongPlaylistItem>(song_data->songs, row, play_now, enqueue_now, enqueue_next_now);
    }
  }
  else if (const PlaylistItemMimeData *item_mimedata = qobject_cast<const PlaylistItemMimeData*>(data)) {
    InsertItems(item_mimedata->items_, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const PlaylistGeneratorMimeData *generator_mimedata = qobject_cast<const PlaylistGeneratorMimeData*>(data)) {
    InsertSmartPlaylist(generator_mimedata->generator_, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const StreamSongMimeData *stream_song_mimedata = qobject_cast<const StreamSongMimeData*>(data)) {
    InsertStreamingItems(stream_song_mimedata->service, stream_song_mimedata->songs, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const RadioMimeData *radio_mimedata = qobject_cast<const RadioMimeData*>(data)) {
    InsertRadioItems(radio_mimedata->songs, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (data->hasFormat(QLatin1String(kRowsMimetype))) {
    // Dragged from the playlist
    // Rearranging it is tricky...

    // Get the list of rows that were moved
    QList<int> source_rows;
    Playlist *source_playlist = nullptr;
    qint64 pid = 0;
    qint64 own_pid = QCoreApplication::applicationPid();

    QDataStream stream(data->data(QLatin1String(kRowsMimetype)));
    stream.readRawData(reinterpret_cast<char*>(&source_playlist), sizeof(&source_playlist));
    stream >> source_rows;
    if (!stream.atEnd()) {
      stream.readRawData(reinterpret_cast<char*>(&pid), sizeof(pid));
    }
    else {
      pid = own_pid;
    }

    std::stable_sort(source_rows.begin(), source_rows.end());  // Make sure we take them in order

    if (source_playlist == this) {
      // Dragged from this playlist - rearrange the items
      undo_stack_->push(new PlaylistUndoCommandMoveItems(this, source_rows, row));
    }
    else if (pid == own_pid) {
      // Drag from a different playlist
      PlaylistItemPtrList items;
      items.reserve(source_rows.count());
      for (const int i : std::as_const(source_rows)) items << source_playlist->item_at(i);

      if (items.count() > kUndoItemLimit) {
        // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
        InsertItemsWithoutUndo(items, row, false, false);
        undo_stack_->clear();
      }
      else {
        undo_stack_->push(new PlaylistUndoCommandInsertItems(this, items, row));
      }

      // Remove the items from the source playlist if it was a move event
      if (action == Qt::MoveAction) {
        for (const int i : std::as_const(source_rows)) {
          source_playlist->undo_stack()->push(new PlaylistUndoCommandRemoveItems(source_playlist, i, 1));
        }
      }
    }
  }
  else if (data->hasFormat(QLatin1String(kCddaMimeType))) {
    SongLoaderInserter *inserter = new SongLoaderInserter(task_manager_, tagreader_client_, url_handlers_, collection_backend_);
    QObject::connect(inserter, &SongLoaderInserter::Error, this, &Playlist::Error);
    inserter->LoadAudioCD(this, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (data->hasUrls()) {
    // URL list dragged from the file list or some other app
    InsertUrls(data->urls(), row, play_now, enqueue_now, enqueue_next_now);
  }

  return true;

}

void Playlist::InsertUrls(const QList<QUrl> &urls, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  SongLoaderInserter *inserter = new SongLoaderInserter(task_manager_, tagreader_client_, url_handlers_, collection_backend_);
  QObject::connect(inserter, &SongLoaderInserter::Error, this, &Playlist::Error);

  inserter->Load(this, pos, play_now, enqueue, enqueue_next, urls);

}

void Playlist::InsertSmartPlaylist(PlaylistGeneratorPtr generator, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  // Hack: If the generator hasn't got a collection set then use the main one
  if (!generator->collection()) {
    generator->set_collection_backend(collection_backend_);
  }

  PlaylistGeneratorInserter *inserter = new PlaylistGeneratorInserter(task_manager_, collection_backend_, this);
  QObject::connect(inserter, &PlaylistGeneratorInserter::Error, this, &Playlist::Error);

  inserter->Load(this, pos, play_now, enqueue, enqueue_next, generator);

  if (generator->is_dynamic()) {
    TurnOnDynamicPlaylist(generator);
  }

}

void Playlist::TurnOnDynamicPlaylist(PlaylistGeneratorPtr gen) {

  dynamic_playlist_ = gen;
  ShuffleModeChanged(PlaylistSequence::ShuffleMode::Off);
  Q_EMIT DynamicModeChanged(true);

  ScheduleSave();

}

void Playlist::MoveItemWithoutUndo(const int source, const int dest) {
  MoveItemsWithoutUndo(QList<int>() << source, dest);
}

void Playlist::MoveItemsWithoutUndo(const QList<int> &source_rows, int pos) {

  Q_EMIT layoutAboutToBeChanged();

  PlaylistItemPtrList old_items = items_;
  PlaylistItemPtrList moved_items;
  moved_items.reserve(source_rows.count());

  if (pos < 0) {
    pos = static_cast<int>(items_.count());
  }

  // Take the items out of the list first, keeping track of whether the insertion point changes
  int offset = 0;
  int start = pos;
  for (const int source_row : source_rows) {
    moved_items << items_.takeAt(source_row - offset);
    if (pos > source_row) {
      --start;
    }
    ++offset;
  }

  // Put the items back in
  for (int i = start; i < start + moved_items.count(); ++i) {
    moved_items[i - start]->RemoveForegroundColor(kDynamicHistoryPriority);
    items_.insert(i, moved_items[i - start]);
  }

  // Update persistent indexes
  const QModelIndexList pidx_list = persistentIndexList();
  for (const QModelIndex &pidx : pidx_list) {
    const int dest_offset = static_cast<int>(source_rows.indexOf(pidx.row()));
    if (dest_offset != -1) {
      // This index was moved
      changePersistentIndex(pidx, index(start + dest_offset, pidx.column(), QModelIndex()));
    }
    else {
      int d = 0;
      for (int source_row : source_rows) {
        if (pidx.row() > source_row) d--;
      }
      if (pidx.row() + d >= start) d += static_cast<int>(source_rows.count());

      changePersistentIndex(pidx, index(pidx.row() + d, pidx.column(), QModelIndex()));
    }
  }

  // Update virtual items
  if (ShuffleMode() != PlaylistSequence::ShuffleMode::Off) {
    const QList<int> old_virtual_items = virtual_items_;
    for (int i = 0; i < virtual_items_.count(); ++i) {
      virtual_items_[i] = static_cast<int>(items_.indexOf(old_items[old_virtual_items[i]]));
    }
  }

  // Update current virtual index
  if (current_item_index_.isValid()) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(current_item_index_.row()));
  }
  else {
    current_virtual_index_ = -1;
  }

  Q_EMIT layoutChanged();

  ScheduleSave();

}

void Playlist::MoveItemsWithoutUndo(int start, const QList<int> &dest_rows) {

  Q_EMIT layoutAboutToBeChanged();

  PlaylistItemPtrList old_items = items_;
  PlaylistItemPtrList moved_items;
  moved_items.reserve(dest_rows.count());

  int pos = start;
  for (const int dest_row : dest_rows) {
    if (dest_row < pos) --start;
  }

  if (start < 0) {
    start = static_cast<int>(items_.count() - dest_rows.count());
  }

  // Take the items out of the list first
  for (int i = 0; i < dest_rows.count(); ++i) {
    moved_items << items_.takeAt(start);
  }

  // Put the items back in
  int offset = 0;
  for (int dest_row : dest_rows) {
    items_.insert(dest_row, moved_items[offset]);
    offset++;
  }

  // Update persistent indexes
  const QModelIndexList pidx_list = persistentIndexList();
  for (const QModelIndex &pidx : pidx_list) {
    if (pidx.row() >= start && pidx.row() < start + dest_rows.count()) {
      // This index was moved
      const int i = pidx.row() - start;
      changePersistentIndex(pidx, index(dest_rows[i], pidx.column(), QModelIndex()));
    }
    else {
      int d = 0;
      if (pidx.row() >= start + dest_rows.count()) {
        d -= static_cast<int>(dest_rows.count());
      }

      for (int dest_row : dest_rows) {
        if (pidx.row() + d > dest_row) d++;
      }

      changePersistentIndex(pidx, index(pidx.row() + d, pidx.column(), QModelIndex()));
    }
  }

  // Update virtual items
  if (ShuffleMode() != PlaylistSequence::ShuffleMode::Off) {
    const QList<int> old_virtual_items = virtual_items_;
    for (int i = 0; i < virtual_items_.count(); ++i) {
      virtual_items_[i] = static_cast<int>(items_.indexOf(old_items[old_virtual_items[i]]));
    }
  }

  // Update current virtual index
  if (current_item_index_.isValid()) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(current_item_index_.row()));
  }
  else {
    current_virtual_index_ = -1;
  }

  Q_EMIT layoutChanged();

  ScheduleSave();

}

void Playlist::InsertItems(const PlaylistItemPtrList &itemsIn, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  if (itemsIn.isEmpty()) {
    return;
  }

  PlaylistItemPtrList items = itemsIn;

  const int start = pos == -1 ? static_cast<int>(items_.count()) : pos;

  if (items.count() > kUndoItemLimit) {
    // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
    InsertItemsWithoutUndo(items, pos, enqueue, enqueue_next);
    undo_stack_->clear();
  }
  else {
    undo_stack_->push(new PlaylistUndoCommandInsertItems(this, items, pos, enqueue, enqueue_next));
  }

  if (play_now) Q_EMIT PlayRequested(index(start, 0), AutoScroll::Maybe);

}

void Playlist::InsertItemsWithoutUndo(const PlaylistItemPtrList &items, const int pos, const bool enqueue, const bool enqueue_next) {

  if (items.isEmpty()) return;

  const int start = pos == -1 ? static_cast<int>(items_.count()) : pos;
  const int end = start + static_cast<int>(items.count()) - 1;

  beginInsertRows(QModelIndex(), start, end);
  for (int i = start; i <= end; ++i) {
    const PlaylistItemPtr item = items[i - start];
    items_.insert(i, item);
    virtual_items_ << static_cast<int>(virtual_items_.count());

    if (Song::IsLinkedCollectionSource(item->source())) {
      const int id = item->EffectiveMetadata().id();
      if (id != -1) {
        collection_items_[item->EffectiveMetadata().source_id()].insert(id, item);
      }
    }

    if (item == current_item()) {
      // It's one we removed before that got re-added through an undo
      current_item_index_ = index(i, 0);
      last_played_item_index_ = current_item_index_;
    }
  }
  endInsertRows();

  if (enqueue) {
    QModelIndexList indexes;
    for (int i = start; i <= end; ++i) {
      indexes << index(i, 0);  // clazy:exclude=reserve-candidates
    }
    queue_->ToggleTracks(indexes);
  }

  if (enqueue_next) {
    QModelIndexList indexes;
    for (int i = start; i <= end; ++i) {
      indexes << index(i, 0);  // clazy:exclude=reserve-candidates
    }
    queue_->InsertFirst(indexes);
  }

  if (auto_sort_) {
    sort(static_cast<int>(sort_column_), sort_order_);
  }

  ReshuffleIndices();

  ScheduleSave();

}

void Playlist::InsertCollectionItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {
  InsertSongItems<CollectionPlaylistItem>(songs, pos, play_now, enqueue, enqueue_next);
}

void Playlist::InsertSongs(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {
  InsertSongItems<SongPlaylistItem>(songs, pos, play_now, enqueue, enqueue_next);
}

void Playlist::InsertSongsOrCollectionItems(const SongList &songs, const QString &playlist_name, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  if (!playlist_name.isEmpty()) {
    Q_EMIT Rename(id_, playlist_name);
  }

  PlaylistItemPtrList items;
  items.reserve(songs.count());
  for (const Song &song : songs) {
    items << PlaylistItem::NewFromSong(song);
  }

  InsertItems(items, pos, play_now, enqueue, enqueue_next);

}

void Playlist::InsertStreamingItems(StreamingServicePtr service, const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemPtrList playlist_items;
  playlist_items.reserve(songs.count());
  for (const Song &song : songs) {
    playlist_items << make_shared<StreamServicePlaylistItem>(service, song);
  }

  InsertItems(playlist_items, pos, play_now, enqueue, enqueue_next);

}

void Playlist::InsertRadioItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemPtrList playlist_items;
  playlist_items.reserve(songs.count());
  for (const Song &song : songs) {
    playlist_items << make_shared<RadioStreamPlaylistItem>(song);
  }

  InsertItems(playlist_items, pos, play_now, enqueue, enqueue_next);

}

void Playlist::UpdateItems(SongList songs) {

  qLog(Debug) << "Updating playlist with new tracks' info";

  // We first convert our songs list into a linked list (a 'real' list), because removals are faster with QLinkedList.
  // Next, we walk through the list of playlist's items then the list of songs
  // we want to update: if an item corresponds to the song (we rely on URL for this), we update the item with the new metadata,
  // then we remove song from our list because we will not need to check it again.
  // And we also update undo actions.

  for (int i = 0; i < items_.size(); i++) {
    // Update current items list
    QMutableListIterator<Song> it(songs);
    while (it.hasNext()) {
      const Song &song = it.next();
      const PlaylistItemPtr item = items_.value(i);
      if (item->EffectiveMetadata().url() == song.url() && (item->EffectiveMetadata().filetype() == Song::FileType::Unknown || item->EffectiveMetadata().filetype() == Song::FileType::Stream || item->EffectiveMetadata().filetype() == Song::FileType::CDDA || !item->EffectiveMetadata().init_from_file())) {
        PlaylistItemPtr new_item;
        if (song.is_linked_collection_song()) {
          new_item = make_shared<CollectionPlaylistItem>(song);
          if (collection_items_[song.source_id()].contains(song.id(), item)) collection_items_[song.source_id()].remove(song.id(), item);
          collection_items_[song.source_id()].insert(song.id(), new_item);
        }
        else {
          if (song.url().isLocalFile()) {
            new_item = make_shared<SongPlaylistItem>(song);
          }
          else {
            if (song.is_radio()) {
              new_item = make_shared<RadioStreamPlaylistItem>(song);
            }
            else {
              new_item = make_shared<StreamServicePlaylistItem>(song);
            }
          }
        }
        items_[i] = new_item;
        Q_EMIT dataChanged(index(i, 0), index(i, ColumnCount - 1));
        // Also update undo actions
        for (int y = 0; y < undo_stack_->count(); y++) {
          QUndoCommand *undo_action = const_cast<QUndoCommand*>(undo_stack_->command(i));
          PlaylistUndoCommandInsertItems *undo_action_insert = dynamic_cast<PlaylistUndoCommandInsertItems*>(undo_action);
          if (undo_action_insert) {
            bool found_and_updated = undo_action_insert->UpdateItem(new_item);
            if (found_and_updated) break;
          }
        }
        it.remove();
        break;
      }
    }
  }

  Q_EMIT PlaylistChanged();

  ScheduleSave();

}

QMimeData *Playlist::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  // We only want one index per row, but we can't just take column 0 because the user might have hidden it.
  const int first_column = indexes.first().column();

  QList<QUrl> urls;
  QList<int> rows;
  for (const QModelIndex &idx : indexes) {
    if (idx.column() != first_column) continue;

    urls << items_[idx.row()]->OriginalUrl();
    rows << idx.row();
  }

  QBuffer buffer;
  if (!buffer.open(QIODevice::WriteOnly)) {
    return nullptr;
  }
  QDataStream stream(&buffer);

  const Playlist *self = this;
  const qint64 pid = QCoreApplication::applicationPid();

  stream.writeRawData(reinterpret_cast<char*>(&self), sizeof(&self));
  stream << rows;
  stream.writeRawData(reinterpret_cast<const char*>(&pid), sizeof(pid));
  buffer.close();

  QMimeData *mimedata = new QMimeData;
  mimedata->setUrls(urls);
  mimedata->setData(QLatin1String(kRowsMimetype), buffer.data());

  return mimedata;

}

namespace {

inline bool CompareStr(const QString &a, const QString &b) {
  return QString::localeAwareCompare(a.toLower(), b.toLower()) < 0;
}

template<typename T>
inline bool CompareVal(const T &a, const T &b) {
  return a < b;
}

}  // namespace

bool Playlist::CompareItems(const Column column, const Qt::SortOrder order, PlaylistItemPtr _a, PlaylistItemPtr _b) {

  PlaylistItemPtr a = (order == Qt::AscendingOrder) ? _a : _b;
  PlaylistItemPtr b = (order == Qt::AscendingOrder) ? _b : _a;

  const auto &ma = a->EffectiveMetadata();
  const auto &mb = b->EffectiveMetadata();

  switch (column) {
    case Column::Title:                     return CompareStr(ma.effective_titlesort(), mb.effective_titlesort());
    case Column::TitleSort:                 return CompareStr(ma.titlesort(), mb.titlesort());
    case Column::Artist:                    return CompareStr(ma.effective_artistsort(), mb.effective_artistsort());
    case Column::ArtistSort:                return CompareStr(ma.artistsort(), mb.artistsort());
    case Column::Album:                     return CompareStr(ma.effective_albumsort(), mb.effective_albumsort());
    case Column::AlbumSort:                 return CompareStr(ma.albumsort(), mb.albumsort());
    case Column::Length:                    return CompareVal(ma.length_nanosec(), mb.length_nanosec());
    case Column::Track:                     return CompareVal(ma.track(), mb.track());
    case Column::Disc:                      return CompareVal(ma.disc(), mb.disc());
    case Column::Year:                      return CompareVal(ma.year(), mb.year());
    case Column::OriginalYear:              return CompareVal(ma.effective_originalyear(), mb.effective_originalyear());
    case Column::Genre:                     return CompareStr(ma.genre(), mb.genre());
    case Column::AlbumArtist:               return CompareStr(ma.playlist_effective_albumartistsort(), mb.playlist_effective_albumartistsort());
    case Column::AlbumArtistSort:           return CompareStr(ma.albumartistsort(), mb.albumartistsort());
    case Column::Composer:                  return CompareStr(ma.effective_composersort(), mb.effective_composersort());
    case Column::ComposerSort:              return CompareStr(ma.composersort(), mb.composersort());
    case Column::Performer:                 return CompareStr(ma.effective_performersort(), mb.effective_performersort());
    case Column::PerformerSort:             return CompareStr(ma.performersort(), mb.performersort());
    case Column::Grouping:                  return CompareStr(ma.grouping(), mb.grouping());

    case Column::PlayCount:                 return CompareVal(ma.playcount(), mb.playcount());
    case Column::SkipCount:                 return CompareVal(ma.skipcount(), mb.skipcount());
    case Column::LastPlayed:                return CompareVal(ma.lastplayed(), mb.lastplayed());

    case Column::Bitrate:                   return CompareVal(ma.bitrate(), mb.bitrate());
    case Column::Samplerate:                return CompareVal(ma.samplerate(), mb.samplerate());
    case Column::Bitdepth:                  return CompareVal(ma.bitdepth(), mb.bitdepth());
    case Column::URL:                       return CompareStr(a->OriginalUrl().path(), b->OriginalUrl().path());
    case Column::BaseFilename:              return CompareVal(ma.basefilename(), mb.basefilename());
    case Column::Filesize:                  return CompareVal(ma.filesize(), mb.filesize());
    case Column::Filetype:                  return CompareVal(ma.filetype(), mb.filetype());
    case Column::DateModified:              return CompareVal(ma.mtime(), mb.mtime());
    case Column::DateCreated:               return CompareVal(ma.ctime(), mb.ctime());

    case Column::Comment:                   return CompareStr(ma.comment(), mb.comment());
    case Column::Source:                    return CompareVal(ma.source(), mb.source());

    case Column::Rating:                    return CompareVal(ma.rating(), mb.rating());

    case Column::HasCUE:                    return CompareVal(ma.has_cue(), mb.has_cue());

    case Column::EBUR128IntegratedLoudness: return CompareVal(ma.ebur128_integrated_loudness_lufs(), mb.ebur128_integrated_loudness_lufs());
    case Column::EBUR128LoudnessRange:      return CompareVal(ma.ebur128_loudness_range_lu(), mb.ebur128_loudness_range_lu());

    case Column::BPM:                       return CompareVal(ma.bpm(), mb.bpm());
    case Column::Mood:                      return CompareStr(ma.mood(), mb.mood());
    case Column::InitialKey:                return CompareStr(ma.initial_key(), mb.initial_key());

    case Column::Moodbar:
    case Column::ColumnCount:
      break;
  }

  return false;
}

QString Playlist::column_name(const Column column) {

  switch (column) {
    case Column::Title:                     return tr("Title");
    case Column::TitleSort:                 return tr("Title Sort");
    case Column::Artist:                    return tr("Artist");
    case Column::ArtistSort:                return tr("Artist Sort");
    case Column::Album:                     return tr("Album");
    case Column::AlbumSort:                 return tr("Album Sort");
    case Column::Track:                     return tr("Track");
    case Column::Disc:                      return tr("Disc");
    case Column::Length:                    return tr("Length");
    case Column::Year:                      return tr("Year");
    case Column::OriginalYear:              return tr("Original Year");
    case Column::Genre:                     return tr("Genre");
    case Column::AlbumArtist:               return tr("Album Artist");
    case Column::AlbumArtistSort:           return tr("Album Artist Sort");
    case Column::Composer:                  return tr("Composer");
    case Column::ComposerSort:              return tr("Composer Sort");
    case Column::Performer:                 return tr("Performer");
    case Column::PerformerSort:             return tr("Performer Sort");
    case Column::Grouping:                  return tr("Grouping");

    case Column::PlayCount:                 return tr("Play Count");
    case Column::SkipCount:                 return tr("Skip Count");
    case Column::LastPlayed:                return tr("Last Played");

    case Column::Samplerate:                return tr("Sample Rate");
    case Column::Bitdepth:                  return tr("Bit Depth");
    case Column::Bitrate:                   return tr("Bitrate");

    case Column::URL:                       return tr("URL");
    case Column::BaseFilename:              return tr("File Name (without path)");
    case Column::Filesize:                  return tr("File Size");
    case Column::Filetype:                  return tr("File Type");
    case Column::DateModified:              return tr("Date Modified");
    case Column::DateCreated:               return tr("Date Created");

    case Column::Comment:                   return tr("Comment");
    case Column::Source:                    return tr("Source");
    case Column::Moodbar:                   return tr("Moodbar");
    case Column::Rating:                    return tr("Rating");
    case Column::HasCUE:                    return tr("CUE");

    case Column::EBUR128IntegratedLoudness: return tr("Integrated Loudness");
    case Column::EBUR128LoudnessRange:      return tr("Loudness Range");

    case Column::BPM:                       return tr("BPM");
    case Column::Mood:                      return tr("Mood");
    case Column::InitialKey:                return tr("Initial key");

    case Column::ColumnCount:
      break;
  }

  return ""_L1;

}

QString Playlist::abbreviated_column_name(const Column column) {

  const QString &column_name = Playlist::column_name(column);

  switch (column) {
    case Column::Disc:
    case Column::PlayCount:
    case Column::SkipCount:
    case Column::Track:
      return QStringLiteral("%1#").arg(column_name[0]);
    default:
      return column_name;
  }

}

void Playlist::sort(const int column_number, const Qt::SortOrder order) {

  const Column column = static_cast<Column>(column_number);

  sort_column_ = static_cast<Column>(column);
  sort_order_ = order;

  if (ignore_sorting_) return;

  PlaylistItemPtrList new_items(items_);
  PlaylistItemPtrList::iterator begin = new_items.begin();

  if (dynamic_playlist_ && current_item_index_.isValid())
    begin += current_item_index_.row() + 1;

  if (column == Column::Album) {
    // When sorting by album, also take into account discs and tracks.
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column::Track, order, std::placeholders::_1, std::placeholders::_2));
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column::Disc, order, std::placeholders::_1, std::placeholders::_2));
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column::Album, order, std::placeholders::_1, std::placeholders::_2));
  }
  else {
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, column, order, std::placeholders::_1, std::placeholders::_2));
  }

  undo_stack_->push(new PlaylistUndoCommandSortItems(this, column, order, new_items));

}

void Playlist::ReOrderWithoutUndo(const PlaylistItemPtrList &new_items) {

  Q_EMIT layoutAboutToBeChanged();

  PlaylistItemPtrList old_items = items_;
  items_ = new_items;

  QHash<const PlaylistItem*, int> new_rows;
  for (int i = 0; i < new_items.length(); ++i) {
    new_rows[&*new_items[i]] = i;
  }

  const QModelIndexList indexes = persistentIndexList();
  for (const QModelIndex &idx : indexes) {
    const PlaylistItem *item = &*old_items[idx.row()];
    changePersistentIndex(idx, index(new_rows[item], idx.column(), idx.parent()));
  }

  // Update virtual items
  if (ShuffleMode() != PlaylistSequence::ShuffleMode::Off) {
    const QList<int> old_virtual_items = virtual_items_;
    for (int i = 0; i < virtual_items_.count(); ++i) {
      virtual_items_[i] = static_cast<int>(items_.indexOf(old_items[old_virtual_items[i]]));
    }
  }

  // Update current virtual index
  if (current_item_index_.isValid()) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(current_item_index_.row()));
  }
  else {
    current_virtual_index_ = -1;
  }

  Q_EMIT layoutChanged();

  Q_EMIT PlaylistChanged();

  ScheduleSave();

}

void Playlist::Playing() { SetCurrentIsPaused(false); }

void Playlist::Paused() { SetCurrentIsPaused(true); }

void Playlist::Stopped() { SetCurrentIsPaused(false); }

void Playlist::SetCurrentIsPaused(const bool paused) {

  if (paused == current_is_paused_) return;

  current_is_paused_ = paused;

  if (current_item_index_.isValid()) {
    Q_EMIT dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount - 1));
  }

}

void Playlist::ScheduleSaveAsync() {

  if (QThread::currentThread() == thread()) {
    ScheduleSave();
  }
  else {
    QMetaObject::invokeMethod(this, &Playlist::ScheduleSave, Qt::QueuedConnection);
  }

}

void Playlist::ScheduleSave() {

  if (!playlist_backend_ || is_loading_) return;

  timer_save_->start();

}

void Playlist::Save() {

  if (!playlist_backend_ || is_loading_) return;

  playlist_backend_->SavePlaylistAsync(id_, items_, last_played_row(), dynamic_playlist_);

}

void Playlist::Restore() {

  if (!playlist_backend_) return;

  items_.clear();
  virtual_items_.clear();
  ClearCollectionItems();

  cancel_restore_ = false;
  QFuture<PlaylistItemPtrList> future = QtConcurrent::run(&PlaylistBackend::GetPlaylistItems, playlist_backend_, id_);
  QFutureWatcher<PlaylistItemPtrList> *watcher = new QFutureWatcher<PlaylistItemPtrList>();
  QObject::connect(watcher, &QFutureWatcher<PlaylistItemPtrList>::finished, this, &Playlist::ItemsLoaded);
  watcher->setFuture(future);

}

void Playlist::ClearCollectionItems() {

  constexpr int collection_items_size = static_cast<int>(sizeof(collection_items_)) / sizeof(collection_items_[0]);
  for (int i = 0; i < collection_items_size; ++i) {
    collection_items_[i].clear();
  }

}

void Playlist::ItemsLoaded() {

  QFutureWatcher<PlaylistItemPtrList> *watcher = static_cast<QFutureWatcher<PlaylistItemPtrList>*>(sender());
  PlaylistItemPtrList items = watcher->result();
  watcher->deleteLater();

  if (cancel_restore_) return;

  // Backend returns empty elements for collection items which it couldn't match (because they got deleted); we don't need those
  QMutableListIterator<PlaylistItemPtr> it(items);
  while (it.hasNext()) {
    PlaylistItemPtr item = it.next();

    if (item->IsLocalCollectionItem() && item->EffectiveMetadata().url().isEmpty()) {
      it.remove();
    }
  }

  is_loading_ = true;
  InsertItems(items, 0);
  is_loading_ = false;

  const PlaylistBackend::Playlist playlist = playlist_backend_->GetPlaylist(id_);

  // The newly loaded list of items might be shorter than it was before so look out for a bad last_played index
  last_played_item_index_ = playlist.last_played == -1 || playlist.last_played >= rowCount() ? QModelIndex() : index(playlist.last_played);

  if (playlist.dynamic_type == PlaylistGenerator::Type::Query) {
    PlaylistGeneratorPtr gen = PlaylistGenerator::Create(playlist.dynamic_type);
    if (gen) {

      SharedPtr<CollectionBackend> backend = nullptr;
      if (playlist.dynamic_backend == collection_backend_->songs_table()) backend = collection_backend_;

      if (backend) {
        gen->set_collection_backend(collection_backend_);
        gen->Load(playlist.dynamic_data);
        TurnOnDynamicPlaylist(gen);
      }

    }
  }

  Q_EMIT RestoreFinished();

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  bool greyout = s.value(PlaylistSettings::kGreyoutSongsStartup, false).toBool();
  s.endGroup();

  // Should we gray out deleted songs asynchronously on startup?
  if (greyout) {
    InvalidateDeletedSongs();
  }

  Q_EMIT PlaylistLoaded();

}

static bool DescendingIntLessThan(const int a, const int b) { return a > b; }

void Playlist::RemoveItemsWithoutUndo(const QList<int> &indicesIn) {

  // Sort the indices descending because removing elements 'backwards' is easier - indices don't 'move' in the process.
  QList<int> indices = indicesIn;
  std::sort(indices.begin(), indices.end(), DescendingIntLessThan);

  for (int j = 0; j < indices.count(); j++) {
    int beginning = indices[j], end = indices[j];

    // Splits the indices into sequences. For example this: [1, 2, 4], will get split into [1, 2] and [4].
    while (j != indices.count() - 1 && indices[j] == indices[j + 1] + 1) {
      beginning--;
      j++;
    }

    // Remove the current sequence.
    removeRows(beginning, end - beginning + 1);
  }

}

bool Playlist::removeRows(const int row, const int count, const QModelIndex &parent) {

  Q_UNUSED(parent);

  if (row < 0 || row >= items_.size() || row + count > items_.size()) {
    return false;
  }

  if (count > kUndoItemLimit) {
    // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
    RemoveItemsWithoutUndo(row, count);
    undo_stack_->clear();
  }
  else {
    undo_stack_->push(new PlaylistUndoCommandRemoveItems(this, row, count));
  }

  return true;

}

bool Playlist::removeRows(QList<int> &rows) {

  if (rows.isEmpty()) {
    return false;
  }

  // Start from the end to be sure that indices won't 'move' during the removal process
  std::sort(rows.begin(), rows.end(), std::greater<int>());

  QList<int> part;
  while (!rows.isEmpty()) {
    // We're splitting the input list into sequences of consecutive numbers
    part.append(rows.takeFirst());
    while (!rows.isEmpty() && rows.first() == part.last() - 1) {
      part.append(rows.takeFirst());
    }

    // And now we're removing the current sequence
    if (!removeRows(part.last(), static_cast<int>(part.size()))) {
      return false;
    }

    part.clear();
  }

  return true;

}

PlaylistItemPtrList Playlist::RemoveItemsWithoutUndo(const int row, const int count) {

  if (row < 0 || row >= items_.size() || row + count > items_.size()) {
    return PlaylistItemPtrList();
  }

  // Remove items
  beginRemoveRows(QModelIndex(), row, row + count - 1);
  PlaylistItemPtrList items;
  items.reserve(count);
  for (int i = 0; i < count; ++i) {
    PlaylistItemPtr item(items_.takeAt(row));
    items << item;
    const int id = item->EffectiveMetadata().id();
    const int source_id = item->EffectiveMetadata().source_id();
    if (id != -1 && collection_items_[source_id].contains(id, item)) {
      collection_items_[source_id].remove(id, item);
    }
  }

  // Update virtual items
  for (int i = row; i < items_.count() + count; ++i) {
    Q_ASSERT(virtual_items_.count(i) == 1);
    if (i >= row + count) {
      virtual_items_[virtual_items_.indexOf(i)] = i - count;
    }
    else {
      virtual_items_.removeAt(virtual_items_.indexOf(i));
    }
  }

  endRemoveRows();

  Q_ASSERT(items_.count() == virtual_items_.count());

  // Update current virtual index
  if (current_item_index_.isValid()) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(current_item_index_.row()));
  }
  else {
    if (row - 1 > 0 && row - 1 < items_.size()) {
      current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(row - 1));
    }
    else {
      current_virtual_index_ = -1;
    }
  }

  ScheduleSave();

  return items;

}

void Playlist::StopAfter(const int row) {

  const QModelIndex old_stop_after = stop_after_;

  if ((stop_after_.isValid() && stop_after_.row() == row) || row == -1) {
    stop_after_ = QModelIndex();
  }
  else {
    stop_after_ = index(row, 0);
  }

  if (old_stop_after.isValid()) {
    Q_EMIT dataChanged(old_stop_after, old_stop_after.sibling(old_stop_after.row(), ColumnCount - 1));
  }
  if (stop_after_.isValid()) {
    Q_EMIT dataChanged(stop_after_, stop_after_.sibling(stop_after_.row(), ColumnCount - 1));
  }

}

void Playlist::ClearStreamMetadata() {

  if (!current_item() || !current_item_index_.isValid()) return;

  const Song old_metadata = current_item()->EffectiveMetadata();
  current_item()->ClearStreamMetadata();
  const Song &new_metadata = current_item()->EffectiveMetadata();

  RowDataChanged(current_row(), ChangedColumns(old_metadata, new_metadata));

  if (old_metadata.length_nanosec() != new_metadata.length_nanosec()) {
    UpdateScrobblePoint();
  }

}

bool Playlist::stop_after_current() const {

  if (RepeatMode() == PlaylistSequence::RepeatMode::OneByOne) {
    return true;
  }

  return stop_after_.isValid() && current_item_index_.isValid() && stop_after_.row() == current_item_index_.row();

}

PlaylistItemPtr Playlist::current_item() const {

  // QList[] runs in constant time, so no need to cache current_item
  if (current_item_index_.isValid() && current_item_index_.row() <= items_.length()) {
    return items_[current_item_index_.row()];
  }

  return PlaylistItemPtr();

}

PlaylistItem::Options Playlist::current_item_options() const {
  if (!current_item()) return PlaylistItem::Option::Default;
  return current_item()->options();
}

Song Playlist::current_item_metadata() const {
  if (!current_item()) return Song();
  return current_item()->EffectiveMetadata();
}

void Playlist::Clear() {

  // If loading songs from session restore async, don't insert them
  cancel_restore_ = true;

  const int count = static_cast<int>(items_.count());

  if (count > kUndoItemLimit) {
    // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
    RemoveItemsWithoutUndo(0, count);
    undo_stack_->clear();
  }
  else {
    undo_stack_->push(new PlaylistUndoCommandRemoveItems(this, 0, count));
  }

  TurnOffDynamicPlaylist();

  ScheduleSave();

}

void Playlist::RepopulateDynamicPlaylist() {

  if (!dynamic_playlist_) return;

  RemoveItemsNotInQueue();
  InsertSmartPlaylist(dynamic_playlist_);

}

void Playlist::ExpandDynamicPlaylist() {

  if (!dynamic_playlist_) return;

  InsertDynamicItems(5);

}

void Playlist::RemoveItemsNotInQueue() {

  if (queue_->is_empty() && !current_item_index_.isValid()) {
    RemoveItemsWithoutUndo(0, static_cast<int>(items_.count()));
    return;
  }

  int start = 0;
  Q_FOREVER {
    // Find a place to start - first row that isn't in the queue
    Q_FOREVER {
      if (start >= rowCount()) return;
      if (!queue_->ContainsSourceRow(start) && current_row() != start) break;
      start++;
    }

    // Figure out how many rows to remove - keep going until we find a row that is in the queue
    int count = 1;
    Q_FOREVER {
      if (start + count >= rowCount()) break;
      if (queue_->ContainsSourceRow(start + count) || current_row() == start + count) break;
      count++;
    }

    RemoveItemsWithoutUndo(start, count);
    start++;
  }

}

void Playlist::ReloadItems(const QList<int> &rows) {

  for (const int row : rows) {
    const PlaylistItemPtr item = item_at(row);
    const QPersistentModelIndex idx = index(row, 0);
    if (idx.isValid()) {
      ItemReload(idx, item->EffectiveMetadata(), false);
    }
  }

}

void Playlist::Shuffle() {

  PlaylistItemPtrList new_items(items_);

  int begin = 0;
  if (current_item_index_.isValid()) {
    if (new_items[0] != new_items[current_item_index_.row()]) {
      std::swap(new_items[0], new_items[current_item_index_.row()]);
    }
    begin = 1;
  }

  if (dynamic_playlist_ && current_item_index_.isValid()) {
    begin += current_item_index_.row() + 1;
  }

  const int count = static_cast<int>(items_.count());
  for (int i = begin; i < count; ++i) {
    const int new_pos = i + (rand() % (count - i));

    std::swap(new_items[i], new_items[new_pos]);
  }

  undo_stack_->push(new PlaylistUndoCommandShuffleItems(this, new_items));

}

namespace {

bool AlbumShuffleComparator(const QMap<QString, int> &album_key_positions, const QMap<int, QString> &album_keys, const int left, const int right) {

  const int left_pos = album_key_positions[album_keys[left]];
  const int right_pos = album_key_positions[album_keys[right]];

  if (left_pos == right_pos) return left < right;
  return left_pos < right_pos;

}

}  // namespace

void Playlist::ReshuffleIndices() {

  const PlaylistSequence::ShuffleMode shuffle_mode = ShuffleMode();
  switch (shuffle_mode) {
    case PlaylistSequence::ShuffleMode::Off:{
      // No shuffling - sort the virtual item list normally.
      std::sort(virtual_items_.begin(), virtual_items_.end());
      break;
    }

    case PlaylistSequence::ShuffleMode::All:
    case PlaylistSequence::ShuffleMode::InsideAlbum:{
      std::random_device rd;
      std::shuffle(virtual_items_.begin(), virtual_items_.end(), std::mt19937(rd()));
      break;
    }

    case PlaylistSequence::ShuffleMode::Albums:{
      QMap<int, QString> album_keys;  // real index -> key
      QSet<QString> album_key_set;    // unique keys

      // Find all the unique albums in the playlist
      for (QList<int>::const_iterator it = virtual_items_.constBegin(); it != virtual_items_.constEnd(); ++it) {
        const int index = *it;
        const QString key = items_[index]->EffectiveMetadata().AlbumKey();
        album_keys[index] = key;
        album_key_set << key;
      }

      // Shuffle them
      QStringList shuffled_album_keys = album_key_set.values();
      std::random_device rd;
      std::shuffle(shuffled_album_keys.begin(), shuffled_album_keys.end(), std::mt19937(rd()));

      // If the user is currently playing a song, force its album to be first
      if (current_row() != -1) {
        const QString key = items_[current_row()]->EffectiveMetadata().AlbumKey();
        const qint64 pos = shuffled_album_keys.indexOf(key);
        if (pos >= 1) {
          std::swap(shuffled_album_keys[0], shuffled_album_keys[pos]);
        }
      }

      // Create album key -> position mapping for fast lookup
      QMap<QString, int> album_key_positions;
      for (int i = 0; i < shuffled_album_keys.count(); ++i) {
        album_key_positions[shuffled_album_keys[i]] = i;
      }

      // Sort the virtual items
      std::stable_sort(virtual_items_.begin(), virtual_items_.end(), std::bind(AlbumShuffleComparator, album_key_positions, album_keys, std::placeholders::_1, std::placeholders::_2));

      break;
    }
  }

  // Update current virtual index
  if (current_item_index_.isValid()) {
    current_virtual_index_ = static_cast<int>(virtual_items_.indexOf(current_item_index_.row()));
  }
  else {
    current_virtual_index_ = -1;
  }

}

void Playlist::set_sequence(PlaylistSequence *v) {

  playlist_sequence_ = v;
  QObject::connect(v, &PlaylistSequence::ShuffleModeChanged, this, &Playlist::ShuffleModeChanged);

  ShuffleModeChanged(ShuffleMode());

}

PlaylistFilter *Playlist::filter() const { return filter_; }

SongList Playlist::GetAllSongs() const {

  SongList songs;
  songs.reserve(items_.count());
  for (PlaylistItemPtr item : items_) {  // clazy:exclude=range-loop-reference
    songs << item->EffectiveMetadata();
  }
  return songs;

}

PlaylistItemPtrList Playlist::GetAllItems() const { return items_; }

quint64 Playlist::GetTotalLength() const {

  quint64 total_length = 0;
  for (PlaylistItemPtr item : items_) {  // clazy:exclude=range-loop-reference
    qint64 length = item->EffectiveMetadata().length_nanosec();
    if (length > 0) total_length += length;
  }

  return total_length;

}

PlaylistItemPtrList Playlist::collection_items(const Song::Source source, const int song_id) const {
  return collection_items_[static_cast<int>(source)].values(song_id);
}

void Playlist::TracksAboutToBeDequeued(const QModelIndex &idx, const int begin, const int end) {

  Q_UNUSED(idx)

  for (int i = begin; i <= end; ++i) {
    temp_dequeue_change_indexes_ << queue_->mapToSource(queue_->index(i, static_cast<int>(Column::Title)));
  }

}

void Playlist::TracksDequeued() {

  for (const QModelIndex &idx : std::as_const(temp_dequeue_change_indexes_)) {
    Q_EMIT dataChanged(idx, idx);
  }
  temp_dequeue_change_indexes_.clear();
  Q_EMIT QueueChanged();

}

void Playlist::TracksEnqueued(const QModelIndex &parent_idx, const int begin, const int end) {

  Q_UNUSED(parent_idx)

  const QModelIndex &b = queue_->mapToSource(queue_->index(begin, static_cast<int>(Column::Title)));
  const QModelIndex &e = queue_->mapToSource(queue_->index(end, static_cast<int>(Column::Title)));
  Q_EMIT dataChanged(b, e);

}

void Playlist::QueueLayoutChanged() {

  for (int i = 0; i < queue_->rowCount(); ++i) {
    const QModelIndex idx = queue_->mapToSource(queue_->index(i, static_cast<int>(Column::Title)));
    Q_EMIT dataChanged(idx, idx);
  }

}

Playlist::Columns Playlist::ChangedColumns(const Song &metadata1, const Song &metadata2) {

  Columns columns;

  if (metadata1.title() != metadata2.title()) {
    columns << Column::Title;
  }
  if (metadata1.titlesort() != metadata2.titlesort()) {
    columns << Column::TitleSort;
  }
  if (metadata1.artist() != metadata2.artist()) {
    columns << Column::Artist;
  }
  if (metadata1.artistsort() != metadata2.artistsort()) {
    columns << Column::ArtistSort;
  }
  if (metadata1.album() != metadata2.album()) {
    columns << Column::Album;
  }
  if (metadata1.albumsort() != metadata2.albumsort()) {
    columns << Column::AlbumSort;
  }
  if (metadata1.effective_albumartist() != metadata2.effective_albumartist()) {
    columns << Column::AlbumArtist;
  }
  if (metadata1.albumartistsort() != metadata2.albumartistsort()) {
    columns << Column::AlbumArtistSort;
  }
  if (metadata1.performer() != metadata2.performer()) {
    columns << Column::Performer;
  }
  if (metadata1.performersort() != metadata2.performersort()) {
    columns << Column::PerformerSort;
  }
  if (metadata1.composer() != metadata2.composer()) {
    columns << Column::Composer;
  }
  if (metadata1.composersort() != metadata2.composersort()) {
    columns << Column::ComposerSort;
  }
  if (metadata1.year() != metadata2.year()) {
    columns << Column::Year;
  }
  if (metadata1.originalyear() != metadata2.originalyear()) {
    columns << Column::OriginalYear;
  }
  if (metadata1.track() != metadata2.track()) {
    columns << Column::Track;
  }
  if (metadata1.disc() != metadata2.disc()) {
    columns << Column::Disc;
  }
  if (metadata1.length_nanosec() != metadata2.length_nanosec()) {
    columns << Column::Length;
  }
  if (metadata1.genre() != metadata2.genre()) {
    columns << Column::Genre;
  }
  if (metadata1.samplerate() != metadata2.samplerate()) {
    columns << Column::Samplerate;
  }
  if (metadata1.bitdepth() != metadata2.bitdepth()) {
    columns << Column::Bitdepth;
  }
  if (metadata1.bitrate() != metadata2.bitrate()) {
    columns << Column::Bitrate;
  }
  if (metadata1.effective_url() != metadata2.effective_url()) {
    qLog(Debug) << "URL is changed for" << metadata1.PrettyTitleWithArtist();
    columns << Column::URL;
    columns << Column::BaseFilename;
  }
  if (metadata1.filesize() != metadata2.filesize()) {
    columns << Column::Filesize;
  }
  if (metadata1.filetype() != metadata2.filetype()) {
    columns << Column::Filetype;
  }
  if (metadata1.ctime() != metadata2.ctime()) {
    columns << Column::DateCreated;
  }
  if (metadata1.mtime() != metadata2.mtime()) {
    columns << Column::DateModified;
  }
  if (metadata1.playcount() != metadata2.playcount()) {
    columns << Column::PlayCount;
  }
  if (metadata1.skipcount() != metadata2.skipcount()) {
    columns << Column::SkipCount;
  }
  if (metadata1.lastplayed() != metadata2.lastplayed()) {
    columns << Column::LastPlayed;
  }
  if (metadata1.comment() != metadata2.comment()) {
    columns << Column::Comment;
  }
  if (metadata1.grouping() != metadata2.grouping()) {
    columns << Column::Grouping;
  }
  if (metadata1.source() != metadata2.source()) {
    columns << Column::Source;
  }
  if (metadata1.rating() != metadata2.rating()) {
    columns << Column::Rating;
  }
  if (metadata1.has_cue() != metadata2.has_cue()) {
    columns << Column::HasCUE;
  }
  if (metadata1.ebur128_integrated_loudness_lufs() != metadata2.ebur128_integrated_loudness_lufs()) {
    columns << Column::EBUR128IntegratedLoudness;
  }
  if (metadata1.ebur128_loudness_range_lu() != metadata2.ebur128_loudness_range_lu()) {
    columns << Column::EBUR128LoudnessRange;
  }
  if (metadata1.bpm() != metadata2.bpm()) {
    columns << Column::BPM;
  }
  if (metadata1.mood() != metadata2.mood()) {
    columns << Column::Mood;
  }
  if (metadata1.initial_key() != metadata2.initial_key()) {
    columns << Column::InitialKey;
  }

  return columns;

}

bool Playlist::MinorMetadataChange(const Song &old_metadata, const Song &new_metadata) {

  return new_metadata.title() == old_metadata.title() &&
         new_metadata.albumartist() == old_metadata.albumartist() &&
         new_metadata.artist() == old_metadata.artist() &&
         new_metadata.album() == old_metadata.album();

}

void Playlist::UpdateItemMetadata(PlaylistItemPtr item, const Song &new_metadata, const bool stream_metadata_update) {

  if (!items_.contains(item)) {
    return;
  }

  for (int row = static_cast<int>(items_.indexOf(item, 0)); row != -1; row = static_cast<int>(items_.indexOf(item, row + 1))) {
    UpdateItemMetadata(row, item, new_metadata, stream_metadata_update);
  }

}

void Playlist::UpdateItemMetadata(const int row, PlaylistItemPtr item, const Song &new_metadata, const bool stream_metadata_update) {

  if (new_metadata.IsEqual(stream_metadata_update ? item->EffectiveMetadata() : item->OriginalMetadata())) return;

  const Song old_metadata = item->EffectiveMetadata();
  const Columns changed_columns = ChangedColumns(old_metadata, new_metadata);

  if (stream_metadata_update) {
    item->SetStreamMetadata(new_metadata);
  }
  else {
    item->SetOriginalMetadata(new_metadata);
    if (item->HasStreamMetadata()) {
      item->UpdateStreamMetadata(new_metadata);
    }
  }

  if (!changed_columns.isEmpty()) {
    RowDataChanged(row, changed_columns);
  }

  if (row == current_row()) {
    InformOfCurrentSongChange(MinorMetadataChange(old_metadata, new_metadata));
    if (new_metadata.length_nanosec() != old_metadata.length_nanosec()) {
      UpdateScrobblePoint();
    }
  }

}

void Playlist::RowDataChanged(const int row, const Columns &columns) {

  if (columns.count() > 5) {
    const QModelIndex idx_column_first = index(row, 0);
    const QModelIndex idx_column_last = index(row, ColumnCount - 1);
    if (idx_column_first.isValid() && idx_column_last.isValid()) {
      Q_EMIT dataChanged(index(row, 0), index(row, ColumnCount - 1));
    }
  }
  else {
    for (const Column &column : columns) {
      const QModelIndex idx = index(row, static_cast<int>(column));
      if (idx.isValid()) {
        Q_EMIT dataChanged(idx, idx);
      }
    }
  }

}

void Playlist::InformOfCurrentSongChange(const bool minor) {

  const Song &metadata = current_item_metadata();
  if (!metadata.is_valid()) {
    return;
  }

  if (minor) {
    Q_EMIT CurrentSongMetadataChanged(metadata);
  }
  else {
    Q_EMIT CurrentSongChanged(metadata);
  }

}

void Playlist::InvalidateDeletedSongs() {

  QList<int> invalidated_rows;

  for (int row = 0; row < items_.count(); ++row) {
    PlaylistItemPtr item = items_.value(row);
    const Song song = item->EffectiveMetadata();

    if (song.url().isValid() && song.url().isLocalFile()) {
      const bool exists = QFile::exists(song.url().toLocalFile());

      if (!exists && !item->HasForegroundColor(kInvalidSongPriority)) {
        // Gray out the song if it's not there
        item->SetForegroundColor(kInvalidSongPriority, kInvalidSongColor);
        invalidated_rows.append(row);  // clazy:exclude=reserve-candidates
      }
      else if (exists && item->HasForegroundColor(kInvalidSongPriority)) {
        item->RemoveForegroundColor(kInvalidSongPriority);
        invalidated_rows.append(row);  // clazy:exclude=reserve-candidates
      }
    }
  }

  if (!invalidated_rows.isEmpty()) {
    ReloadItems(invalidated_rows);
  }

}

void Playlist::RemoveDeletedSongs() {

  QList<int> rows_to_remove;

  for (int row = 0; row < items_.count(); ++row) {
    const PlaylistItemPtr item = items_.value(row);
    const Song song = item->EffectiveMetadata();

    if (song.url().isLocalFile() && !QFile::exists(song.url().toLocalFile())) {
      rows_to_remove.append(row);  // clazy:exclude=reserve-candidates
    }
  }

  removeRows(rows_to_remove);

}

namespace {

struct SongSimilarHash {
  size_t operator()(const Song &song) const {
    return HashSimilar(song);
  }
};

struct SongSimilarEqual {
  size_t operator()(const Song &song1, const Song &song2) const {
    return song1.IsSimilar(song2);
  }
};

}  // namespace

void Playlist::RemoveDuplicateSongs() {

  QList<int> rows_to_remove;
  std::unordered_map<Song, int, SongSimilarHash, SongSimilarEqual> unique_songs;

  for (int row = 0; row < items_.count(); ++row) {
    const PlaylistItemPtr item = items_.value(row);
    const Song &song = item->EffectiveMetadata();

    bool found_duplicate = false;

    auto uniq_song_it = unique_songs.find(song);
    if (uniq_song_it != unique_songs.end()) {
      const Song &uniq_song = uniq_song_it->first;

      if (song.bitrate() > uniq_song.bitrate()) {
        rows_to_remove.append(unique_songs[uniq_song]);  // clazy:exclude=reserve-candidates
        unique_songs.erase(uniq_song);
        unique_songs.insert(std::make_pair(song, row));
      }
      else {
        rows_to_remove.append(row);  // clazy:exclude=reserve-candidates
      }
      found_duplicate = true;
    }

    if (!found_duplicate) {
      unique_songs.insert(std::make_pair(song, row));
    }
  }

  removeRows(rows_to_remove);

}

void Playlist::RemoveUnavailableSongs() {

  QList<int> rows_to_remove;
  for (int row = 0; row < items_.count(); ++row) {
    const PlaylistItemPtr item = items_.value(row);
    const Song &song = item->EffectiveMetadata();

    // Check only local files
    if (song.url().isLocalFile() && !QFile::exists(song.url().toLocalFile())) {
      rows_to_remove.append(row);  // clazy:exclude=reserve-candidates
    }
  }

  removeRows(rows_to_remove);

}

bool Playlist::ApplyValidityOnCurrentSong(const QUrl &url, const bool valid) {

  const PlaylistItemPtr current = current_item();

  if (current) {
    const Song current_song = current->EffectiveMetadata();

    // If validity has changed, reload the item
    if (current_song.source() == Song::Source::LocalFile || current_song.source() == Song::Source::Collection) {
      if (current_song.url() == url && current_song.url().isLocalFile() && current_song.is_valid() != QFile::exists(current_song.url().toLocalFile())) {
        ReloadItems(QList<int>() << current_row());
      }
    }

    // Gray out the song if it's now broken; otherwise undo the gray color
    if (valid) {
      current->RemoveForegroundColor(kInvalidSongPriority);
    }
    else {
      current->SetForegroundColor(kInvalidSongPriority, kInvalidSongColor);
    }
  }

  return static_cast<bool>(current);

}

void Playlist::SetColumnAlignment(const ColumnAlignmentMap &alignment) {
  column_alignments_ = alignment;
}

void Playlist::SkipTracks(const QModelIndexList &source_indexes) {

  for (const QModelIndex &source_index : source_indexes) {
    PlaylistItemPtr track_to_skip = item_at(source_index.row());
    track_to_skip->SetShouldSkip(!((track_to_skip)->GetShouldSkip()));
    Q_EMIT dataChanged(source_index, source_index);
  }

}

void Playlist::UpdateScrobblePoint(const qint64 seek_point_nanosec) {

  const qint64 length = current_item_metadata().length_nanosec();

  if (seek_point_nanosec <= 0) {
    if (length == 0) {
      scrobble_point_ = kMaxScrobblePointNsecs;
    }
    else {
      scrobble_point_ = qBound(kMinScrobblePointNsecs, length / 2, kMaxScrobblePointNsecs);
    }
  }
  else {
    if (length <= 0) {
      scrobble_point_ = seek_point_nanosec + kMaxScrobblePointNsecs;
    }
    else {
      scrobble_point_ = qBound(seek_point_nanosec + kMinScrobblePointNsecs, seek_point_nanosec + (length / 2), seek_point_nanosec + kMaxScrobblePointNsecs);
    }
  }

  scrobbled_ = false;

}

void Playlist::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  // Update art_manual for local songs that are not in the collection.
  if (((result.type == AlbumCoverLoaderResult::Type::Manual && result.album_cover.cover_url.isLocalFile()) || result.type == AlbumCoverLoaderResult::Type::Unset) && (song.source() == Song::Source::LocalFile || song.source() == Song::Source::CDDA || song.source() == Song::Source::Device)) {
    PlaylistItemPtr item = current_item();
    if (item && item->EffectiveMetadata() == song && (!item->EffectiveMetadata().art_manual_is_valid() || (result.type == AlbumCoverLoaderResult::Type::Unset && !item->EffectiveMetadata().art_unset()))) {
      qLog(Debug) << "Updating art manual for local song" << song.title() << song.album() << song.title() << "to" << result.album_cover.cover_url << "in playlist.";
      item->SetArtManual(result.album_cover.cover_url);
      ScheduleSaveAsync();
    }
  }

}

int Playlist::dynamic_history_length() const {
  return dynamic_playlist_ && last_played_item_index_.isValid() ? last_played_item_index_.row() + 1 : 0;
}

void Playlist::TurnOffDynamicPlaylist() {

  dynamic_playlist_.reset();

  if (playlist_sequence_) {
    ShuffleModeChanged(ShuffleMode());
  }

  Q_EMIT DynamicModeChanged(false);

  ScheduleSave();

}

void Playlist::RateSong(const QModelIndex &idx, const float rating) {

  if (has_item_at(idx.row())) {
    const PlaylistItemPtr item = item_at(idx.row());
    if (item && item->IsLocalCollectionItem() && item->EffectiveMetadata().id() != -1) {
      collection_backend_->UpdateSongRatingAsync(item->EffectiveMetadata().id(), rating);
    }
  }

}

void Playlist::RateSongs(const QModelIndexList &index_list, const float rating) {

  QList<int> id_list;
  for (const QModelIndex &idx : index_list) {
    const int row = idx.row();
    if (has_item_at(row)) {
      const PlaylistItemPtr item = item_at(row);
      if (item && item->IsLocalCollectionItem() && item->EffectiveMetadata().id() != -1) {
        id_list << item->EffectiveMetadata().id();  // clazy:exclude=reserve-candidates
      }
    }
  }
  collection_backend_->UpdateSongsRatingAsync(id_list, rating);

}
