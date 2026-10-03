/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef STREAMINGSEARCHVIEW_H
#define STREAMINGSEARCHVIEW_H

#include "config.h"

#include <QWidget>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QPersistentModelIndex>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "collection/collectionmodel.h"
#include "covermanager/albumcoverloaderresult.h"
#include "streamingservice.h"
#include "streamingsearchmodel.h"

class QMimeData;
class QTimer;
class QMenu;
class QAction;
class QActionGroup;
class QEvent;
class QKeyEvent;
class QShowEvent;
class QContextMenuEvent;
class QTimerEvent;

class MimeData;
class AlbumCoverLoader;
class GroupByDialog;
class StreamingSearchSortModel;
class Ui_StreamingSearchView;

class StreamingSearchView : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingSearchView(QWidget *parent = nullptr);
  ~StreamingSearchView() override;

  void Init(const SharedPtr<StreamingService> service, const SharedPtr<AlbumCoverLoader> albumcover_loader);

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

  void LazyLoadAlbumCover(const QModelIndex &proxy_index);

 protected:
  void showEvent(QShowEvent *e) override;
  bool eventFilter(QObject *object, QEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

 private:
  struct DelayedSearch {
    int id_{};
    QString query_;
    StreamingService::SearchType type_;
  };

  struct CoverLoaderTask {
    QPersistentModelIndex index_;
    QString pixmap_cache_key_;
  };

  bool SearchKeyEvent(QKeyEvent *e);
  bool ResultsContextMenuEvent(QContextMenuEvent *e);

  StreamingSearchModel::ResultList SelectedResults();
  MimeData *SelectedMimeData();
  SongList SelectedSongs();

  void SetSearchType(const StreamingService::SearchType type);

  int SearchAsync(const QString &query, const StreamingService::SearchType type);
  void SearchAsync(const int id, const QString &query, const StreamingService::SearchType type);
  void SearchError(const int id, const QString &error);
  void CancelSearch(const int id);
  // Whether the service's search ID belongs to the search that is shown.
  bool IsCurrentSearch(const int service_id) const;

  QString PixmapCacheKey(const Song &song) const;
  // Returns the results for the songs that are not shown yet, and remembers them as shown.
  StreamingSearchModel::ResultList NewResults(const SongMap &songs);

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *mimedata);
  void AddArtistsSignal(const SongList &songs);
  void AddAlbumsSignal(const SongList &songs);
  void AddSongsSignal(const SongList &songs);
  void OpenSettingsDialog(const Song::Source source);

 private Q_SLOTS:
  void SwapModels();
  void TextEdited(const QString &text);
  void StartSearch(const QString &query);
  void SearchDone(const int service_id, const SongMap &songs, const QString &error);
  void SearchSongsAvailable(const int service_id, const SongMap &songs);

  void UpdateStatus(const int service_id, const QString &text);
  void ProgressSetMaximum(const int service_id, const int max);
  void UpdateProgress(const int service_id, const int progress);
  void AddResults(const int search_id, const StreamingSearchModel::ResultList &results);

  void FocusOnFilter(QKeyEvent *e);

  void AddSelectedToPlaylist();
  void LoadSelected();
  void OpenSelectedInNewPlaylist();
  void AddSelectedToPlaylistEnqueue();
  void AddArtists();
  void AddAlbums();
  void AddSongs();
  void SearchForThis();
  void Configure();

  void SearchArtistsClicked(const bool checked);
  void SearchAlbumsClicked(const bool checked);
  void SearchSongsClicked(const bool checked);
  void GroupByClicked(QAction *action);
  void SetGroupBy(const CollectionModel::Grouping g);

  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &albumcover_result);

 public Q_SLOTS:
  void ReloadSettings();

 private:
  SharedPtr<StreamingService> service_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;

  Ui_StreamingSearchView *ui_;
  ScopedPtr<GroupByDialog> group_by_dialog_;

  QMenu *context_menu_;
  QList<QAction*> context_actions_;
  QAction *search_for_this_;
  QActionGroup *group_by_actions_;

  // Like graphics APIs have a front buffer and a back buffer, there's a front model and a back model
  // The front model is the one that's shown in the UI and the back model is the one that lies in wait.
  // current_model_ will point to either the front or the back model.
  StreamingSearchModel *front_model_;
  StreamingSearchModel *back_model_;
  StreamingSearchModel *current_model_;

  StreamingSearchSortModel *front_proxy_;
  StreamingSearchSortModel *back_proxy_;

  QTimer *swap_models_timer_;

  bool use_pretty_covers_;
  bool show_search_album_edition_;
  bool show_search_album_quality_;
  StreamingService::SearchType search_type_;
  bool search_error_;
  int last_search_id_;
  int searches_next_id_;

  QMap<int, DelayedSearch> delayed_searches_;
  // Maps the service's search ID to our search ID.
  QMap<int, int> pending_searches_;
  // The IDs of the songs shown for the current search, since they are shown as they are received.
  QSet<QString> shown_song_ids_;

  QMap<quint64, CoverLoaderTask> cover_loader_tasks_;
};

#endif  // STREAMINGSEARCHVIEW_H
