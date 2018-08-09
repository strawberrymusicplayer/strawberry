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

#ifndef STATUSVIEW_H
#define STATUSVIEW_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QMovie>
#include <QPainter>
#include <QTimeLine>
#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QBoxLayout>
#include <QtEvents>

#include "core/song.h"
#include "covermanager/albumcoverloaderoptions.h"

class QEvent;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;

class Application;
class CollectionView;
class CollectionViewContainer;
class AlbumCoverChoiceController;

class StatusView : public QWidget {
  Q_OBJECT

public:
  StatusView(CollectionViewContainer *collectionviewcontainer, QWidget *parent = nullptr);
  ~StatusView();

  static const char* kSettingsGroup;
  static const int kPadding;
  static const int kGradientHead;
  static const int kGradientTail;
  static const int kMaxCoverSize;
  static const int kBottomOffset;
  static const int kTopBorder;

  void SetApplication(Application *app);

public slots:
  void SongChanged(const Song &song);
  void SongFinished();
  void AlbumArtLoaded(const Song& metadata, const QString &uri, const QImage &image);
  void FadePreviousTrack(qreal value);

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ShowCover();
  void SearchCoverAutomatically();
  void AutomaticCoverSearchDone();
   
private:
  QVBoxLayout *layout_;
  QScrollArea *scrollarea_;
  QVBoxLayout *container_layout_;
  QWidget *container_widget_;

  QWidget *widget_stopped_;
  QWidget *widget_playing_;
  QVBoxLayout *layout_playing_;
  QVBoxLayout *layout_stopped_;
  QLabel *label_stopped_top_;
  QLabel *label_stopped_logo_;
  QLabel *label_stopped_text_;
  QLabel *label_playing_top_;
  QLabel *label_playing_album_;
  QLabel *label_playing_text_;

  QPixmap *pixmap_album_;
  QPainter *painter_album_;
  
  CollectionView *collectionview_;
  
  AlbumCoverLoaderOptions cover_loader_options_;
  
  QImage original_;
  
  void CreateWidget();
  void NoSongWidget();
  void SongWidget();
  void AddActions();
  void SetImage(const QImage &image);
  void DrawImage(QPainter *p);
  void ScaleCover();
  bool GetCoverAutomatically();
  
  Application *app_;
  AlbumCoverChoiceController *album_cover_choice_controller_;

  QAction *fit_cover_width_action_;

  bool visible_;
  int small_ideal_height_;
  int total_height_;
  bool fit_width_;
  QTimeLine *fade_animation_;
  QImage image_blank_;
  QImage image_nosong_;

  // Information about the current track
  Song metadata_;
  QPixmap pixmap_current_;

  // Holds the last track while we're fading to the new track
  QPixmap pixmap_previous_;
  qreal pixmap_previous_opacity_;

  std::unique_ptr<QMovie> spinner_animation_;
  bool downloading_covers_;
  bool stopped_;
  bool playing_;
  
  enum WidgetState {
    None = 0,
    Playing,
    Stopped
  };
  WidgetState widgetstate_;
  QMenu *menu_;

protected:
  bool eventFilter(QObject *, QEvent *);
  void handlePaintEvent(QObject *object, QEvent *event);
  void paintEvent_album(QEvent *event);
  void contextMenuEvent(QContextMenuEvent *e);
  void mouseReleaseEvent(QMouseEvent *);
  void dragEnterEvent(QDragEnterEvent *e);
  void dropEvent(QDropEvent *e);
  void UpdateSong();
  void NoSong();
  void SwitchWidgets(WidgetState state);

 private slots:
  void UpdateNoSong();

};

#endif  // STATUSVIEW_H

