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

#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QDialog>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QString>
#include <QRegularExpression>
#include <QUrl>
#include <QAbstractItemModel>
#include <QScrollBar>
#include <QSettings>
#include <QMessageBox>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "constants/filenameconstants.h"
#include "utilities/timeutils.h"
#include "collection/collectionbackend.h"
#include "covermanager/currentalbumcoverloader.h"
#include "constants/playlistsettings.h"
#include "playlist.h"
#include "playlistbackend.h"
#include "playlistcontainer.h"
#include "playlistmanager.h"
#include "playlistitem.h"
#include "playlistview.h"
#include "playlistsaveoptionsdialog.h"
#include "playlistparsers/playlistparser.h"
#include "dialogs/saveplaylistsdialog.h"

using namespace Qt::Literals::StringLiterals;

class ParserBase;

PlaylistManager::PlaylistManager(const SharedPtr<TaskManager> task_manager,
                                 const SharedPtr<TagReaderClient> tagreader_client,
                                 const SharedPtr<UrlHandlers> url_handlers,
                                 const SharedPtr<PlaylistBackend> playlist_backend,
                                 const SharedPtr<CollectionBackend> collection_backend,
                                 const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                 QObject *parent)
    : PlaylistManagerInterface(parent),
      task_manager_(task_manager),
      tagreader_client_(tagreader_client),
      url_handlers_(url_handlers),
      playlist_backend_(playlist_backend),
      collection_backend_(collection_backend),
      current_albumcover_loader_(current_albumcover_loader),
      sequence_(nullptr),
      parser_(nullptr),
      playlist_container_(nullptr),
      current_(-1),
      active_(-1),
      playlists_loading_(0) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

}

PlaylistManager::~PlaylistManager() {

  const QList<Data> datas = playlists_.values();
  for (const Data &data : datas) delete data.p;

}

void PlaylistManager::Init(PlaylistSequence *sequence, PlaylistContainer *playlist_container) {

  sequence_ = sequence;
  playlist_container_ = playlist_container;

  parser_ = new PlaylistParser(tagreader_client_, collection_backend_, this);

  QObject::connect(&*collection_backend_, &CollectionBackend::SongsChanged, this, &PlaylistManager::UpdateCollectionSongs);
  QObject::connect(&*collection_backend_, &CollectionBackend::SongsStatisticsChanged, this, &PlaylistManager::UpdateCollectionSongs);
  QObject::connect(&*collection_backend_, &CollectionBackend::SongsRatingChanged, this, &PlaylistManager::UpdateCollectionSongs);

  QObject::connect(parser_, &PlaylistParser::Error, this, &PlaylistManager::Error);

  const PlaylistBackend::PlaylistList playlists = playlist_backend_->GetAllOpenPlaylists();
  for (const PlaylistBackend::Playlist &p : playlists) {
    ++playlists_loading_;
    Playlist *ret = AddPlaylist(p.id, p.name, p.special_type, p.ui_path, p.favorite);
    QObject::connect(ret, &Playlist::PlaylistLoaded, this, &PlaylistManager::PlaylistLoaded);
  }

  // If no playlist exists then make a new one
  if (playlists_.isEmpty()) New(tr("Playlist"));

  Q_EMIT PlaylistManagerInitialized();

}

void PlaylistManager::PlaylistLoaded() {

  Playlist *playlist = qobject_cast<Playlist*>(sender());
  if (!playlist) return;
  QObject::disconnect(playlist, &Playlist::PlaylistLoaded, this, &PlaylistManager::PlaylistLoaded);
  --playlists_loading_;
  if (playlists_loading_ == 0) {
    Q_EMIT AllPlaylistsLoaded();
  }

}

QList<Playlist*> PlaylistManager::GetAllPlaylists() const {

  QList<Playlist*> result;

  const QList<Data> datas = playlists_.values();
  result.reserve(datas.count());
  for (const Data &data : datas) {
    result.append(data.p);
  }

  return result;

}

QItemSelection PlaylistManager::selection(const int id) const {
  QMap<int, Data>::const_iterator it = playlists_.find(id);
  return it->selection;
}

Playlist *PlaylistManager::AddPlaylist(const int id, const QString &name, const QString &special_type, const QString &ui_path, const bool favorite) {

  Playlist *ret = new Playlist(task_manager_, url_handlers_, playlist_backend_, collection_backend_, tagreader_client_, id, special_type, favorite);
  ret->set_sequence(sequence_);
  ret->set_ui_path(ui_path);

  QObject::connect(ret, &Playlist::CurrentSongChanged, this, &PlaylistManager::CurrentSongChanged);
  QObject::connect(ret, &Playlist::CurrentSongMetadataChanged, this, &PlaylistManager::CurrentSongMetadataChanged);
  QObject::connect(ret, &Playlist::PlaylistChanged, this, &PlaylistManager::OneOfPlaylistsChanged);
  QObject::connect(ret, &Playlist::PlaylistChanged, this, &PlaylistManager::UpdateSummaryText);
  QObject::connect(ret, &Playlist::EditingFinished, this, &PlaylistManager::EditingFinished);
  QObject::connect(ret, &Playlist::Error, this, &PlaylistManager::Error);
  QObject::connect(ret, &Playlist::PlayRequested, this, &PlaylistManager::PlayRequested);
  QObject::connect(ret, &Playlist::Rename, this, &PlaylistManager::Rename);
  QObject::connect(playlist_container_->view(), &PlaylistView::ColumnAlignmentChanged, ret, &Playlist::SetColumnAlignment);
  QObject::connect(&*current_albumcover_loader_, &CurrentAlbumCoverLoader::AlbumCoverLoaded, ret, &Playlist::AlbumCoverLoaded);

  playlists_[id] = Data(ret, name);

  Q_EMIT PlaylistAdded(id, name, favorite);

  if (current_ == -1) {
    SetCurrentPlaylist(id);
  }
  if (active_ == -1) {
    SetActivePlaylist(id);
  }

  return ret;

}

void PlaylistManager::New(const QString &name, const SongList &songs, const QString &special_type) {

  if (name.isNull()) return;

  int id = playlist_backend_->CreatePlaylist(name, special_type);

  if (id == -1) qFatal("Couldn't create playlist");

  Playlist *playlist = AddPlaylist(id, name, special_type, QString(), false);
  playlist->InsertSongsOrCollectionItems(songs);

  SetCurrentPlaylist(id);

  // If the name is just "Playlist", append the id
  if (name == tr("Playlist")) {
    Rename(id, QStringLiteral("%1 %2").arg(name).arg(id));
  }

}

void PlaylistManager::Load(const QString &filename) {

  QFileInfo fileinfo(filename);

  const int id = playlist_backend_->CreatePlaylist(fileinfo.completeBaseName(), QString());

  if (id == -1) {
    Q_EMIT Error(tr("Couldn't create playlist"));
    return;
  }

  Playlist *playlist = AddPlaylist(id, fileinfo.completeBaseName(), QString(), QString(), false);

  playlist->InsertUrls(QList<QUrl>() << QUrl::fromLocalFile(filename));

}

void PlaylistManager::Save(const int id, const QString &playlist_name, const QString &filename, const PlaylistSettings::PathType path_type) {

  if (playlists_.contains(id)) {
    parser_->Save(playlist_name, playlist(id)->GetAllSongs(), filename, path_type);
  }
  else {
    // Playlist is not in the playlist manager: probably save action was triggered from the left sidebar and the playlist isn't loaded.
    QFuture<SongList> future = QtConcurrent::run(&PlaylistBackend::GetPlaylistSongs, playlist_backend_, id);
    QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
    QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher, playlist_name, filename, path_type]() {
      ItemsLoadedForSavePlaylist(playlist_name, watcher->result(), filename, path_type);
      watcher->deleteLater();
    });
    watcher->setFuture(future);
  }

}

void PlaylistManager::ItemsLoadedForSavePlaylist(const QString &playlist_name, const SongList &songs, const QString &filename, const PlaylistSettings::PathType path_type) {

  parser_->Save(playlist_name, songs, filename, path_type);

}

void PlaylistManager::SaveWithUI(const int id, const QString &playlist_name) {

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  QString last_save_filter = s.value(PlaylistSettings::kLastSaveFilter, parser()->default_filter()).toString();
  QString last_save_path = s.value(PlaylistSettings::kLastSavePath, QDir::homePath()).toString();
  QString last_save_extension = s.value(PlaylistSettings::kLastSaveExtension, parser()->default_extension()).toString();
  s.endGroup();

  QString suggested_filename = playlist_name;
  QString filename = last_save_path + QLatin1Char('/') + suggested_filename.remove(u'/').remove(QRegularExpression(QLatin1String(kProblematicCharactersRegex), QRegularExpression::CaseInsensitiveOption)) + QLatin1Char('.') + last_save_extension;

  QFileInfo fileinfo;
  Q_FOREVER {
    filename = QFileDialog::getSaveFileName(nullptr, tr("Save playlist", "Title of the playlist save dialog."), filename, parser()->filters(PlaylistParser::Type::Save), &last_save_filter);
    if (filename.isEmpty()) return;
    fileinfo.setFile(filename);
    ParserBase *parser = parser_->ParserForExtension(PlaylistParser::Type::Save, fileinfo.suffix());
    if (parser) break;
    QMessageBox::warning(nullptr, tr("Unknown playlist extension"), tr("Unknown file extension for playlist."));
  }

  s.beginGroup(PlaylistSettings::kSettingsGroup);
  PlaylistSettings::PathType path_type = static_cast<PlaylistSettings::PathType>(s.value(PlaylistSettings::kPathType, static_cast<int>(PlaylistSettings::PathType::Automatic)).toInt());
  s.endGroup();
  if (path_type == PlaylistSettings::PathType::Ask_User) {
    PlaylistSaveOptionsDialog optionsdialog;
    optionsdialog.setModal(true);
    if (optionsdialog.exec() != QDialog::Accepted) return;
    path_type = optionsdialog.path_type();
  }

  s.beginGroup(PlaylistSettings::kSettingsGroup);
  s.setValue(PlaylistSettings::kLastSaveFilter, last_save_filter);
  s.setValue(PlaylistSettings::kLastSavePath, fileinfo.path());
  s.setValue(PlaylistSettings::kLastSaveExtension, fileinfo.suffix());
  s.endGroup();

  Save(id == -1 ? current_id() : id, playlist_name, filename, path_type);

}

void PlaylistManager::Rename(const int id, const QString &new_name) {

  Q_ASSERT(playlists_.contains(id));

  playlist_backend_->RenamePlaylist(id, new_name);
  playlists_[id].name = new_name;

  Q_EMIT PlaylistRenamed(id, new_name);

}

void PlaylistManager::Favorite(const int id, const bool favorite) {

  if (playlists_.contains(id)) {
    // If playlists_ contains this playlist, its means it's opened: star or unstar it.
    playlist_backend_->FavoritePlaylist(id, favorite);
    playlists_[id].p->set_favorite(favorite);
  }
  else {
    Q_ASSERT(!favorite);
    // Otherwise it means user wants to remove this playlist from the left panel,
    // while it's not visible in the playlist tabbar either, because it has been closed: delete it.
    playlist_backend_->RemovePlaylist(id);
  }
  Q_EMIT PlaylistFavorited(id, favorite);

}

bool PlaylistManager::Close(const int id) {

  // Won't allow removing the last playlist
  if (playlists_.count() <= 1 || !playlists_.contains(id)) return false;

  int next_id = -1;
  const QList<int> playlist_ids = playlists_.keys();
  for (const int possible_next_id : playlist_ids) {
    if (possible_next_id != id) {
      next_id = possible_next_id;
      break;
    }
  }
  if (next_id == -1) return false;

  if (id == active_) SetActivePlaylist(next_id);
  if (id == current_) SetCurrentPlaylist(next_id);

  Data data = playlists_.take(id);
  Q_EMIT PlaylistClosed(id);

  if (!data.p->is_favorite()) {
    playlist_backend_->RemovePlaylist(id);
    Q_EMIT PlaylistDeleted(id);
  }
  delete data.p;

  return true;

}

void PlaylistManager::Delete(const int id) {

  if (!Close(id)) {
    return;
  }

  playlist_backend_->RemovePlaylist(id);
  Q_EMIT PlaylistDeleted(id);

}

void PlaylistManager::OneOfPlaylistsChanged() {
  Q_EMIT PlaylistChanged(qobject_cast<Playlist*>(sender()));
}

void PlaylistManager::SetCurrentPlaylist(const int id) {

  Q_ASSERT(playlists_.contains(id));

  // Save the scroll position for the current playlist.
  if (playlists_.contains(current_)) {
    playlists_[current_].scroll_position = playlist_container_->view()->verticalScrollBar()->value();
  }

  current_ = id;
  Q_EMIT CurrentChanged(current(), playlists_[id].scroll_position);
  UpdateSummaryText();

}

void PlaylistManager::SetActivePlaylist(const int id) {

  Q_ASSERT(playlists_.contains(id));

  // Kinda a hack: unset the current item from the old active playlist before setting the new one
  if (active_ != -1 && active_ != id) active()->set_current_row(-1);

  active_ = id;

  Q_EMIT ActiveChanged(active());

}

void PlaylistManager::SetActiveToCurrent() {

  // Check if we need to update the active playlist.
  // By calling SetActiveToCurrent, the playlist manager emits the signal "ActiveChanged".
  // This signal causes the network remote module to send all playlists to the clients, even if no change happen.
  if (current_id() != active_id()) {
    SetActivePlaylist(current_id());
  }

}

void PlaylistManager::ClearCurrent() {
  current()->Clear();
}

void PlaylistManager::ShuffleCurrent() {
  current()->Shuffle();
}

void PlaylistManager::RemoveDuplicatesCurrent() {
  current()->RemoveDuplicateSongs();
}

void PlaylistManager::RemoveUnavailableCurrent() {
  current()->RemoveUnavailableSongs();
}

void PlaylistManager::SetActivePlaying() { active()->Playing(); }

void PlaylistManager::SetActivePaused() { active()->Paused(); }

void PlaylistManager::SetActiveStopped() { active()->Stopped(); }

void PlaylistManager::ChangePlaylistOrder(const QList<int> &ids) {
  playlist_backend_->SetPlaylistOrder(ids);
}

void PlaylistManager::UpdateSummaryText() {

  int tracks = current()->rowCount();
  quint64 nanoseconds = 0;
  int selected = 0;

  // Get the length of the selected tracks
  const QItemSelection ranges = playlists_.value(current_id()).selection;
  for (const QItemSelectionRange &range : ranges) {
    if (!range.isValid()) continue;

    selected += range.bottom() - range.top() + 1;
    for (int i = range.top(); i <= range.bottom(); ++i) {
      qint64 length = range.model()->index(i, static_cast<int>(Playlist::Column::Length)).data().toLongLong();
      if (length > 0) {
        nanoseconds += length;
      }
    }
  }

  QString summary;
  if (selected > 1) {
    summary += tr("%1 selected of").arg(selected) + QLatin1Char(' ');
  }
  else {
    nanoseconds = current()->GetTotalLength();
  }

  summary += tr("%n track(s)", "", tracks);

  if (nanoseconds > 0) {
    summary += " - [ "_L1 + Utilities::WordyTimeNanosec(nanoseconds) + " ]"_L1;
  }

  Q_EMIT SummaryTextChanged(summary);

}

void PlaylistManager::SelectionChanged(const QItemSelection &selection) {
  playlists_[current_id()].selection = selection;
  UpdateSummaryText();
}

void PlaylistManager::UpdateCollectionSongs(const SongList &songs) {

  // Some songs might've changed in the collection, let's update any playlist items we have that match those songs

  for (const Song &song : songs) {
    for (const Data &data : std::as_const(playlists_)) {
      const PlaylistItemPtrList items = data.p->collection_items(song.source(), song.id());
      for (PlaylistItemPtr item : items) {
        if (item->EffectiveMetadata().directory_id() != song.directory_id()) continue;
        data.p->UpdateItemMetadata(item, song, false);
      }
    }
  }

}

// When Player has processed the new song chosen by the user...
void PlaylistManager::SongChangeRequestProcessed(const QUrl &url, const bool valid) {

  const QList<Playlist*> playlists = GetAllPlaylists();
  for (Playlist *playlist : playlists) {
    if (playlist->ApplyValidityOnCurrentSong(url, valid)) {
      return;
    }
  }

}

void PlaylistManager::InsertUrls(const int id, const QList<QUrl> &urls, const int pos, const bool play_now, const bool enqueue) {

  Q_ASSERT(playlists_.contains(id));

  playlists_[id].p->InsertUrls(urls, pos, play_now, enqueue);

}

void PlaylistManager::InsertSongs(const int id, const SongList &songs, const int pos, const bool play_now, const bool enqueue) {

  Q_ASSERT(playlists_.contains(id));

  playlists_[id].p->InsertSongs(songs, pos, play_now, enqueue);

}

void PlaylistManager::RemoveItemsWithoutUndo(const int id, const QList<int> &indices) {

  Q_ASSERT(playlists_.contains(id));

  playlists_[id].p->RemoveItemsWithoutUndo(indices);

}

void PlaylistManager::RemoveCurrentSong() const {
  active()->removeRows(active()->current_index().row(), 1);
}

void PlaylistManager::RemoveDeletedSongs() {

  const QList<Playlist*> playlists = GetAllPlaylists();
  for (Playlist *playlist : playlists) {
    playlist->RemoveDeletedSongs();
  }

}

void PlaylistManager::Open(const int id) {

  if (playlists_.contains(id)) {
    return;
  }

  const PlaylistBackend::Playlist &p = playlist_backend_->GetPlaylist(id);
  if (p.id != id) {
    return;
  }

  AddPlaylist(p.id, p.name, p.special_type, p.ui_path, p.favorite);

}

void PlaylistManager::SetCurrentOrOpen(const int id) {

  Open(id);
  SetCurrentPlaylist(id);

}

bool PlaylistManager::IsPlaylistOpen(const int id) {
  return playlists_.contains(id);
}

void PlaylistManager::PlaySmartPlaylist(PlaylistGeneratorPtr generator, bool as_new, bool clear) {

  if (as_new) {
    New(generator->name());
  }

  if (clear) {
    current()->Clear();
  }

  current()->InsertSmartPlaylist(generator);

}

void PlaylistManager::RateCurrentSong(const float rating) {
  active()->RateSong(active()->current_index(), rating);
}

void PlaylistManager::RateCurrentSong2(const int rating) {
  RateCurrentSong(static_cast<float>(rating) / 5.0F);
}

void PlaylistManager::SaveAllPlaylists() {

  SavePlaylistsDialog dialog(parser()->file_extensions(PlaylistParser::Type::Save), parser()->default_extension());
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString path = dialog.path();
  if (path.isEmpty() || !QDir().exists(path)) return;

  QString extension = dialog.extension();
  if (extension.isEmpty()) extension = parser()->default_extension();

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  PlaylistSettings::PathType path_type = static_cast<PlaylistSettings::PathType>(s.value(PlaylistSettings::kPathType, static_cast<int>(PlaylistSettings::PathType::Automatic)).toInt());
  s.endGroup();
  if (path_type == PlaylistSettings::PathType::Ask_User) {
    PlaylistSaveOptionsDialog optionsdialog;
    optionsdialog.setModal(true);
    if (optionsdialog.exec() != QDialog::Accepted) return;
    path_type = optionsdialog.path_type();
  }

  for (QMap<int, Data>::const_iterator it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
    const Data &data = *it;
    const QString filepath = path + QLatin1Char('/') + data.name + QLatin1Char('.') + extension;
    Save(it.key(), data.name, filepath, path_type);
  }

}
