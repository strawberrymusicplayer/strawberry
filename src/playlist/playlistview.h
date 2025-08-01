/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
#include <QTreeView>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QRect>
#include <QRegion>
#include <QStyleOption>
#include <QPoint>
#include <QBasicTimer>

#include "core/song.h"
#include "covermanager/albumcoverloaderresult.h"
#include "constants/appearancesettings.h"
#include "playlist.h"

class QWidget;
class QTimer;
class QTimeLine;
class QPainter;
class QEvent;
class QShowEvent;
class QContextMenuEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QTimerEvent;

class Player;
class CollectionBackend;
class PlaylistManager;
class CurrentAlbumCoverLoader;
class PlaylistHeader;
class PlaylistProxyStyle;
class DynamicPlaylistControls;
class RatingItemDelegate;

#ifdef HAVE_MOODBAR
class MoodbarLoader;
#endif

class PlaylistView : public QTreeView {
  Q_OBJECT

 public:
  explicit PlaylistView(QWidget *parent = nullptr);
  ~PlaylistView() override;

  static ColumnAlignmentMap DefaultColumnAlignment();

  void Init(const SharedPtr<Player> player,
            const SharedPtr<PlaylistManager> playlist_manager,
            const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
            const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
            const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader);

  void SetItemDelegates();
  void SetPlaylist(Playlist *playlist);
  void RemoveSelected();

  void SetReadOnlySettings(const bool read_only) { read_only_settings_ = read_only; }

  Playlist *playlist() const { return playlist_; }
  AppearanceSettings::BackgroundImageType background_image_type() const { return background_image_type_; }
  Qt::Alignment column_alignment(int section) const;

  void ResetHeaderState();

  // QTreeView
  void setModel(QAbstractItemModel *model) override;

 public Q_SLOTS:
  void ReloadSettings();
  void SaveSettings();
  void SetColumnAlignment(const int section, const Qt::Alignment alignment);
  void JumpToCurrentlyPlayingTrack();

 Q_SIGNALS:
  void PlayItem(const QModelIndex idx, const Playlist::AutoScroll autoscroll);
  void PlayPause(const quint64 offset_nanosec = 0, const Playlist::AutoScroll autoscroll = Playlist::AutoScroll::Never);
  void RightClicked(const QPoint global_pos, const QModelIndex idx);
  void SeekForward();
  void SeekBackward();
  void FocusOnFilterSignal(QKeyEvent *event);
  void BackgroundPropertyChanged();
  void ColumnAlignmentChanged(const ColumnAlignmentMap alignment);

 protected:
  // QWidget
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *e) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void timerEvent(QTimerEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent*) override;
  void paintEvent(QPaintEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  bool eventFilter(QObject *object, QEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  // QTreeView
  void drawTree(QPainter *painter, const QRegion &region) const;
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;

  // QAbstractScrollArea
  void scrollContentsBy(const int dx, const int dy) override;

  // QAbstractItemView
  void rowsInserted(const QModelIndex &parent, const int start, const int end) override;
  void closeEditor(QWidget *editor, const QAbstractItemDelegate::EndEditHint hint) override;

 private Q_SLOTS:
  void Update() { update(); }
  void SetHeaderState();
  void HeaderSectionResized(const int logical_index, const int old_size, const int new_size);
  void InhibitAutoscrollTimeout();
  void MaybeAutoscroll(const Playlist::AutoScroll autoscroll);
  void InvalidateCachedCurrentPixmap();
  void PlaylistDestroyed();
  void StretchChanged(const bool stretch);
  void FadePreviousBackgroundImage(const qreal value);
  void StopGlowing();
  void StartGlowing();
  void JumpToLastPlayedTrack();
  void CopyCurrentSongToClipboard() const;
  void Playing();
  void Stopped();
  void SongChanged(const Song &song);
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result = AlbumCoverLoaderResult());
  void DynamicModeChanged(const bool dynamic);
  void SetRatingLockStatus(const bool state);
  void RatingHoverIn(const QModelIndex &idx, const QPoint pos);
  void RatingHoverOut();

 private:
  void LoadHeaderState();
  void RestoreHeaderState();

  void ReloadBarPixmaps();
  QList<QPixmap> LoadBarPixmap(const QString &filename, const bool keep_aspect_ratio);
  void LoadTinyPlayPausePixmaps(const int desired_size);
  void UpdateCachedCurrentRowPixmap(QStyleOptionViewItem option, const QModelIndex &idx);

  void set_background_image_type(AppearanceSettings::BackgroundImageType bg) {
    background_image_type_ = bg;
    Q_EMIT BackgroundPropertyChanged();  // clazy:exclude=incorrect-emit
  }
  // Save image as the background_image_ after applying some modifications (opacity, ...).
  // Should be used instead of modifying background_image_ directly
  void set_background_image(const QImage &image);

  void GlowIntensityChanged();

 private:
  QList<int> GetEditableColumns();
  QModelIndex NextEditableIndex(const QModelIndex &current);
  QModelIndex PrevEditableIndex(const QModelIndex &current);

  void RepositionDynamicControls();

  SharedPtr<Player> player_;
  SharedPtr<PlaylistManager> playlist_manager_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
#ifdef HAVE_MOODBAR
  SharedPtr<MoodbarLoader> moodbar_loader_;
#endif

  PlaylistProxyStyle *style_;
  Playlist *playlist_;
  PlaylistHeader *header_;

  qreal device_pixel_ratio_;
  AppearanceSettings::BackgroundImageType background_image_type_;
  QString background_image_filename_;
  AppearanceSettings::BackgroundImagePosition background_image_position_;
  int background_image_maxsize_;
  bool background_image_stretch_;
  bool background_image_do_not_cut_;
  bool background_image_keep_aspect_ratio_;
  int blur_radius_;
  int opacity_level_;

  bool background_initialized_;
  bool set_initial_header_layout_;
  bool header_state_loaded_;
  bool header_state_restored_;
  bool read_only_settings_;

  QImage background_image_;
  QImage current_song_cover_art_;
  QPixmap cached_scaled_background_image_;

  // For fading when image change
  QPixmap previous_background_image_;
  qreal previous_background_image_opacity_;
  QTimeLine *fade_animation_;

  // To know if we should redraw the background or not
  bool force_background_redraw_;
  int last_height_;
  int last_width_;
  int current_background_image_x_;
  int current_background_image_y_;
  int previous_background_image_x_;
  int previous_background_image_y_;

  bool bars_enabled_;
  bool glow_enabled_;
  bool select_track_;
  bool auto_sort_;

  bool currently_glowing_;
  QBasicTimer glow_timer_;
  int glow_intensity_step_;
  QModelIndex last_current_item_;
  QRect last_glow_rect_;

  QTimer *inhibit_autoscroll_timer_;
  bool inhibit_autoscroll_;
  bool currently_autoscrolling_;

  int row_height_;  // Used to invalidate the currenttrack_bar pixmaps
  QList<QPixmap> currenttrack_bar_left_;
  QList<QPixmap> currenttrack_bar_mid_;
  QList<QPixmap> currenttrack_bar_right_;
  QPixmap currenttrack_play_;
  QPixmap currenttrack_pause_;

  QRegion current_paint_region_;
  QPixmap cached_current_row_;
  QRect cached_current_row_rect_;
  int cached_current_row_row_;

  QPixmap cached_tree_;
  int drop_indicator_row_;
  bool drag_over_;

  QByteArray header_state_;
  ColumnAlignmentMap column_alignment_;
  bool rating_locked_;

  Song song_playing_;

  DynamicPlaylistControls *dynamic_controls_;
  RatingItemDelegate *rating_delegate_;

  QColor playlist_playing_song_color_;

  QPixmap pixmap_tinyplay_;
  QPixmap pixmap_tinypause_;
};

#endif  // PLAYLISTVIEW_H
