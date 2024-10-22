/*
 * Strawberry Music Player
 * Copyright 2013-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QList>
#include <QString>
#include <QImage>
#include <QAction>

#include "core/song.h"
#include "contextalbum.h"

class QMenu;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QGridLayout;
class QScrollArea;
class QSpacerItem;
class QResizeEvent;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;

class ResizableTextEdit;
class CollectionView;
class AlbumCoverChoiceController;
class LyricsProviders;
class LyricsFetcher;

class ContextView : public QWidget {
  Q_OBJECT

 public:
  explicit ContextView(QWidget *parent = nullptr);

  void Init(CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller, SharedPtr<LyricsProviders> lyrics_providers);

  ContextAlbum *album_widget() const { return widget_album_; }
  bool album_enabled() const { return action_show_album_->isChecked(); }
  Song song_playing() const { return song_playing_; }

 protected:
  void resizeEvent(QResizeEvent *e) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void dragEnterEvent(QDragEnterEvent*) override;
  void dropEvent(QDropEvent*) override;

 private:
  void AddActions();
  static void SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def = QString());
  void NoSong();
  void SetSong();
  void UpdateSong(const Song &song);
  void ResetSong();
  void GetCoverAutomatically();
  void SearchLyrics();
  void UpdateFonts();

 Q_SIGNALS:
  void AlbumEnabledChanged();

 private Q_SLOTS:
  void ActionShowAlbum();
  void ActionShowData();
  void ActionShowLyrics();
  void ActionSearchLyrics();
  void UpdateNoSong();
  void FadeStopFinished();
  void UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics);

 public Q_SLOTS:
  void ReloadSettings();
  void Playing();
  void Stopped();
  void Error();
  void SongChanged(const Song &song);
  void AlbumCoverLoaded(const Song &song, const QImage &image);

 private:
  CollectionView *collectionview_;
  AlbumCoverChoiceController *album_cover_choice_controller_;
  LyricsFetcher *lyrics_fetcher_;

  QMenu *menu_options_;
  QAction *action_show_album_;
  QAction *action_show_data_;
  QAction *action_show_lyrics_;
  QAction *action_search_lyrics_;

  QVBoxLayout *layout_container_;
  QWidget *widget_scrollarea_;
  QVBoxLayout *layout_scrollarea_;
  QScrollArea *scrollarea_;
  ResizableTextEdit *textedit_top_;
  ContextAlbum *widget_album_;
  QStackedWidget *widget_stacked_;
  QWidget *widget_stop_;
  QWidget *widget_play_;
  QVBoxLayout *layout_stop_;
  QVBoxLayout *layout_play_;
  QLabel *label_stop_summary_;
  QWidget *widget_play_data_;
  QGridLayout *layout_play_data_;
  ResizableTextEdit *textedit_play_lyrics_;

  QSpacerItem *spacer_play_data_;

  QLabel *label_filetype_title_;
  QLabel *label_length_title_;
  QLabel *label_samplerate_title_;
  QLabel *label_bitdepth_title_;
  QLabel *label_bitrate_title_;

  QLabel *label_ebur128_integrated_loudness_title_;
  QLabel *label_ebur128_loudness_range_title_;

  QLabel *label_filetype_;
  QLabel *label_length_;
  QLabel *label_samplerate_;
  QLabel *label_bitdepth_;
  QLabel *label_bitrate_;

  QLabel *label_ebur128_integrated_loudness_;
  QLabel *label_ebur128_loudness_range_;

  Song song_playing_;
  Song song_prev_;
  QImage image_original_;
  bool lyrics_tried_;
  qint64 lyrics_id_;
  QString lyrics_;
  QString title_fmt_;
  QString summary_fmt_;
  QFont font_headline_;
  QFont font_normal_;
  QFont font_nosong_;

  QList<QLabel*> labels_play_;
  QList<ResizableTextEdit*> textedit_play_;
  QList<QLabel*> labels_play_data_;
  QList<QLabel*> labels_play_all_;
};

#endif  // CONTEXTVIEW_H
