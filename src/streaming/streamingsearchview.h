/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef STREAMINGSEARCHVIEW_H
#define STREAMINGSEARCHVIEW_H

#include "config.h"

#include <QWidget>
#include <QPair>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QMetaType>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "collection/collectionmodel.h"
#include "covermanager/albumcoverloaderresult.h"
#include "streamingservice.h"

class QSortFilterProxyModel;
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
class StreamingSearchModel;
class Ui_StreamingSearchView;

class StreamingSearchView : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingSearchView(QWidget *parent = nullptr);
  ~StreamingSearchView() override;

  struct Result {
    Song metadata_;
    QString pixmap_cache_key_;
  };
  using ResultList = QList<Result>;

  void Init(const SharedPtr<StreamingService> service, const SharedPtr<AlbumCoverLoader> albumcover_loader);

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

  void LazyLoadAlbumCover(const QModelIndex &proxy_index);

 protected:
  struct PendingState {
    PendingState() : orig_id_(-1) {}
    PendingState(int orig_id, const QStringList &tokens) : orig_id_(orig_id), tokens_(tokens) {}
    int orig_id_;
    QStringList tokens_;

    bool operator<(const PendingState &b) const {
      return orig_id_ < b.orig_id_;
    }

    bool operator==(const PendingState &b) const {
      return orig_id_ == b.orig_id_;
    }
  };

  void showEvent(QShowEvent *e) override;
  bool eventFilter(QObject *object, QEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

  // These functions treat queries in the same way as CollectionQuery.
  // They're useful for figuring out whether you got a result because it matched in the song title or the artist/album name.
  static QStringList TokenizeQuery(const QString &query);
  static bool Matches(const QStringList &tokens, const QString &string);

 private:
  struct DelayedSearch {
    int id_;
    QString query_;
    StreamingService::SearchType type_;
  };

  bool SearchKeyEvent(QKeyEvent *e);
  bool ResultsContextMenuEvent(QContextMenuEvent *e);

  MimeData *SelectedMimeData();

  void SetSearchType(const StreamingService::SearchType type);

  int SearchAsync(const QString &query, const StreamingService::SearchType type);
  void SearchAsync(const int id, const QString &query, const StreamingService::SearchType type);
  void SearchError(const int id, const QString &error);
  void CancelSearch(const int id);

  QString PixmapCacheKey(const Result &result) const;
  bool FindCachedPixmap(const Result &result, QPixmap *pixmap) const;
  int LoadAlbumCoverAsync(const Result &result);

 Q_SIGNALS:
  void AddToPlaylist(QMimeData*);
  void AddArtistsSignal(const SongList &songs);
  void AddAlbumsSignal(const SongList &songs);
  void AddSongsSignal(const SongList &songs);
  void OpenSettingsDialog(const Song::Source source);

 private Q_SLOTS:
  void SwapModels();
  void TextEdited(const QString &text);
  void StartSearch(const QString &query);
  void SearchDone(const int service_id, const SongMap &songs, const QString &error);

  void UpdateStatus(const int service_id, const QString &text);
  void ProgressSetMaximum(const int service_id, const int max);
  void UpdateProgress(const int service_id, const int progress);
  void AddResults(const int service_id, const StreamingSearchView::ResultList &results);

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
  QActionGroup *group_by_actions_;

  // Like graphics APIs have a front buffer and a back buffer, there's a front model and a back model
  // The front model is the one that's shown in the UI and the back model is the one that lies in wait.
  // current_model_ will point to either the front or the back model.
  StreamingSearchModel *front_model_;
  StreamingSearchModel *back_model_;
  StreamingSearchModel *current_model_;

  QSortFilterProxyModel *front_proxy_;
  QSortFilterProxyModel *back_proxy_;
  QSortFilterProxyModel *current_proxy_;

  QTimer *swap_models_timer_;

  bool use_pretty_covers_;
  StreamingService::SearchType search_type_;
  bool search_error_;
  int last_search_id_;
  int searches_next_id_;

  QMap<int, DelayedSearch> delayed_searches_;
  QMap<int, PendingState> pending_searches_;

  QMap<quint64, QPair<QModelIndex, QString>> cover_loader_tasks_;
};
Q_DECLARE_METATYPE(StreamingSearchView::Result)
Q_DECLARE_METATYPE(StreamingSearchView::ResultList)

#endif  // STREAMINGSEARCHVIEW_H
