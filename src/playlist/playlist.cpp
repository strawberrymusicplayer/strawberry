/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <random>

#include <QtGlobal>
#include <QObject>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFuture>
#include <QIODevice>
#include <QDataStream>
#include <QBuffer>
#include <QFile>
#include <QList>
#include <QMap>
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
#include <QtDebug>

#include "core/application.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/mimedata.h"
#include "core/tagreaderclient.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectionplaylistitem.h"
#include "covermanager/albumcoverloader.h"
#include "queue/queue.h"
#include "playlist.h"
#include "playlistitem.h"
#include "playlistview.h"
#include "playlistsequence.h"
#include "playlistbackend.h"
#include "playlistfilter.h"
#include "playlistitemmimedata.h"
#include "playlistundocommands.h"
#include "songloaderinserter.h"
#include "songmimedata.h"
#include "songplaylistitem.h"
#include "tagreadermessages.pb.h"

#include "smartplaylists/playlistgenerator.h"
#include "smartplaylists/playlistgeneratorinserter.h"
#include "smartplaylists/playlistgeneratormimedata.h"

#include "internet/internetplaylistitem.h"
#include "internet/internetsongmimedata.h"

using std::placeholders::_1;
using std::placeholders::_2;

const char *Playlist::kCddaMimeType = "x-content/audio-cdda";
const char *Playlist::kRowsMimetype = "application/x-strawberry-playlist-rows";
const char *Playlist::kPlayNowMimetype = "application/x-strawberry-play-now";

const int Playlist::kInvalidSongPriority = 200;
const QRgb Playlist::kInvalidSongColor = qRgb(0xC0, 0xC0, 0xC0);

const int Playlist::kDynamicHistoryPriority = 100;
const QRgb Playlist::kDynamicHistoryColor = qRgb(0x80, 0x80, 0x80);

const char *Playlist::kSettingsGroup = "Playlist";

const char *Playlist::kPathType = "path_type";
const char *Playlist::kWriteMetadata = "write_metadata";

const int Playlist::kUndoStackSize = 20;
const int Playlist::kUndoItemLimit = 500;

const qint64 Playlist::kMinScrobblePointNsecs = 31ll * kNsecPerSec;
const qint64 Playlist::kMaxScrobblePointNsecs = 240ll * kNsecPerSec;

Playlist::Playlist(PlaylistBackend *backend, TaskManager *task_manager, CollectionBackend *collection, const int id, const QString &special_type, const bool favorite, QObject *parent)
    : QAbstractListModel(parent),
      is_loading_(false),
      proxy_(new PlaylistFilter(this)),
      queue_(new Queue(this)),
      backend_(backend),
      task_manager_(task_manager),
      collection_(collection),
      id_(id),
      favorite_(favorite),
      current_is_paused_(false),
      current_virtual_index_(-1),
      is_shuffled_(false),
      playlist_sequence_(nullptr),
      ignore_sorting_(false),
      undo_stack_(new QUndoStack(this)),
      special_type_(special_type),
      cancel_restore_(false),
      scrobbled_(false),
      scrobble_point_(-1),
      editing_(-1),
      auto_sort_(false),
      sort_column_(Column_Title),
      sort_order_(Qt::AscendingOrder)
      {

  undo_stack_->setUndoLimit(kUndoStackSize);

  connect(this, SIGNAL(rowsInserted(QModelIndex, int, int)), SIGNAL(PlaylistChanged()));
  connect(this, SIGNAL(rowsRemoved(QModelIndex, int, int)), SIGNAL(PlaylistChanged()));

  Restore();

  proxy_->setSourceModel(this);
  queue_->setSourceModel(this);

  connect(queue_, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)), SLOT(TracksAboutToBeDequeued(QModelIndex, int, int)));
  connect(queue_, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(TracksDequeued()));

  connect(queue_, SIGNAL(rowsInserted(QModelIndex, int, int)), SLOT(TracksEnqueued(QModelIndex, int, int)));

  connect(queue_, SIGNAL(layoutChanged()), SLOT(QueueLayoutChanged()));

  column_alignments_ = PlaylistView::DefaultColumnAlignment();

}

Playlist::~Playlist() {
  items_.clear();
  collection_items_by_id_.clear();
}

template <typename T>
void Playlist::InsertSongItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemList items;

  for (const Song &song : songs) {
    items << PlaylistItemPtr(new T(song));
  }

  InsertItems(items, pos, play_now, enqueue, enqueue_next);

}

QVariant Playlist::headerData(int section, Qt::Orientation, int role) const {

  if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return QVariant();

  const QString name = column_name(static_cast<Playlist::Column>(section));
  if (!name.isEmpty()) return name;

  return QVariant();

}

bool Playlist::column_is_editable(Playlist::Column column) {

  switch (column) {
    case Column_Title:
    case Column_Artist:
    case Column_Album:
    case Column_AlbumArtist:
    case Column_Composer:
    case Column_Performer:
    case Column_Grouping:
    case Column_Track:
    case Column_Disc:
    case Column_Year:
    case Column_Genre:
    case Column_Comment:
      return true;
    default:
      break;
  }
  return false;

}

bool Playlist::set_column_value(Song &song, Playlist::Column column, const QVariant &value) {

  if (!song.IsEditable()) return false;

  switch (column) {
    case Column_Title:
      song.set_title(value.toString());
      break;
    case Column_Artist:
      song.set_artist(value.toString());
      break;
    case Column_Album:
      song.set_album(value.toString());
      break;
    case Column_AlbumArtist:
      song.set_albumartist(value.toString());
      break;
    case Column_Composer:
      song.set_composer(value.toString());
      break;
    case Column_Performer:
      song.set_performer(value.toString());
      break;
    case Column_Grouping:
      song.set_grouping(value.toString());
      break;
    case Column_Track:
      song.set_track(value.toInt());
      break;
    case Column_Disc:
      song.set_disc(value.toInt());
      break;
    case Column_Year:
      song.set_year(value.toInt());
      break;
    case Column_Genre:
      song.set_genre(value.toString());
      break;
    case Column_Comment:
      song.set_comment(value.toString());
      break;
    default:
      break;
  }

  return true;

}

QVariant Playlist::data(const QModelIndex &idx, int role) const {

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
      return idx.column() == Column_Rating && items_[idx.row()]->IsLocalCollectionItem() && items_[idx.row()]->Metadata().id() != -1;

    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::DisplayRole: {
      PlaylistItemPtr item = items_[idx.row()];
      Song song = item->Metadata();

      // Don't forget to change Playlist::CompareItems when adding new columns
      switch (idx.column()) {
        case Column_Title:              return song.PrettyTitle();
        case Column_Artist:             return song.artist();
        case Column_Album:              return song.album();
        case Column_Length:             return song.length_nanosec();
        case Column_Track:              return song.track();
        case Column_Disc:               return song.disc();
        case Column_Year:               return song.year();
        case Column_OriginalYear:       return song.effective_originalyear();
        case Column_Genre:              return song.genre();
        case Column_AlbumArtist:        return song.playlist_albumartist();
        case Column_Composer:           return song.composer();
        case Column_Performer:          return song.performer();
        case Column_Grouping:           return song.grouping();

        case Column_PlayCount:          return song.playcount();
        case Column_SkipCount:          return song.skipcount();
        case Column_LastPlayed:         return song.lastplayed();

        case Column_Samplerate:         return song.samplerate();
        case Column_Bitdepth:           return song.bitdepth();
        case Column_Bitrate:            return song.bitrate();

        case Column_Filename:           return song.effective_stream_url();
        case Column_BaseFilename:       return song.basefilename();
        case Column_Filesize:           return song.filesize();
        case Column_Filetype:           return song.filetype();
        case Column_DateModified:       return song.mtime();
        case Column_DateCreated:        return song.ctime();

        case Column_Comment:
          if (role == Qt::DisplayRole)  return song.comment().simplified();
          return song.comment();

        case Column_Source:             return song.source();

        case Column_Rating:             return song.rating();

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
      if (idx.row() < dynamic_history_length()) {
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
  emit dataChanged(idx.sibling(idx.row(), Column_Mood), idx.sibling(idx.row(), Column_Mood));
}
#endif

bool Playlist::setData(const QModelIndex &idx, const QVariant &value, int role) {

  Q_UNUSED(role);

  int row = idx.row();
  PlaylistItemPtr item = item_at(row);
  Song song = item->OriginalMetadata();

  if (idx.data() == value) return false;

  if (!set_column_value(song, static_cast<Column>(idx.column()), value)) return false;

  TagReaderReply *reply = TagReaderClient::Instance()->SaveFile(song.url().toLocalFile(), song);
  NewClosure(reply, SIGNAL(Finished(bool)), this, SLOT(SongSaveComplete(TagReaderReply*, QPersistentModelIndex)), reply, QPersistentModelIndex(idx));

  return true;

}

void Playlist::SongSaveComplete(TagReaderReply *reply, const QPersistentModelIndex &idx) {

  if (reply->is_successful() && idx.isValid()) {
    if (reply->message().save_file_response().success()) {
      PlaylistItemPtr item = item_at(idx.row());
      if (item) {
        QFuture<void> future = item->BackgroundReload();
        NewClosure(future, this, SLOT(ItemReloadComplete(QPersistentModelIndex)), idx);
      }
    }
    else {
      emit Error(tr("An error occurred writing metadata to '%1'").arg(QString::fromStdString(reply->request_message().save_file_request().filename())));
    }
  }
  reply->deleteLater();

}

void Playlist::ItemReloadComplete(const QPersistentModelIndex &idx) {

  if (idx.isValid()) {

    PlaylistItemPtr item = item_at(idx.row());
    if (item && item->HasTemporaryMetadata()) {  // Update temporary metadata.
      item->UpdateTemporaryMetadata(item->OriginalMetadata());
    }

    emit dataChanged(idx, idx);
    emit EditingFinished(idx);
  }

}

int Playlist::current_row() const {
  return current_item_index_.isValid() ? current_item_index_.row() : -1;
}

const QModelIndex Playlist::current_index() const {
  return current_item_index_;
}

int Playlist::last_played_row() const {
  return last_played_item_index_.isValid() ? last_played_item_index_.row() : -1;
}

void Playlist::ShuffleModeChanged(const PlaylistSequence::ShuffleMode mode) {
  is_shuffled_ = (mode != PlaylistSequence::Shuffle_Off);
  ReshuffleIndices();
}

bool Playlist::FilterContainsVirtualIndex(const int i) const {
  if (i < 0 || i >= virtual_items_.count()) return false;

  return proxy_->filterAcceptsRow(virtual_items_[i], QModelIndex());
}

int Playlist::NextVirtualIndex(int i, const bool ignore_repeat_track) const {

  PlaylistSequence::RepeatMode repeat_mode = playlist_sequence_->repeat_mode();
  PlaylistSequence::ShuffleMode shuffle_mode = playlist_sequence_->shuffle_mode();
  bool album_only = repeat_mode == PlaylistSequence::Repeat_Album || shuffle_mode == PlaylistSequence::Shuffle_InsideAlbum;

  // This one's easy - if we have to repeat the current track then just return i
  if (repeat_mode == PlaylistSequence::Repeat_Track && !ignore_repeat_track) {
    if (!FilterContainsVirtualIndex(i))
      return virtual_items_.count();  // It's not in the filter any more
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
  Song last_song = current_item_metadata();
  for (int j = i + 1; j < virtual_items_.count(); ++j) {
    if (item_at(virtual_items_[j])->GetShouldSkip()) {
      continue;
    }
    Song this_song = item_at(virtual_items_[j])->Metadata();
    if (((last_song.is_compilation() && this_song.is_compilation()) ||
         last_song.effective_albumartist() == this_song.effective_albumartist()) &&
        last_song.album() == this_song.album() &&
        FilterContainsVirtualIndex(j)) {
      return j;  // Found one
    }
  }

  // Couldn't find one - return past the end of the list
  return virtual_items_.count();

}

int Playlist::PreviousVirtualIndex(int i, const bool ignore_repeat_track) const {

  PlaylistSequence::RepeatMode repeat_mode = playlist_sequence_->repeat_mode();
  PlaylistSequence::ShuffleMode shuffle_mode = playlist_sequence_->shuffle_mode();
  bool album_only = repeat_mode == PlaylistSequence::Repeat_Album || shuffle_mode == PlaylistSequence::Shuffle_InsideAlbum;

  // This one's easy - if we have to repeat the current track then just return i
  if (repeat_mode == PlaylistSequence::Repeat_Track && !ignore_repeat_track) {
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
    Song this_song = item_at(virtual_items_[j])->Metadata();
    if (((last_song.is_compilation() && this_song.is_compilation()) || last_song.artist() == this_song.artist()) && last_song.album() == this_song.album() && FilterContainsVirtualIndex(j)) {
      return j;  // Found one
    }
  }

  // Couldn't find one - return before the start of the list
  return -1;

}

int Playlist::next_row(const bool ignore_repeat_track) const {

  // Any queued items take priority
  if (!queue_->is_empty()) {
    return queue_->PeekNext();
  }

  int next_virtual_index = NextVirtualIndex(current_virtual_index_, ignore_repeat_track);
  if (next_virtual_index >= virtual_items_.count()) {
    // We've gone off the end of the playlist.

    switch (playlist_sequence_->repeat_mode()) {
      case PlaylistSequence::Repeat_Off:
      case PlaylistSequence::Repeat_Intro:
        return -1;
      case PlaylistSequence::Repeat_Track:
        next_virtual_index = current_virtual_index_;
        break;

      default:
        next_virtual_index = NextVirtualIndex(-1, ignore_repeat_track);
        break;
    }
  }

  // Still off the end?  Then just give up
  if (next_virtual_index < 0 || next_virtual_index >= virtual_items_.count()) return -1;

  return virtual_items_[next_virtual_index];

}

int Playlist::previous_row(const bool ignore_repeat_track) const {

  int prev_virtual_index = PreviousVirtualIndex(current_virtual_index_,ignore_repeat_track);

  if (prev_virtual_index < 0) {
    // We've gone off the beginning of the playlist.

    switch (playlist_sequence_->repeat_mode()) {
      case PlaylistSequence::Repeat_Off:
        return -1;
      case PlaylistSequence::Repeat_Track:
        prev_virtual_index = current_virtual_index_;
        break;

      default:
        prev_virtual_index = PreviousVirtualIndex(virtual_items_.count(),ignore_repeat_track);
        break;
    }
  }

  // Still off the beginning?  Then just give up
  if (prev_virtual_index < 0) return -1;

  return virtual_items_[prev_virtual_index];

}

void Playlist::set_current_row(const int i, const AutoScroll autoscroll, const bool is_stopping, const bool force_inform) {

  QModelIndex old_current_item_index = current_item_index_;
  QModelIndex new_current_item_index;
  if (i != -1) new_current_item_index = QPersistentModelIndex(index(i, 0, QModelIndex()));

  if (new_current_item_index != current_item_index_) ClearStreamMetadata();

  int nextrow = next_row();
  if (nextrow != -1 && nextrow != i) {
    PlaylistItemPtr next_item = item_at(nextrow);
    if (next_item) {
      next_item->ClearTemporaryMetadata();
      emit dataChanged(index(nextrow, 0), index(nextrow, ColumnCount - 1));
    }
  }

  current_item_index_ = new_current_item_index;

  // if the given item is the first in the queue, remove it from the queue
  if (current_item_index_.isValid() && current_item_index_.row() == queue_->PeekNext()) {
    queue_->TakeNext();
  }

  if (current_item_index_ == old_current_item_index && !force_inform) {
    UpdateScrobblePoint();
    return;
  }

  if (old_current_item_index.isValid()) {
    emit dataChanged(old_current_item_index, old_current_item_index.sibling(old_current_item_index.row(), ColumnCount - 1));
  }

  // Update the virtual index
  if (i == -1) {
    current_virtual_index_ = -1;
  }
  else if (is_shuffled_ && current_virtual_index_ == -1) {
    // This is the first thing we're playing so we want to make sure the array is shuffled
    ReshuffleIndices();

    // Bring the one we've been asked to play to the start of the list
    virtual_items_.takeAt(virtual_items_.indexOf(i));
    virtual_items_.prepend(i);
    current_virtual_index_ = 0;
  }
  else if (is_shuffled_) {
    current_virtual_index_ = virtual_items_.indexOf(i);
  }
  else {
    current_virtual_index_ = i;
  }

  if (current_item_index_.isValid() && !is_stopping) {
    InformOfCurrentSongChange(autoscroll, false);
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
      const int count = dynamic_history_length() + 1 + dynamic_playlist_->GetDynamicFuture() - items_.count();
      if (count > 0) {
        InsertDynamicItems(count);
      }

      // Shrink the history, again this is not necessarily by 1, because the user might have moved items by hand.
      const int remove_count = dynamic_history_length() - dynamic_playlist_->GetDynamicHistory();
      if (0 < remove_count) RemoveItemsWithoutUndo(0, remove_count);
    }

    // the above actions make all commands on the undo stack invalid, so we better clear it.
    undo_stack_->clear();
  }

  if (current_item_index_.isValid()) {
    last_played_item_index_ = current_item_index_;
    Save();
  }

  UpdateScrobblePoint();

}

void Playlist::InsertDynamicItems(const int count) {

  PlaylistGeneratorInserter* inserter = new PlaylistGeneratorInserter(task_manager_, collection_, this);
  connect(inserter, SIGNAL(Error(QString)), SIGNAL(Error(QString)));
  connect(inserter, SIGNAL(PlayRequested(QModelIndex)), SIGNAL(PlayRequested(QModelIndex)));

  inserter->Load(this, -1, false, false, false, dynamic_playlist_, count);

}

Qt::ItemFlags Playlist::flags(const QModelIndex &idx) const {

  if (idx.isValid()) {
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (item_at(idx.row())->Metadata().IsEditable() && column_is_editable(static_cast<Column>(idx.column()))) flags |= Qt::ItemIsEditable;
    return flags;
  }
  else {
    return Qt::ItemIsDropEnabled;
  }

}

QStringList Playlist::mimeTypes() const {

  return QStringList() << "text/uri-list" << kRowsMimetype;

}

Qt::DropActions Playlist::supportedDropActions() const {
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
}

bool Playlist::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int, const QModelIndex&) {

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
    // We want to check if these songs are from the actual local file backend, if they are we treat them differently.
    if (song_data->backend && song_data->backend->songs_table() == SCollection::kSongsTable)
      InsertSongItems<CollectionPlaylistItem>(song_data->songs, row, play_now, enqueue_now, enqueue_next_now);
    else
      InsertSongItems<SongPlaylistItem>(song_data->songs, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const PlaylistItemMimeData *item_data = qobject_cast<const PlaylistItemMimeData*>(data)) {
    InsertItems(item_data->items_, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const InternetSongMimeData* internet_song_data = qobject_cast<const InternetSongMimeData*>(data)) {
    InsertInternetItems(internet_song_data->service, internet_song_data->songs, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (const PlaylistGeneratorMimeData *generator_data = qobject_cast<const PlaylistGeneratorMimeData*>(data)) {
    InsertSmartPlaylist(generator_data->generator_, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (data->hasFormat(kRowsMimetype)) {
    // Dragged from the playlist
    // Rearranging it is tricky...

    // Get the list of rows that were moved
    QList<int> source_rows;
    Playlist *source_playlist = nullptr;
    qint64 pid = 0;
    qint64 own_pid = QCoreApplication::applicationPid();

    QDataStream stream(data->data(kRowsMimetype));
    stream.readRawData(reinterpret_cast<char*>(&source_playlist), sizeof(source_playlist));
    stream >> source_rows;
    if (!stream.atEnd()) {
      stream.readRawData(reinterpret_cast<char*>(&pid), sizeof(pid));
    }
    else {
      pid = !own_pid;
    }

    std::stable_sort(source_rows.begin(), source_rows.end());  // Make sure we take them in order

    if (source_playlist == this) {
      // Dragged from this playlist - rearrange the items
      undo_stack_->push(new PlaylistUndoCommands::MoveItems(this, source_rows, row));
    }
    else if (pid == own_pid) {
      // Drag from a different playlist
      PlaylistItemList items;
      for (const int i : source_rows) items << source_playlist->item_at(i);

      if (items.count() > kUndoItemLimit) {
        // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
        InsertItemsWithoutUndo(items, row, false, false);
        undo_stack_->clear();
      }
      else {
        undo_stack_->push(new PlaylistUndoCommands::InsertItems(this, items, row));
      }

      // Remove the items from the source playlist if it was a move event
      if (action == Qt::MoveAction) {
        for (const int i : source_rows) {
          source_playlist->undo_stack()->push(new PlaylistUndoCommands::RemoveItems(source_playlist, i, 1));
        }
      }
    }
  }
  else if (data->hasFormat(kCddaMimeType)) {
    SongLoaderInserter *inserter = new SongLoaderInserter(task_manager_, collection_, backend_->app()->player());
    connect(inserter, SIGNAL(Error(QString)), SIGNAL(Error(QString)));
    inserter->LoadAudioCD(this, row, play_now, enqueue_now, enqueue_next_now);
  }
  else if (data->hasUrls()) {
    // URL list dragged from the file list or some other app
    InsertUrls(data->urls(), row, play_now, enqueue_now, enqueue_next_now);
  }

  return true;

}

void Playlist::InsertUrls(const QList<QUrl> &urls, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  SongLoaderInserter *inserter = new SongLoaderInserter(task_manager_, collection_, backend_->app()->player());
  connect(inserter, SIGNAL(Error(QString)), SIGNAL(Error(QString)));

  inserter->Load(this, pos, play_now, enqueue, enqueue_next, urls);

}

void Playlist::InsertSmartPlaylist(PlaylistGeneratorPtr generator, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  // Hack: If the generator hasn't got a collection set then use the main one
  if (!generator->collection()) {
    generator->set_collection(collection_);
  }

  PlaylistGeneratorInserter *inserter = new PlaylistGeneratorInserter(task_manager_, collection_, this);
  connect(inserter, SIGNAL(Error(QString)), SIGNAL(Error(QString)));

  inserter->Load(this, pos, play_now, enqueue, enqueue_next, generator);

  if (generator->is_dynamic()) {
    TurnOnDynamicPlaylist(generator);
  }

}

void Playlist::TurnOnDynamicPlaylist(PlaylistGeneratorPtr gen) {

  dynamic_playlist_ = gen;
  ShuffleModeChanged(PlaylistSequence::Shuffle_Off);
  emit DynamicModeChanged(true);
  Save();

}

void Playlist::MoveItemWithoutUndo(const int source, const int dest) {
  MoveItemsWithoutUndo(QList<int>() << source, dest);
}

void Playlist::MoveItemsWithoutUndo(const QList<int> &source_rows, int pos) {

  layoutAboutToBeChanged();
  PlaylistItemList moved_items;

  if (pos < 0) {
    pos = items_.count();
  }

  // Take the items out of the list first, keeping track of whether the insertion point changes
  int offset = 0;
  int start = pos;
  for (int source_row : source_rows) {
    moved_items << items_.takeAt(source_row - offset);
    if (pos > source_row) {
      start--;
    }
    offset++;
  }

  // Put the items back in
  for (int i = start; i < start + moved_items.count(); ++i) {
    moved_items[i - start]->RemoveForegroundColor(kDynamicHistoryPriority);
    items_.insert(i, moved_items[i - start]);
  }

  // Update persistent indexes
  for (const QModelIndex &pidx : persistentIndexList()) {
    const int dest_offset = source_rows.indexOf(pidx.row());
    if (dest_offset != -1) {
      // This index was moved
      changePersistentIndex(pidx, index(start + dest_offset, pidx.column(), QModelIndex()));
    }
    else {
      int d = 0;
      for (int source_row : source_rows) {
        if (pidx.row() > source_row) d--;
      }
      if (pidx.row() + d >= start) d += source_rows.count();

      changePersistentIndex(pidx, index(pidx.row() + d, pidx.column(), QModelIndex()));
    }
  }
  current_virtual_index_ = virtual_items_.indexOf(current_row());

  layoutChanged();
  Save();

}

void Playlist::MoveItemsWithoutUndo(int start, const QList<int> &dest_rows) {

  layoutAboutToBeChanged();
  PlaylistItemList moved_items;

  int pos = start;
  for (int dest_row : dest_rows) {
    if (dest_row < pos) start--;
  }

  if (start < 0) {
    start = items_.count() - dest_rows.count();
  }

  // Take the items out of the list first
  for (int i = 0; i < dest_rows.count(); i++)
    moved_items << items_.takeAt(start);

  // Put the items back in
  int offset = 0;
  for (int dest_row : dest_rows) {
    items_.insert(dest_row, moved_items[offset]);
    offset++;
  }

  // Update persistent indexes
  for (const QModelIndex &pidx : persistentIndexList()) {
    if (pidx.row() >= start && pidx.row() < start + dest_rows.count()) {
      // This index was moved
      const int i = pidx.row() - start;
      changePersistentIndex(pidx, index(dest_rows[i], pidx.column(), QModelIndex()));
    } else {
      int d = 0;
      if (pidx.row() >= start + dest_rows.count())
        d -= dest_rows.count();

      for (int dest_row : dest_rows) {
        if (pidx.row() + d > dest_row) d++;
      }

      changePersistentIndex(pidx, index(pidx.row() + d, pidx.column(), QModelIndex()));
    }
  }
  current_virtual_index_ = virtual_items_.indexOf(current_row());

  layoutChanged();
  Save();

}

void Playlist::InsertItems(const PlaylistItemList &itemsIn, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  if (itemsIn.isEmpty())
    return;

  PlaylistItemList items = itemsIn;

  // exercise vetoes
  SongList songs;

  for (PlaylistItemPtr item : items) {
    songs << item->Metadata();
  }

  const int song_count = songs.length();
  QSet<Song> vetoed;
  for (SongInsertVetoListener *listener : veto_listeners_) {
    for (const Song &song : listener->AboutToInsertSongs(GetAllSongs(), songs)) {
      // avoid veto-ing a song multiple times
      vetoed.insert(song);
    }
    if (vetoed.count() == song_count) {
      // all songs were vetoed and there's nothing more to do (there's no need for an undo step)
      return;
    }
  }

  if (!vetoed.isEmpty()) {
    QMutableListIterator<PlaylistItemPtr> it(items);
    while (it.hasNext()) {
      PlaylistItemPtr item = it.next();
      const Song &current = item->Metadata();

      if (vetoed.contains(current)) {
        vetoed.remove(current);
        it.remove();
      }
    }

    // check for empty items once again after veto
    if (items.isEmpty()) {
      return;
    }
  }

  const int start = pos == -1 ? items_.count() : pos;

  if (items.count() > kUndoItemLimit) {
    // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
    InsertItemsWithoutUndo(items, pos, enqueue, enqueue_next);
    undo_stack_->clear();
  }
  else {
    undo_stack_->push(new PlaylistUndoCommands::InsertItems(this, items, pos, enqueue, enqueue_next));
  }

  if (play_now) emit PlayRequested(index(start, 0), AutoScroll_Maybe);

}

void Playlist::InsertItemsWithoutUndo(const PlaylistItemList &items, const int pos, const bool enqueue, const bool enqueue_next) {

  if (items.isEmpty()) return;

  const int start = pos == -1 ? items_.count() : pos;
  const int end = start + items.count() - 1;

  beginInsertRows(QModelIndex(), start, end);
  for (int i = start; i <= end; ++i) {
    PlaylistItemPtr item = items[i - start];
    items_.insert(i, item);
    virtual_items_ << virtual_items_.count();

    if (item->source() == Song::Source_Collection) {
      int id = item->Metadata().id();
      if (id != -1) {
        collection_items_by_id_.insert(id, item);
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
      indexes << index(i, 0);
    }
    queue_->ToggleTracks(indexes);
  }

  if (enqueue_next) {
    QModelIndexList indexes;
    for (int i = start; i <= end; ++i) {
      indexes << index(i, 0);
    }
    queue_->InsertFirst(indexes);
  }

  Save();

  if (auto_sort_) {
    sort(sort_column_, sort_order_);
  }
  else {
    ReshuffleIndices();
  }

}

void Playlist::InsertCollectionItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {
  InsertSongItems<CollectionPlaylistItem>(songs, pos, play_now, enqueue, enqueue_next);
}

void Playlist::InsertSongs(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {
  InsertSongItems<SongPlaylistItem>(songs, pos, play_now, enqueue, enqueue_next);
}

void Playlist::InsertSongsOrCollectionItems(const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemList items;
  for (const Song &song : songs) {
    if (song.is_collection_song()) {
      items << PlaylistItemPtr(new CollectionPlaylistItem(song));
    }
    else {
      items << PlaylistItemPtr(new SongPlaylistItem(song));
    }
  }
  InsertItems(items, pos, play_now, enqueue, enqueue_next);

}

void Playlist::InsertInternetItems(InternetService *service, const SongList &songs, const int pos, const bool play_now, const bool enqueue, const bool enqueue_next) {

  PlaylistItemList playlist_items;
  for (const Song &song : songs) {
    playlist_items << std::shared_ptr<PlaylistItem>(new InternetPlaylistItem(service, song));
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

  for (int i = 0;  i < items_.size() ; i++) {
    // Update current items list
    QMutableListIterator<Song> it(songs);
    while (it.hasNext()) {
      const Song &song = it.next();
      const PlaylistItemPtr &item = items_[i];
      if (item->Metadata().url() == song.url() && (item->Metadata().filetype() == Song::FileType_Unknown || item->Metadata().filetype() == Song::FileType_Stream || item->Metadata().filetype() == Song::FileType_CDDA)) {
        PlaylistItemPtr new_item;
        if (song.is_collection_song()) {
          new_item = PlaylistItemPtr(new CollectionPlaylistItem(song));
          if (collection_items_by_id_.contains(song.id(), item)) collection_items_by_id_.remove(song.id(), item);
          collection_items_by_id_.insert(song.id(), new_item);
        }
        else {
          new_item = PlaylistItemPtr(new SongPlaylistItem(song));
        }
        items_[i] = new_item;
        emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
        // Also update undo actions
        for (int y = 0 ; y < undo_stack_->count() ; y++) {
          QUndoCommand *undo_action = const_cast<QUndoCommand*>(undo_stack_->command(i));
          PlaylistUndoCommands::InsertItems *undo_action_insert = dynamic_cast<PlaylistUndoCommands::InsertItems*>(undo_action);
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
  Save();

}

QMimeData *Playlist::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  // We only want one index per row, but we can't just take column 0 because the user might have hidden it.
  const int first_column = indexes.first().column();

  QMimeData *mimedata = new QMimeData;

  QList<QUrl> urls;
  QList<int> rows;
  for (const QModelIndex &idx : indexes) {
    if (idx.column() != first_column) continue;

    urls << items_[idx.row()]->Url();
    rows << idx.row();
  }

  QBuffer buf;
  buf.open(QIODevice::WriteOnly);
  QDataStream stream(&buf);

  const Playlist *self = this;
  const qint64 pid = QCoreApplication::applicationPid();

  stream.writeRawData(reinterpret_cast<char*>(&self), sizeof(self));
  stream << rows;
  stream.writeRawData(reinterpret_cast<const char*>(&pid), sizeof(pid));
  buf.close();

  mimedata->setUrls(urls);
  mimedata->setData(kRowsMimetype, buf.data());

  return mimedata;

}

bool Playlist::CompareItems(const int column, const Qt::SortOrder order, std::shared_ptr<PlaylistItem> _a, std::shared_ptr<PlaylistItem> _b) {

  std::shared_ptr<PlaylistItem> a = order == Qt::AscendingOrder ? _a : _b;
  std::shared_ptr<PlaylistItem> b = order == Qt::AscendingOrder ? _b : _a;

#define cmp(field) return a->Metadata().field() < b->Metadata().field()
#define strcmp(field) return QString::localeAwareCompare(a->Metadata().field().toLower(), b->Metadata().field().toLower()) < 0;

  switch (column) {

    case Column_Title:        strcmp(title_sortable);
    case Column_Artist:       strcmp(artist_sortable);
    case Column_Album:        strcmp(album_sortable);
    case Column_Length:       cmp(length_nanosec);
    case Column_Track:        cmp(track);
    case Column_Disc:         cmp(disc);
    case Column_Year:         cmp(year);
    case Column_OriginalYear: cmp(originalyear);
    case Column_Genre:        strcmp(genre);
    case Column_AlbumArtist:  strcmp(playlist_albumartist_sortable);
    case Column_Composer:     strcmp(composer);
    case Column_Performer:    strcmp(performer);
    case Column_Grouping:     strcmp(grouping);

    case Column_PlayCount:    cmp(playcount);
    case Column_SkipCount:    cmp(skipcount);
    case Column_LastPlayed:   cmp(lastplayed);

    case Column_Bitrate:      cmp(bitrate);
    case Column_Samplerate:   cmp(samplerate);
    case Column_Bitdepth:     cmp(bitdepth);
    case Column_Filename:
      return (QString::localeAwareCompare(a->Url().path().toLower(), b->Url().path().toLower()) < 0);
    case Column_BaseFilename: cmp(basefilename);
    case Column_Filesize:     cmp(filesize);
    case Column_Filetype:     cmp(filetype);
    case Column_DateModified: cmp(mtime);
    case Column_DateCreated:  cmp(ctime);

    case Column_Comment:      strcmp(comment);
    case Column_Source:       cmp(source);

    case Column_Rating:       cmp(rating);

    default: qLog(Error) << "No such column" << column;
  }

#undef cmp
#undef strcmp

  return false;

}

bool Playlist::ComparePathDepths(const Qt::SortOrder order, std::shared_ptr<PlaylistItem> _a, std::shared_ptr<PlaylistItem> _b) {

  std::shared_ptr<PlaylistItem> a = order == Qt::AscendingOrder ? _a : _b;
  std::shared_ptr<PlaylistItem> b = order == Qt::AscendingOrder ? _b : _a;

  int a_dir_level = a->Url().path().count('/');
  int b_dir_level = b->Url().path().count('/');

  return a_dir_level < b_dir_level;

}

QString Playlist::column_name(Column column) {

  switch (column) {
    case Column_Title:        return tr("Title");
    case Column_Artist:       return tr("Artist");
    case Column_Album:        return tr("Album");
    case Column_Track:        return tr("Track");
    case Column_Disc:         return tr("Disc");
    case Column_Length:       return tr("Length");
    case Column_Year:         return tr("Year");
    case Column_OriginalYear: return tr("Original year");
    case Column_Genre:        return tr("Genre");
    case Column_AlbumArtist:  return tr("Album artist");
    case Column_Composer:     return tr("Composer");
    case Column_Performer:    return tr("Performer");
    case Column_Grouping:     return tr("Grouping");

    case Column_PlayCount:    return tr("Play count");
    case Column_SkipCount:    return tr("Skip count");
    case Column_LastPlayed:   return tr("Last played");

    case Column_Samplerate:   return tr("Sample rate");
    case Column_Bitdepth:     return tr("Bit depth");
    case Column_Bitrate:      return tr("Bitrate");

    case Column_Filename:     return tr("File name");
    case Column_BaseFilename: return tr("File name (without path)");
    case Column_Filesize:     return tr("File size");
    case Column_Filetype:     return tr("File type");
    case Column_DateModified: return tr("Date modified");
    case Column_DateCreated:  return tr("Date created");

    case Column_Comment:      return tr("Comment");
    case Column_Source:       return tr("Source");
    case Column_Mood:         return tr("Mood");
    case Column_Rating:       return tr("Rating");
    default:                  qLog(Error) << "No such column" << column;;
  }
  return "";

}

QString Playlist::abbreviated_column_name(const Column column) {

  const QString &column_name = Playlist::column_name(column);

  switch (column) {
    case Column_Disc:
    case Column_PlayCount:
    case Column_SkipCount:
    case Column_Track:
      return QString("%1#").arg(column_name[0]);
    default:
      return column_name;
  }
  return "";

}

void Playlist::sort(int column, Qt::SortOrder order) {

  sort_column_ = column;
  sort_order_ = order;

  if (ignore_sorting_) return;

  PlaylistItemList new_items(items_);
  PlaylistItemList::iterator begin = new_items.begin();

  if (dynamic_playlist_ && current_item_index_.isValid())
    begin += current_item_index_.row() + 1;

  if (column == Column_Album) {
    // When sorting by album, also take into account discs and tracks.
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column_Track, order, _1, _2));
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column_Disc, order, _1, _2));
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column_Album, order, _1, _2));
  }
  else if (column == Column_Filename) {
    // When sorting by full paths we also expect a hierarchical order. This returns a breath-first ordering of paths.
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, Column_Filename, order, _1, _2));
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::ComparePathDepths, order, _1, _2));
  }
  else {
    std::stable_sort(begin, new_items.end(), std::bind(&Playlist::CompareItems, column, order, _1, _2));
  }

  undo_stack_->push(new PlaylistUndoCommands::SortItems(this, column, order, new_items));

  ReshuffleIndices();

}

void Playlist::ReOrderWithoutUndo(const PlaylistItemList &new_items) {

  layoutAboutToBeChanged();

  PlaylistItemList old_items = items_;
  items_ = new_items;

  QMap<const PlaylistItem*, int> new_rows;
  for (int i = 0; i < new_items.length(); ++i) {
    new_rows[new_items[i].get()] = i;
  }

  for (const QModelIndex &idx : persistentIndexList()) {
    const PlaylistItem *item = old_items[idx.row()].get();
    changePersistentIndex(idx, index(new_rows[item], idx.column(), idx.parent()));
  }

  layoutChanged();

  emit PlaylistChanged();
  Save();

}

void Playlist::Playing() { SetCurrentIsPaused(false); }

void Playlist::Paused() { SetCurrentIsPaused(true); }

void Playlist::Stopped() { SetCurrentIsPaused(false); }

void Playlist::SetCurrentIsPaused(const bool paused) {

  if (paused == current_is_paused_) return;

  current_is_paused_ = paused;

  if (current_item_index_.isValid())
    dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount - 1));
}

void Playlist::Save() const {

  if (!backend_ || is_loading_) return;

  backend_->SavePlaylistAsync(id_, items_, last_played_row(), dynamic_playlist_);

}

void Playlist::Restore() {

  if (!backend_) return;

  items_.clear();
  virtual_items_.clear();
  collection_items_by_id_.clear();

  cancel_restore_ = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QFuture<QList<PlaylistItemPtr>> future = QtConcurrent::run(&PlaylistBackend::GetPlaylistItems, backend_, id_);
#else
  QFuture<QList<PlaylistItemPtr>> future = QtConcurrent::run(backend_, &PlaylistBackend::GetPlaylistItems, id_);
#endif
  NewClosure(future, this, SLOT(ItemsLoaded(QFuture<PlaylistItemList>)), future);

}

void Playlist::ItemsLoaded(QFuture<PlaylistItemList> future) {

  if (cancel_restore_) return;

  PlaylistItemList items = future.result();

  // Backend returns empty elements for collection items which it couldn't match (because they got deleted); we don't need those
  QMutableListIterator<PlaylistItemPtr> it(items);
  while (it.hasNext()) {
    PlaylistItemPtr item = it.next();

    if (item->IsLocalCollectionItem() && item->Metadata().url().isEmpty()) {
      it.remove();
    }
  }

  is_loading_ = true;
  InsertItems(items, 0);
  is_loading_ = false;

  PlaylistBackend::Playlist p = backend_->GetPlaylist(id_);

  // The newly loaded list of items might be shorter than it was before so look out for a bad last_played index
  last_played_item_index_ = p.last_played == -1 || p.last_played >= rowCount() ? QModelIndex() : index(p.last_played);

  if (p.dynamic_type == PlaylistGenerator::Type_Query) {
    PlaylistGeneratorPtr gen = PlaylistGenerator::Create(p.dynamic_type);
    if (gen) {

      CollectionBackend *backend = nullptr;
      if (p.dynamic_backend == collection_->songs_table()) backend = collection_;

      if (backend) {
        gen->set_collection(backend);
        gen->Load(p.dynamic_data);
        TurnOnDynamicPlaylist(gen);
      }

    }
  }

  emit RestoreFinished();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  bool greyout = s.value("greyout_songs_startup", true).toBool();
  s.endGroup();

  // Should we gray out deleted songs asynchronously on startup?
  if (greyout) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    (void)QtConcurrent::run(&Playlist::InvalidateDeletedSongs, this);
#else
    (void)QtConcurrent::run(this, &Playlist::InvalidateDeletedSongs);
#endif
  }

  emit PlaylistLoaded();

}

static bool DescendingIntLessThan(int a, int b) { return a > b; }

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

bool Playlist::removeRows(int row, int count, const QModelIndex &parent) {

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
    undo_stack_->push(new PlaylistUndoCommands::RemoveItems(this, row, count));
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
    // we're splitting the input list into sequences of consecutive numbers
    part.append(rows.takeFirst());
    while (!rows.isEmpty() && rows.first() == part.last() - 1) {
      part.append(rows.takeFirst());
    }

    // and now we're removing the current sequence
    if (!removeRows(part.last(), part.size())) {
      return false;
    }

    part.clear();
  }

  return true;

}

PlaylistItemList Playlist::RemoveItemsWithoutUndo(const int row, const int count) {

  if (row < 0 || row >= items_.size() || row + count > items_.size()) {
    return PlaylistItemList();
  }
  beginRemoveRows(QModelIndex(), row, row + count - 1);

  // Remove items
  PlaylistItemList ret;
  for (int i = 0; i < count; ++i) {
    PlaylistItemPtr item(items_.takeAt(row));
    ret << item;

    if (item->source() == Song::Source_Collection) {
      int id = item->Metadata().id();
      if (id != -1 && collection_items_by_id_.contains(id, item)) {
        collection_items_by_id_.remove(id, item);
      }
    }
  }

  endRemoveRows();

  QList<int>::iterator it = virtual_items_.begin();
  while (it != virtual_items_.end()) {
    if (*it >= items_.count())
      it = virtual_items_.erase(it);
    else
      ++it;
  }

  // Reset current_virtual_index_
  if (current_row() == -1)
    if (row - 1 > 0 && row - 1 < items_.size()) {
      current_virtual_index_ = virtual_items_.indexOf(row - 1);
    }
    else {
      current_virtual_index_ = -1;
    }
  else
    current_virtual_index_ = virtual_items_.indexOf(current_row());

  Save();
  return ret;

}

void Playlist::StopAfter(const int row) {

  QModelIndex old_stop_after = stop_after_;

  if ((stop_after_.isValid() && stop_after_.row() == row) || row == -1)
    stop_after_ = QModelIndex();
  else
    stop_after_ = index(row, 0);

  if (old_stop_after.isValid())
    emit dataChanged(old_stop_after, old_stop_after.sibling(old_stop_after.row(), ColumnCount - 1));
  if (stop_after_.isValid())
    emit dataChanged(stop_after_, stop_after_.sibling(stop_after_.row(), ColumnCount - 1));

}

void Playlist::SetStreamMetadata(const QUrl &url, const Song &song, const bool minor) {

  if (!current_item() || current_item()->Url() != url) return;

  bool update_scrobble_point = song.length_nanosec() != current_item_metadata().length_nanosec();
  current_item()->SetTemporaryMetadata(song);
  if (update_scrobble_point) UpdateScrobblePoint();
  InformOfCurrentSongChange(AutoScroll_Never, minor);

}

void Playlist::ClearStreamMetadata() {

  if (!current_item()) return;

  current_item()->ClearTemporaryMetadata();
  UpdateScrobblePoint();

  emit dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount-1));

}

bool Playlist::stop_after_current() const {

  PlaylistSequence::RepeatMode repeat_mode = playlist_sequence_->repeat_mode();
  if (repeat_mode == PlaylistSequence::Repeat_OneByOne) {
    return true;
  }

  return stop_after_.isValid() && current_item_index_.isValid() && stop_after_.row() == current_item_index_.row();

}

PlaylistItemPtr Playlist::current_item() const {

  // QList[] runs in constant time, so no need to cache current_item
  if (current_item_index_.isValid() && current_item_index_.row() <= items_.length())
    return items_[current_item_index_.row()];
  return PlaylistItemPtr();

}

PlaylistItem::Options Playlist::current_item_options() const {
  if (!current_item()) return PlaylistItem::Default;
  return current_item()->options();
}

Song Playlist::current_item_metadata() const {
  if (!current_item()) return Song();
  return current_item()->Metadata();
}

void Playlist::Clear() {

  // If loading songs from session restore async, don't insert them
  cancel_restore_ = true;

  const int count = items_.count();

  if (count > kUndoItemLimit) {
    // Too big to keep in the undo stack. Also clear the stack because it might have been invalidated.
    RemoveItemsWithoutUndo(0, count);
    undo_stack_->clear();
  }
  else {
    undo_stack_->push(new PlaylistUndoCommands::RemoveItems(this, 0, count));
  }

  TurnOffDynamicPlaylist();

  Save();

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
    RemoveItemsWithoutUndo(0, items_.count());
    return;
  }

  int start = 0;
  forever {
    // Find a place to start - first row that isn't in the queue
    forever {
      if (start >= rowCount()) return;
      if (!queue_->ContainsSourceRow(start) && current_row() != start) break;
      start++;
    }

    // Figure out how many rows to remove - keep going until we find a row that is in the queue
    int count = 1;
    forever {
      if (start + count >= rowCount()) break;
      if (queue_->ContainsSourceRow(start + count) || current_row() == start + count) break;
      count++;
    }

    RemoveItemsWithoutUndo(start, count);
    start++;
  }

}

void Playlist::ReloadItems(const QList<int> &rows) {

  for (int row : rows) {
    PlaylistItemPtr item = item_at(row);

    Song old_metadata = item->Metadata();

    item->Reload();

    if (row == current_row()) {
      const bool minor = old_metadata.title() == item->Metadata().title() &&
                         old_metadata.albumartist() == item->Metadata().albumartist() &&
                         old_metadata.artist() == item->Metadata().artist() &&
                         old_metadata.album() == item->Metadata().album();
      InformOfCurrentSongChange(AutoScroll_Never, minor);
    }
    else {
      emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    }
  }

  Save();

}

void Playlist::AddSongInsertVetoListener(SongInsertVetoListener *listener) {
  veto_listeners_.append(listener);
  connect(listener, SIGNAL(destroyed()), this, SLOT(SongInsertVetoListenerDestroyed()));
}

void Playlist::RemoveSongInsertVetoListener(SongInsertVetoListener *listener) {
  disconnect(listener, SIGNAL(destroyed()), this, SLOT(SongInsertVetoListenerDestroyed()));
  veto_listeners_.removeAll(listener);
}

void Playlist::SongInsertVetoListenerDestroyed() {
  veto_listeners_.removeAll(qobject_cast<SongInsertVetoListener*>(sender()));
}

void Playlist::Shuffle() {

  PlaylistItemList new_items(items_);

  int begin = 0;
  if (current_item_index_.isValid()) {
    if (new_items[0] != new_items[current_item_index_.row()])
      std::swap(new_items[0], new_items[current_item_index_.row()]);
    begin = 1;
  }

  if (dynamic_playlist_ && current_item_index_.isValid())
    begin += current_item_index_.row() + 1;

  const int count = items_.count();
  for (int i = begin; i < count; ++i) {
    int new_pos = i + (rand() % (count - i));

    std::swap(new_items[i], new_items[new_pos]);
  }

  undo_stack_->push(new PlaylistUndoCommands::ShuffleItems(this, new_items));

}

namespace {
bool AlbumShuffleComparator(const QMap<QString, int> &album_key_positions, const QMap<int, QString> &album_keys, const int left, const int right) {

  const int left_pos = album_key_positions[album_keys[left]];
  const int right_pos = album_key_positions[album_keys[right]];

  if (left_pos == right_pos) return left < right;
  return left_pos < right_pos;

}
}

void Playlist::ReshuffleIndices() {

  if (!playlist_sequence_) {
    return;
  }

  if (playlist_sequence_->shuffle_mode() == PlaylistSequence::Shuffle_Off) {
    // No shuffling - sort the virtual item list normally.
    std::sort(virtual_items_.begin(), virtual_items_.end());
    if (current_row() != -1)
      current_virtual_index_ = virtual_items_.indexOf(current_row());
    return;
  }

  // If the user is already playing a song, advance the begin iterator to only shuffle items that haven't been played yet.
  QList<int>::iterator begin = virtual_items_.begin();
  QList<int>::iterator end = virtual_items_.end();
  if (current_virtual_index_ != -1)
    std::advance(begin, current_virtual_index_ + 1);

  std::random_device rd;
  std::mt19937 g(rd());

  switch (playlist_sequence_->shuffle_mode()) {
    case PlaylistSequence::Shuffle_Off:
      // Handled above.
      break;

    case PlaylistSequence::Shuffle_All:
    case PlaylistSequence::Shuffle_InsideAlbum:
      std::shuffle(begin, end, g);
      break;

    case PlaylistSequence::Shuffle_Albums: {
      QMap<int, QString> album_keys;  // real index -> key
      QSet<QString> album_key_set;    // unique keys

      // Find all the unique albums in the playlist
      for (QList<int>::iterator it = begin; it != end; ++it) {
        const int index = *it;
        const QString key = items_[index]->Metadata().AlbumKey();
        album_keys[index] = key;
        album_key_set << key;
      }

      // Shuffle them
      QStringList shuffled_album_keys = album_key_set.values();
      std::shuffle(shuffled_album_keys.begin(), shuffled_album_keys.end(), g);

      // If the user is currently playing a song, force its album to be first
      // Or if the song was not playing but it was selected, force its album to be first.
      if (current_virtual_index_ != -1 || current_row() != -1) {
        const QString key = items_[current_row()]->Metadata().AlbumKey();
        const int pos = shuffled_album_keys.indexOf(key);
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
      std::stable_sort(begin, end, std::bind(AlbumShuffleComparator, album_key_positions, album_keys, _1, _2));

      break;
    }
  }

}

void Playlist::set_sequence(PlaylistSequence *v) {

  playlist_sequence_ = v;
  connect(v, SIGNAL(ShuffleModeChanged(PlaylistSequence::ShuffleMode)), SLOT(ShuffleModeChanged(PlaylistSequence::ShuffleMode)));

  ShuffleModeChanged(v->shuffle_mode());

}

QSortFilterProxyModel *Playlist::proxy() const { return proxy_; }

SongList Playlist::GetAllSongs() const {

  SongList ret;
  for (PlaylistItemPtr item : items_) {
    ret << item->Metadata();
  }
  return ret;

}

PlaylistItemList Playlist::GetAllItems() const { return items_; }

quint64 Playlist::GetTotalLength() const {

  quint64 ret = 0;
  for (PlaylistItemPtr item : items_) {
    quint64 length = item->Metadata().length_nanosec();
    if (length > 0) ret += length;
  }
  return ret;

}

PlaylistItemList Playlist::collection_items_by_id(const int id) const {
  return collection_items_by_id_.values(id);
}

void Playlist::TracksAboutToBeDequeued(const QModelIndex&, int begin, int end) {

  for (int i = begin; i <= end; ++i) {
    temp_dequeue_change_indexes_ << queue_->mapToSource(queue_->index(i, Column_Title));
  }

}

void Playlist::TracksDequeued() {

  for (const QModelIndex &idx : temp_dequeue_change_indexes_) {
    emit dataChanged(idx, idx);
  }
  temp_dequeue_change_indexes_.clear();
  emit QueueChanged();

}

void Playlist::TracksEnqueued(const QModelIndex&, const int begin, const int end) {

  const QModelIndex &b = queue_->mapToSource(queue_->index(begin, Column_Title));
  const QModelIndex &e = queue_->mapToSource(queue_->index(end, Column_Title));
  emit dataChanged(b, e);

}

void Playlist::QueueLayoutChanged() {

  for (int i = 0; i < queue_->rowCount(); ++i) {
    const QModelIndex &idx = queue_->mapToSource(queue_->index(i, Column_Title));
    emit dataChanged(idx, idx);
  }

}

void Playlist::ItemChanged(const int row) {

  QModelIndex idx = index(row, ColumnCount - 1);
  if (idx.isValid()) {
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
  }

}

void Playlist::ItemChanged(PlaylistItemPtr item) {

  for (int row = 0; row < items_.count(); ++row) {
    if (items_[row] == item) {
      ItemChanged(row);
    }
  }

}

void Playlist::InformOfCurrentSongChange(const AutoScroll autoscroll, const bool minor) {

  // if the song is invalid, we won't play it - there's no point in informing anybody about the change
  const Song metadata(current_item_metadata());
  if (metadata.is_valid()) {
    if (minor) {
      emit SongMetadataChanged(metadata);
      if (editing_ != current_item_index_.row()) {
        emit dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount - 1));
      }
    }
    else {
      emit CurrentSongChanged(metadata);
      emit MaybeAutoscroll(autoscroll);
      emit dataChanged(index(current_item_index_.row(), 0), index(current_item_index_.row(), ColumnCount - 1));
    }
  }

}

void Playlist::InvalidateDeletedSongs() {

  QList<int> invalidated_rows;

  for (int row = 0; row < items_.count(); ++row) {
    PlaylistItemPtr item = items_[row];
    Song song = item->Metadata();

    if (song.url().isLocalFile()) {
      bool exists = QFile::exists(song.url().toLocalFile());

      if (!exists && !item->HasForegroundColor(kInvalidSongPriority)) {
        // gray out the song if it's not there
        item->SetForegroundColor(kInvalidSongPriority, kInvalidSongColor);
        invalidated_rows.append(row);
      }
      else if (exists && item->HasForegroundColor(kInvalidSongPriority)) {
        item->RemoveForegroundColor(kInvalidSongPriority);
        invalidated_rows.append(row);
      }
    }
  }

  if (!invalidated_rows.isEmpty())
    ReloadItems(invalidated_rows);

}

void Playlist::RemoveDeletedSongs() {

  QList<int> rows_to_remove;

  for (int row = 0; row < items_.count(); ++row) {
    PlaylistItemPtr item = items_[row];
    Song song = item->Metadata();

    if (song.url().isLocalFile() && !QFile::exists(song.url().toLocalFile())) {
      rows_to_remove.append(row);
    }
  }

  removeRows(rows_to_remove);

}

namespace {

struct SongSimilarHash {
  long operator() (const Song &song) const {
    return HashSimilar(song);
  }
};

struct SongSimilarEqual {
  long operator()(const Song &song1, const Song &song2) const {
    return song1.IsSimilar(song2);
  }
};

}  // namespace

void Playlist::RemoveDuplicateSongs() {

  QList<int> rows_to_remove;
  std::unordered_map<Song, int, SongSimilarHash, SongSimilarEqual> unique_songs;

  for (int row = 0; row < items_.count(); ++row) {
    PlaylistItemPtr item = items_[row];
    const Song &song = item->Metadata();

    bool found_duplicate = false;

    auto uniq_song_it = unique_songs.find(song);
    if (uniq_song_it != unique_songs.end()) {
      const Song &uniq_song = uniq_song_it->first;

      if (song.bitrate() > uniq_song.bitrate()) {
        rows_to_remove.append(unique_songs[uniq_song]);
        unique_songs.erase(uniq_song);
        unique_songs.insert(std::make_pair(song, row));
      }
      else {
        rows_to_remove.append(row);
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
    PlaylistItemPtr item = items_[row];
    const Song &song = item->Metadata();

    // Check only local files
    if (song.url().isLocalFile() && !QFile::exists(song.url().toLocalFile())) {
      rows_to_remove.append(row);
    }
  }

  removeRows(rows_to_remove);

}

bool Playlist::ApplyValidityOnCurrentSong(const QUrl &url, const bool valid) {

  PlaylistItemPtr current = current_item();

  if (current) {
    Song current_song = current->Metadata();

    // If validity has changed, reload the item
    if (current_song.source() == Song::Source_LocalFile || current_song.source() == Song::Source_Collection) {
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
    emit dataChanged(source_index, source_index);
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
  if (((result.type == AlbumCoverLoaderResult::Type_Manual && result.cover_url.isLocalFile()) || result.type == AlbumCoverLoaderResult::Type_ManuallyUnset) && (song.source() == Song::Source_LocalFile || song.source() == Song::Source_CDDA || song.source() == Song::Source_Device)) {
    PlaylistItemPtr item = current_item();
    if (item && item->Metadata() == song && (!item->Metadata().art_manual_is_valid() || (result.type == AlbumCoverLoaderResult::Type_ManuallyUnset && !item->Metadata().has_manually_unset_cover()))) {
      qLog(Debug) << "Updating art manual for local song" << song.title() << song.album() << song.title() << "to" << result.cover_url << "in playlist.";
      item->SetArtManual(result.cover_url);
      Save();
    }
  }

}

int Playlist::dynamic_history_length() const {
  return dynamic_playlist_ && last_played_item_index_.isValid() ? last_played_item_index_.row() + 1 : 0;
}

void Playlist::TurnOffDynamicPlaylist() {

  dynamic_playlist_.reset();

  if (playlist_sequence_) {
    ShuffleModeChanged(playlist_sequence_->shuffle_mode());
  }

  emit DynamicModeChanged(false);
  Save();

}

void Playlist::RateSong(const QModelIndex &idx, const double rating) {

  if (has_item_at(idx.row())) {
    PlaylistItemPtr item = item_at(idx.row());
    if (item && item->IsLocalCollectionItem() && item->Metadata().id() != -1) {
      collection_->UpdateSongRatingAsync(item->Metadata().id(), rating);
    }
  }

}

void Playlist::RateSongs(const QModelIndexList &index_list, const double rating) {

  QList<int> id_list;
  for (const QModelIndex &idx : index_list) {
    const int row = idx.row();
    if (has_item_at(row)) {
      PlaylistItemPtr item = item_at(row);
      if (item && item->IsLocalCollectionItem() && item->Metadata().id() != -1) {
        id_list << item->Metadata().id();
      }
    }
  }
  collection_->UpdateSongsRatingAsync(id_list, rating);

}
