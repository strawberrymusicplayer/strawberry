/*
 * Strawberry Music Player
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#ifndef CONTEXTVIEW_H
#define CONTEXTVIEW_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QMovie>
#include <QTimeLine>
#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QtEvents>

#include "core/song.h"
#include "covermanager/albumcoverloaderoptions.h"

#include "ui_contextviewcontainer.h"

using std::unique_ptr;

class Application;
class CollectionView;
class CollectionModel;
class AlbumCoverChoiceController;
class Ui_ContextViewContainer;
class ContextAlbumsView;
class LyricsFetcher;

class ContextView : public QWidget {
  Q_OBJECT

 public:
  ContextView(QWidget *parent = nullptr);
  ~ContextView();

  void Init(Application *app, CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller);

  ContextAlbumsView *albums() { return ui_->widget_play_albums; }

 public slots:
  void UpdateNoSong();
  void Playing();
  void Stopped();
  void Error();
  void SongChanged(const Song &song);

 private:
  static const char *kSettingsGroup;

  Ui_ContextViewContainer *ui_;
  Application *app_;
  CollectionView *collectionview_;
  AlbumCoverChoiceController *album_cover_choice_controller_;
  LyricsFetcher *lyrics_fetcher_;

  QMenu *menu_;
  QTimeLine *timeline_fade_;
  QImage image_strawberry_;
  bool active_;
  bool downloading_covers_;

  QAction *action_show_data_;
  QAction *action_show_output_;
  QAction *action_show_albums_;
  QAction *action_show_lyrics_;
  AlbumCoverLoaderOptions cover_loader_options_;
  Song song_;
  Song song_playing_;
  Song song_prev_;
  QImage image_original_;
  QImage image_previous_;
  QPixmap pixmap_current_;
  QPixmap pixmap_previous_;
  qreal pixmap_previous_opacity_;
  std::unique_ptr<QMovie> spinner_animation_;
  qint64 lyrics_id_;
  QString lyrics_;

  void AddActions();
  void SetLabelEnabled(QLabel *label);
  void SetLabelDisabled(QLabel *label);
  void SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def = QString());
  void NoSong();
  void SetSong(const Song &song);
  void UpdateSong(const Song &song);
  void SetImage(const QImage &image);
  void DrawImage(QPainter *p);
  void ScaleCover();
  void GetCoverAutomatically();

 protected:
  bool eventFilter(QObject *, QEvent *);
  void handlePaintEvent(QObject *object, QEvent *event);
  void PaintEventAlbum(QEvent *event);
  void contextMenuEvent(QContextMenuEvent *e);
  void mouseReleaseEvent(QMouseEvent *);
  void dragEnterEvent(QDragEnterEvent *e);
  void dropEvent(QDropEvent *e);

 private slots:
  void ActionShowData();
  void ActionShowOutput();
  void ActionShowAlbums();
  void ActionShowLyrics();
  void UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics);
  void SearchCoverAutomatically();
  void AutomaticCoverSearchDone();
  void AlbumArtLoaded(const Song &song, const QString &uri, const QImage &image);
  void FadePreviousTrack(qreal value);

};

#endif  // CONTEXTVIEW_H

