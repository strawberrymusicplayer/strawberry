/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTCONTAINER_H
#define PLAYLISTCONTAINER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QSettings>

class QTimer;
class QTimeLine;
class QLabel;
class QAction;
class QEvent;
class QKeyEvent;
class QResizeEvent;

class Playlist;
class PlaylistManager;
class PlaylistView;

class Ui_PlaylistContainer;

#include "includes/shared_ptr.h"
#include "core/settings.h"

class PlaylistContainer : public QWidget {
  Q_OBJECT

 public:
  explicit PlaylistContainer(QWidget *parent = nullptr);
  ~PlaylistContainer() override;

  void SetActions(QAction *new_playlist, QAction *load_playlist, QAction *save_playlist, QAction *clear_playlist, QAction *next_playlist, QAction *previous_playlist, QAction *save_all_playlists);
  void SetManager(SharedPtr<PlaylistManager> manager);
  void ReloadSettings();

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

  PlaylistView *view() const;

  bool eventFilter(QObject *objectWatched, QEvent *event) override;

 Q_SIGNALS:
  void UndoRedoActionsChanged(QAction *undo, QAction *redo);
  void ViewSelectionModelChanged();

 protected:
  // QWidget
  void resizeEvent(QResizeEvent*) override;

 private Q_SLOTS:
  void NewPlaylist();
  void LoadPlaylist();
  void SaveCurrentPlaylist() { SavePlaylist(-1); }
  void SavePlaylist(const int id);
  void ClearPlaylist();
  void GoToNextPlaylistTab();
  void GoToPreviousPlaylistTab();

  void SetViewModel(Playlist *playlist, const int scroll_position);
  void PlaylistAdded(const int id, const QString &name, bool favorite);
  void PlaylistClosed(const int id);
  void PlaylistRenamed(const int id, const QString &new_name);

  void Started();

  void Save();

  void SetTabBarVisible(const bool visible);
  void SetTabBarHeight(const int height);

  void SelectionChanged();
  void MaybeUpdateFilter();
  void UpdateFilter();
  void FocusOnFilter(QKeyEvent *event);

  void UpdateNoMatchesLabel();

 public Q_SLOTS:
  void ActivePlaying();
  void ActivePaused();
  void ActiveStopped();

 private:
  void UpdateActiveIcon(const QIcon &icon);
  void RepositionNoMatchesLabel(bool force = false);

 private:
  Ui_PlaylistContainer *ui_;

  SharedPtr<PlaylistManager> manager_;
  QAction *undo_;
  QAction *redo_;
  Playlist *playlist_;

  Settings settings_;
  bool starting_up_;

  bool tab_bar_visible_;
  QTimeLine *tab_bar_animation_;

  QLabel *no_matches_label_;

  QTimer *filter_timer_;
};

#endif  // PLAYLISTCONTAINER_H
