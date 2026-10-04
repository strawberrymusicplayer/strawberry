/*
 * Strawberry Music Player
 * Copyright 2013-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "lyrics/lyricssearchresult.h"
#include "utilities/lrcutils.h"
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
class QTimer;
class QDragEnterEvent;
class QDropEvent;

class ResizableTextEdit;
class CollectionView;
class AlbumCoverChoiceController;
class LyricsProviders;
class LyricsFetcher;
class Player;

class ContextView : public QWidget {
  Q_OBJECT

 public:
  explicit ContextView(QWidget *parent = nullptr);

  void Init(CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller, SharedPtr<Player> player, SharedPtr<LyricsProviders> lyrics_providers);

  ContextAlbum *album_widget() const { return widget_album_; }
  bool album_enabled() const { return action_show_album_->isChecked(); }
  Song song_playing() const { return song_playing_; }

 protected:
  void resizeEvent(QResizeEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dropEvent(QDropEvent *e) override;

 private:
  void AddActions();
  static void SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def = QString());
  void NoSong();
  void SetSong();
  void UpdateSong(const Song &song);
  void ResetSong();
  void GetCoverAutomatically();
  void SearchLyrics();
  void SetLyricsText();
  void SetSyncedLyricsText(const QString &provider);
  void SetSyncedLyricsLineBold(const qsizetype line, const bool bold);
  qint64 SyncedLyricsPositionMsec() const;
  qsizetype SyncedLyricsLine(const qint64 position_msec) const;
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
  void UpdateLyrics(const quint64 id, const LyricsSearchResult &result);
  void UpdateSyncedLyricsPosition();

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
  SharedPtr<Player> player_;
  LyricsFetcher *lyrics_fetcher_;
  QTimer *timer_synced_lyrics_;

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

  QLabel *label_filetype_;
  QLabel *label_length_;
  QLabel *label_samplerate_;
  QLabel *label_bitdepth_;
  QLabel *label_bitrate_;

  Song song_playing_;
  Song song_prev_;
  QImage image_original_;
  bool lyrics_tried_;
  qint64 lyrics_id_;
  QString lyrics_;
  QString synced_lyrics_;
  QString fetched_lyrics_;
  QString fetched_synced_lyrics_;
  QString fetched_lyrics_provider_;
  QString synced_lyrics_shown_;
  bool synced_lyrics_enabled_;
  Utilities::LRCLines synced_lyrics_lines_;
  qsizetype synced_lyrics_line_;
  qint64 synced_lyrics_offset_msec_;
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
