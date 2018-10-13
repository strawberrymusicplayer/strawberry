/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef DEEZERSEARCHVIEW_H
#define DEEZERSEARCHVIEW_H

#include "config.h"

#include <QWidget>
#include <QObject>
#include <QTimer>
#include <QMap>
#include <QList>
#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QMimeData>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QAction>
#include <QActionGroup>
#include <QtEvents>

#include "collection/collectionmodel.h"
#include "settings/settingsdialog.h"
#include "playlist/playlistmanager.h"
#include "deezersearch.h"
#include "settings/deezersettingspage.h"

class Application;
class GroupByDialog;
class DeezerSearchModel;
class Ui_DeezerSearchView;

class DeezerSearchView : public QWidget {
  Q_OBJECT

 public:
  DeezerSearchView(Application *app, QWidget *parent = nullptr);
  ~DeezerSearchView();

  static const int kSwapModelsTimeoutMsec;

  void LazyLoadArt(const QModelIndex &index);

  void showEvent(QShowEvent *e);
  void hideEvent(QHideEvent *e);
  bool eventFilter(QObject *object, QEvent *event);

 public slots:
  void ReloadSettings();
  void StartSearch(const QString &query);
  void FocusSearchField();
  void OpenSettingsDialog();

signals:
  void AddToPlaylist(QMimeData *data);

 private slots:
  void SwapModels();
  void TextEdited(const QString &text);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int progress);
  void UpdateProgress(int max);
  void AddResults(int id, const DeezerSearch::ResultList &results);
  void SearchError(const int id, const QString error);
  void ArtLoaded(int id, const QPixmap &pixmap);

  void FocusOnFilter(QKeyEvent *event);

  void AddSelectedToPlaylist();
  void LoadSelected();
  void OpenSelectedInNewPlaylist();
  void AddSelectedToPlaylistEnqueue();

  void SearchForThis();

  void SearchBySongsClicked(bool);
  void SearchByAlbumsClicked(bool);
  void GroupByClicked(QAction *action);
  void SetSearchBy(DeezerSettingsPage::SearchBy searchby);
  void SetGroupBy(const CollectionModel::Grouping &g);

 private:
  MimeData *SelectedMimeData();

  bool SearchKeyEvent(QKeyEvent *event);
  bool ResultsContextMenuEvent(QContextMenuEvent *event);

  Application *app_;
  DeezerSearch *engine_;
  Ui_DeezerSearchView *ui_;
  QScopedPointer<GroupByDialog> group_by_dialog_;

  QMenu *context_menu_;
  QList<QAction*> context_actions_;
  QActionGroup *group_by_actions_;

  int last_search_id_;

  // Like graphics APIs have a front buffer and a back buffer, there's a front model and a back model
  // The front model is the one that's shown in the UI and the back model is the one that lies in wait.
  // current_model_ will point to either the front or the back model.
  DeezerSearchModel *front_model_;
  DeezerSearchModel *back_model_;
  DeezerSearchModel *current_model_;

  QSortFilterProxyModel *front_proxy_;
  QSortFilterProxyModel *back_proxy_;
  QSortFilterProxyModel *current_proxy_;

  QMap<int, QModelIndex> art_requests_;

  QTimer *swap_models_timer_;

  QIcon search_icon_;
  QIcon warning_icon_;

  DeezerSettingsPage::SearchBy searchby_;
  bool error_;

};

#endif  // DEEZERSEARCHVIEW_H
