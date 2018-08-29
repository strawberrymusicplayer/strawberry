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

  void SetApplication(Application *app);
  void SetCollectionView(CollectionView *collectionview);

  ContextAlbumsView *albums() { return ui_->widget_play_albums; }

 public slots:
  void UpdateNoSong();
  void Playing();
  void Stopped();
  void Error();
  void SongChanged(const Song &song);
  void AlbumArtLoaded(const Song &song, const QString &uri, const QImage &image);

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ShowCover();
  void SearchCoverAutomatically();
  void AutomaticCoverSearchDone();
  void UpdateLyrics(quint64 id, const QString lyrics);

 private:

  enum WidgetState {
    //State_None = 0,
    State_Playing,
    State_Stopped
  };

  static const char *kSettingsGroup;
  static const int kPadding;
  static const int kGradientHead;
  static const int kGradientTail;
  static const int kMaxCoverSize;
  static const int kBottomOffset;
  static const int kTopBorder;

  Application *app_;
  Ui_ContextViewContainer *ui_;
  CollectionView *collectionview_;
  WidgetState widgetstate_;
  QMenu *menu_;
  QTimeLine *timeline_fade_;
  QImage image_strawberry_;
  AlbumCoverChoiceController *album_cover_choice_controller_;
  LyricsFetcher *lyrics_fetcher_;
  bool active_;
  bool downloading_covers_;

  QAction *action_show_data_;
  QAction *action_show_output_;
  QAction *action_show_albums_;
  QAction *action_show_lyrics_;
  AlbumCoverLoaderOptions cover_loader_options_;
  Song song_;
  Song song_empty_;
  Song song_prev_;
  QImage image_original_;
  QImage image_previous_;
  QPixmap *pixmap_album_;
  QPixmap pixmap_current_;
  QPixmap pixmap_previous_;
  QPainter *painter_album_;
  qreal pixmap_previous_opacity_;
  std::unique_ptr<QMovie> spinner_animation_;

  QString prev_artist_;
  QString lyrics_;

  void LoadSettings();
  void AddActions();
  void SetText(QLabel *label, int value, const QString &suffix, const QString &def = QString());
  void NoSong();
  void UpdateSong();
  void SetImage(const QImage &image);
  void DrawImage(QPainter *p);
  void ScaleCover();
  bool GetCoverAutomatically();

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
  void FadePreviousTrack(qreal value);

};

#endif  // CONTEXTVIEW_H

