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

#include "config.h"

#include <utility>
#include <optional>
#include <algorithm>
#include <iterator>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QIcon>
#include <QFont>
#include <QSize>
#include <QSizePolicy>
#include <QMenu>
#include <QAction>
#include <QFontDatabase>
#include <QLayoutItem>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QScrollArea>
#include <QSpacerItem>
#include <QLabel>
#include <QTextEdit>
#include <QSettings>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTimer>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>

#include "core/song.h"
#include "core/settings.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/lrcutils.h"
#include "widgets/resizabletextedit.h"
#include "collection/collectionview.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "lyrics/lyricsfetcher.h"
#include "lyrics/lyricssearchresult.h"
#include "constants/contextsettings.h"
#include "constants/timeconstants.h"

#include "contextview.h"
#include "contextalbum.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kWidgetSpacing = 50;
constexpr qint64 kSyncedLyricsMaxUpdateTimeMs = 250;
}  // namespace

ContextView::ContextView(QWidget *parent)
    : QWidget(parent),
      collectionview_(nullptr),
      album_cover_choice_controller_(nullptr),
      lyrics_fetcher_(nullptr),
      timer_synced_lyrics_(new QTimer(this)),
      menu_options_(new QMenu(this)),
      action_show_album_(nullptr),
      action_show_data_(nullptr),
      action_show_lyrics_(nullptr),
      action_search_lyrics_(nullptr),
      layout_container_(new QVBoxLayout()),
      widget_scrollarea_(new QWidget(this)),
      layout_scrollarea_(new QVBoxLayout()),
      scrollarea_(new QScrollArea(this)),
      textedit_top_(new ResizableTextEdit(this)),
      widget_album_(new ContextAlbum(this)),
      widget_stacked_(new QStackedWidget(this)),
      widget_stop_(new QWidget(this)),
      widget_play_(new QWidget(this)),
      layout_stop_(new QVBoxLayout()),
      layout_play_(new QVBoxLayout()),
      label_stop_summary_(new QLabel(this)),
      widget_play_data_(new QWidget(this)),
      layout_play_data_(new QGridLayout()),
      textedit_play_lyrics_(new ResizableTextEdit(this)),
      spacer_play_data_(new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed)),
      label_filetype_title_(new QLabel(this)),
      label_length_title_(new QLabel(this)),
      label_samplerate_title_(new QLabel(this)),
      label_bitdepth_title_(new QLabel(this)),
      label_bitrate_title_(new QLabel(this)),
      label_filetype_(new QLabel(this)),
      label_length_(new QLabel(this)),
      label_samplerate_(new QLabel(this)),
      label_bitdepth_(new QLabel(this)),
      label_bitrate_(new QLabel(this)),
      lyrics_tried_(false),
      lyrics_id_(-1),
      synced_lyrics_enabled_(ContextSettings::kDefaultSyncedLyrics),
      synced_lyrics_line_(-1),
      synced_lyrics_offset_msec_(0) {

  setLayout(layout_container_);

  layout_container_->setObjectName(u"context-layout-container"_s);
  layout_container_->setContentsMargins(0, 0, 0, 0);
  layout_container_->addWidget(scrollarea_);

  scrollarea_->setObjectName(u"context-scrollarea"_s);
  scrollarea_->setWidgetResizable(true);
  scrollarea_->setWidget(widget_scrollarea_);
  scrollarea_->setContentsMargins(0, 0, 0, 0);
  scrollarea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollarea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  widget_scrollarea_->setObjectName(u"context-widget-scrollarea"_s);
  widget_scrollarea_->setLayout(layout_scrollarea_);
  widget_scrollarea_->setContentsMargins(0, 0, 0, 0);

  textedit_top_->setReadOnly(true);
  textedit_top_->setFrameShape(QFrame::NoFrame);

  layout_scrollarea_->setObjectName(u"context-layout-scrollarea"_s);
  layout_scrollarea_->setContentsMargins(15, 15, 15, 15);
  layout_scrollarea_->addWidget(textedit_top_);
  layout_scrollarea_->addWidget(widget_album_);
  layout_scrollarea_->addWidget(widget_stacked_);
  layout_scrollarea_->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Expanding));

  widget_stacked_->setContentsMargins(0, 0, 0, 0);
  widget_stacked_->addWidget(widget_stop_);
  widget_stacked_->addWidget(widget_play_);
  widget_stacked_->setCurrentWidget(widget_stop_);

  widget_stop_->setLayout(layout_stop_);
  widget_stop_->setContentsMargins(0, 0, 0, 0);
  widget_play_->setLayout(layout_play_);
  widget_play_->setContentsMargins(0, 0, 0, 0);

  layout_stop_->setContentsMargins(0, 0, 0, 0);
  layout_play_->setContentsMargins(0, 0, 0, 0);

  // Stopped

  label_stop_summary_->setAlignment(Qt::AlignLeft | Qt::AlignTop);

  layout_stop_->setContentsMargins(0, 0, 0, 0);
  layout_stop_->addWidget(label_stop_summary_);

  // Playing

  label_filetype_title_->setText(tr("Filetype"));
  label_length_title_->setText(tr("Length"));
  label_samplerate_title_->setText(tr("Samplerate"));
  label_bitdepth_title_->setText(tr("Bit depth"));
  label_bitrate_title_->setText(tr("Bitrate"));

  label_filetype_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_length_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_samplerate_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_bitdepth_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_bitrate_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  label_filetype_->setWordWrap(true);
  label_length_->setWordWrap(true);
  label_samplerate_->setWordWrap(true);
  label_bitdepth_->setWordWrap(true);
  label_bitrate_->setWordWrap(true);

  layout_play_data_->setContentsMargins(0, 0, 0, 0);
  layout_play_data_->addWidget(label_filetype_title_, 0, 0);
  layout_play_data_->addWidget(label_filetype_, 0, 1);
  layout_play_data_->addWidget(label_length_title_, 1, 0);
  layout_play_data_->addWidget(label_length_, 1, 1);
  layout_play_data_->addWidget(label_samplerate_title_, 2, 0);
  layout_play_data_->addWidget(label_samplerate_, 2, 1);
  layout_play_data_->addWidget(label_bitdepth_title_, 3, 0);
  layout_play_data_->addWidget(label_bitdepth_, 3, 1);
  layout_play_data_->addWidget(label_bitrate_title_, 4, 0);
  layout_play_data_->addWidget(label_bitrate_, 4, 1);

  widget_play_data_->setLayout(layout_play_data_);

  textedit_play_lyrics_->setReadOnly(true);
  textedit_play_lyrics_->setFrameShape(QFrame::NoFrame);
  // The active synchronized lyrics line is marked by changing the formatting, which should not end up in an undo stack.
  textedit_play_lyrics_->document()->setUndoRedoEnabled(false);
  textedit_play_lyrics_->hide();

  layout_play_->setContentsMargins(0, 0, 0, 0);
  layout_play_->addWidget(widget_play_data_);
  layout_play_->addSpacerItem(spacer_play_data_);
  layout_play_->addWidget(textedit_play_lyrics_);
  layout_play_->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Expanding));

  labels_play_ << label_filetype_title_
               << label_length_title_
               << label_samplerate_title_
               << label_bitdepth_title_
               << label_bitrate_title_;

  labels_play_data_ << label_filetype_
                    << label_length_
                    << label_samplerate_
                    << label_bitdepth_
                    << label_bitrate_;

  labels_play_all_ = labels_play_ << labels_play_data_;

  textedit_play_ << textedit_play_lyrics_;

  QObject::connect(widget_album_, &ContextAlbum::FadeStopFinished, this, &ContextView::FadeStopFinished);

  // The timer is scheduled for when the next line starts, so it needs to be precise for the line to be highlighted on time.
  timer_synced_lyrics_->setSingleShot(true);
  timer_synced_lyrics_->setTimerType(Qt::PreciseTimer);
  QObject::connect(timer_synced_lyrics_, &QTimer::timeout, this, &ContextView::UpdateSyncedLyricsPosition);

}

void ContextView::Init(CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller, SharedPtr<Player> player, SharedPtr<LyricsProviders> lyrics_providers) {

  collectionview_ = collectionview;
  album_cover_choice_controller_ = album_cover_choice_controller;
  player_ = player;

  widget_album_->Init(this, album_cover_choice_controller_);
  lyrics_fetcher_ = new LyricsFetcher(lyrics_providers, this);

  QObject::connect(collectionview_, &CollectionView::TotalSongCountUpdated_, this, &ContextView::UpdateNoSong);
  QObject::connect(collectionview_, &CollectionView::TotalArtistCountUpdated_, this, &ContextView::UpdateNoSong);
  QObject::connect(collectionview_, &CollectionView::TotalAlbumCountUpdated_, this, &ContextView::UpdateNoSong);
  QObject::connect(lyrics_fetcher_, &LyricsFetcher::LyricsFetched, this, &ContextView::UpdateLyrics);

  AddActions();

}

void ContextView::AddActions() {

  action_show_album_ = new QAction(tr("Show album cover"), this);
  action_show_album_->setCheckable(true);
  action_show_album_->setChecked(true);

  action_show_data_ = new QAction(tr("Show song technical data"), this);
  action_show_data_->setCheckable(true);
  action_show_data_->setChecked(true);

  action_show_lyrics_ = new QAction(tr("Show song lyrics"), this);
  action_show_lyrics_->setCheckable(true);
  action_show_lyrics_->setChecked(true);

  action_search_lyrics_ = new QAction(tr("Automatically search for song lyrics"), this);
  action_search_lyrics_->setCheckable(true);
  action_search_lyrics_->setChecked(true);

  menu_options_->addAction(action_show_album_);
  menu_options_->addAction(action_show_data_);
  menu_options_->addAction(action_show_lyrics_);
  menu_options_->addAction(action_search_lyrics_);
  menu_options_->addSeparator();

  ReloadSettings();

  QObject::connect(action_show_album_, &QAction::triggered, this, &ContextView::ActionShowAlbum);
  QObject::connect(action_show_data_, &QAction::triggered, this, &ContextView::ActionShowData);
  QObject::connect(action_show_lyrics_, &QAction::triggered, this, &ContextView::ActionShowLyrics);
  QObject::connect(action_search_lyrics_, &QAction::triggered, this, &ContextView::ActionSearchLyrics);

}

void ContextView::ReloadSettings() {

  QString default_font;
  if (QFontDatabase::families().contains(QLatin1String(ContextSettings::kDefaultFontFamily))) {
    default_font = QLatin1String(ContextSettings::kDefaultFontFamily);
  }
  else {
    default_font = font().family();
  }

  Settings s;
  s.beginGroup(ContextSettings::kSettingsGroup);
  title_fmt_ = s.value(ContextSettings::kSettingsTitleFmt, QLatin1String(ContextSettings::kDefaultTitleFmt)).toString();
  summary_fmt_ = s.value(ContextSettings::kSettingsSummaryFmt, QLatin1String(ContextSettings::kDefaultSummaryFmt)).toString();
  action_show_album_->setChecked(s.value(ContextSettings::kAlbum, ContextSettings::kDefaultAlbum).toBool());
  action_show_data_->setChecked(s.value(ContextSettings::kTechnicalData, ContextSettings::kDefaultTechnicalData).toBool());
  action_show_lyrics_->setChecked(s.value(ContextSettings::kSongLyrics, ContextSettings::kDefaultSongLyrics).toBool());
  action_search_lyrics_->setChecked(s.value(ContextSettings::kSearchLyrics, ContextSettings::kDefaultSearchLyrics).toBool());
  synced_lyrics_enabled_ = s.value(ContextSettings::kSyncedLyrics, ContextSettings::kDefaultSyncedLyrics).toBool();
  font_headline_.setFamily(s.value(ContextSettings::kFontHeadline, default_font).toString());
  font_headline_.setPointSizeF(s.value(ContextSettings::kFontSizeHeadline, ContextSettings::kDefaultFontSizeHeadline).toReal());
  font_nosong_.setFamily(font_headline_.family());
  font_nosong_.setPointSizeF(font_headline_.pointSizeF() * 1.6F);
  font_normal_.setFamily(s.value(ContextSettings::kFontNormal, default_font).toString());
  font_normal_.setPointSizeF(s.value(ContextSettings::kFontSizeNormal, font().pointSizeF()).toReal());
  s.endGroup();

  UpdateFonts();

  if (widget_stacked_->currentWidget() == widget_stop_) {
    NoSong();
  }
  else {
    SetSong();
    // The changed settings can leave the song without lyrics to show, for example when synchronized lyrics are disabled.
    SearchLyrics();
  }

}

void ContextView::resizeEvent(QResizeEvent *e) {

  if (e->size().width() != e->oldSize().width()) {
    widget_album_->UpdateWidth(width() - kWidgetSpacing);
  }

  QWidget::resizeEvent(e);

}

void ContextView::Playing() {

  // Reschedule right away when playback resumes, the timer only checks occasionally while paused.
  if (!synced_lyrics_lines_.isEmpty()) {
    UpdateSyncedLyricsPosition();
  }

}

void ContextView::Stopped() {

  song_playing_ = Song();
  song_prev_ = Song();
  lyrics_.clear();
  synced_lyrics_.clear();
  fetched_lyrics_.clear();
  fetched_synced_lyrics_.clear();
  fetched_lyrics_provider_.clear();
  synced_lyrics_shown_.clear();
  synced_lyrics_lines_.clear();
  synced_lyrics_line_ = -1;
  timer_synced_lyrics_->stop();
  // Ignore a pending lyrics search, so a late result does not show lyrics or restart the timer with no song playing.
  lyrics_id_ = -1;
  image_original_ = QImage();
  widget_album_->SetImage();

}

void ContextView::Error() {}

void ContextView::SongChanged(const Song &song) {

  // Changed lyrics, for example from editing the tags of the playing song, need a full update to be shown.
  if (widget_stacked_->currentWidget() == widget_play_ && song_playing_.is_valid() && song == song_playing_ && song.title() == song_playing_.title() && song.album() == song_playing_.album() && song.artist() == song_playing_.artist() && song.lyrics() == song_playing_.lyrics() && song.synced_lyrics() == song_playing_.synced_lyrics()) {
    UpdateSong(song);
  }
  else {
    song_prev_ = song_playing_;
    song_playing_ = song;
    lyrics_ = song.lyrics();
    synced_lyrics_ = song.synced_lyrics();
    fetched_lyrics_.clear();
    fetched_synced_lyrics_.clear();
    fetched_lyrics_provider_.clear();
    lyrics_id_ = -1;
    lyrics_tried_ = false;
    SetSong();
  }

  SearchLyrics();

}

void ContextView::SearchLyrics() {

  // Synchronized lyrics from the tags are only enough when they are valid LRC and shown, otherwise search for plain lyrics.
  // synced_lyrics_lines_ is only set by SetLyricsText() under those conditions.
  const bool have_lyrics = !lyrics_.isEmpty() || !synced_lyrics_lines_.isEmpty();
  if (!have_lyrics && action_show_lyrics_->isChecked() && action_search_lyrics_->isChecked() && !song_playing_.artist().isEmpty() && !song_playing_.title().isEmpty() && !lyrics_tried_ && lyrics_id_ == -1) {
    lyrics_fetcher_->Clear();
    lyrics_tried_ = true;
    lyrics_id_ = static_cast<qint64>(lyrics_fetcher_->Search(song_playing_.effective_albumartist(), song_playing_.artist(), song_playing_.album(), song_playing_.title(), song_playing_.length_nanosec() / kNsecPerSec));
  }

}

void ContextView::FadeStopFinished() {

  widget_stacked_->setCurrentWidget(widget_stop_);
  NoSong();
  ResetSong();
  widget_stacked_->updateGeometry();

}

void ContextView::SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def) {
  label->setText(value <= 0 ? def : (QString::number(value) + QLatin1Char(' ') + suffix));
}

void ContextView::UpdateNoSong() {
  if (widget_stacked_->currentWidget() == widget_stop_) NoSong();
}

void ContextView::NoSong() {

  if (!widget_album_->isVisibleTo(this)) {
    widget_album_->show();
  }

  textedit_top_->setFont(font_nosong_);
  textedit_top_->SetText(tr("No song playing"));

  QString html;
  if (collectionview_->TotalSongs() == 1) html += tr("%1 song").arg(collectionview_->TotalSongs());
  else html += tr("%1 songs").arg(collectionview_->TotalSongs());
  html += "<br />"_L1;

  if (collectionview_->TotalArtists() == 1) html += tr("%1 artist").arg(collectionview_->TotalArtists());
  else html += tr("%1 artists").arg(collectionview_->TotalArtists());
  html += "<br />"_L1;

  if (collectionview_->TotalAlbums() == 1) html += tr("%1 album").arg(collectionview_->TotalAlbums());
  else html += tr("%1 albums").arg(collectionview_->TotalAlbums());
  html += "<br />"_L1;

  label_stop_summary_->setFont(font_normal_);
  label_stop_summary_->setText(html);

}

void ContextView::UpdateFonts() {

  for (QLabel *l : std::as_const(labels_play_all_)) {
    l->setFont(font_normal_);
  }
  for (QTextEdit *e : std::as_const(textedit_play_)) {
    e->setFont(font_normal_);
  }

}

void ContextView::SetSong() {

  textedit_top_->setFont(font_headline_);
  textedit_top_->SetText(QStringLiteral("<b>%1</b><br />%2").arg(Utilities::ReplaceMessage(title_fmt_, song_playing_, u"<br />"_s, true), Utilities::ReplaceMessage(summary_fmt_, song_playing_, u"<br />"_s, true)));

  label_stop_summary_->clear();

  bool widget_album_changed = !song_prev_.is_valid();
  if (action_show_album_->isChecked() && !widget_album_->isVisibleTo(this)) {
    widget_album_->show();
    widget_album_changed = true;
  }
  else if (!action_show_album_->isChecked() && widget_album_->isVisibleTo(this)) {
    widget_album_->hide();
    widget_album_changed = true;
  }
  if (widget_album_changed) Q_EMIT AlbumEnabledChanged();

  if (action_show_data_->isChecked()) {
    widget_play_data_->show();
    label_filetype_->setText(song_playing_.TextForFiletype());
    if (song_playing_.length_nanosec() <= 0) {
      label_length_title_->hide();
      label_length_->hide();
      label_length_->clear();
    }
    else {
      label_length_title_->show();
      label_length_->show();
      label_length_->setText(Utilities::PrettyTimeNanosec(song_playing_.length_nanosec()));
    }
    if (song_playing_.samplerate() <= 0) {
      label_samplerate_title_->hide();
      label_samplerate_->hide();
      label_samplerate_->clear();
    }
    else {
      label_samplerate_title_->show();
      label_samplerate_->show();
      SetLabelText(label_samplerate_, song_playing_.samplerate(), u"Hz"_s);
    }
    if (song_playing_.bitdepth() <= 0) {
      label_bitdepth_title_->hide();
      label_bitdepth_->hide();
      label_bitdepth_->clear();
    }
    else {
      label_bitdepth_title_->show();
      label_bitdepth_->show();
      SetLabelText(label_bitdepth_, song_playing_.bitdepth(), u"Bit"_s);
    }
    if (song_playing_.bitrate() <= 0) {
      label_bitrate_title_->hide();
      label_bitrate_->hide();
      label_bitrate_->clear();
    }
    else {
      label_bitrate_title_->show();
      label_bitrate_->show();
      SetLabelText(label_bitrate_, song_playing_.bitrate(), tr("kbps"));
    }
    spacer_play_data_->changeSize(20, 20, QSizePolicy::Fixed);
  }
  else {
    widget_play_data_->hide();
    label_filetype_->clear();
    label_length_->clear();
    label_samplerate_->clear();
    label_bitdepth_->clear();
    label_bitrate_->clear();
    spacer_play_data_->changeSize(0, 0, QSizePolicy::Fixed);
  }

  SetLyricsText();

  widget_stacked_->setCurrentWidget(widget_play_);
  widget_stacked_->updateGeometry();

}

void ContextView::UpdateSong(const Song &song) {

  const QString top_text = QStringLiteral("<b>%1</b><br />%2").arg(Utilities::ReplaceMessage(title_fmt_, song, u"<br />"_s, true), Utilities::ReplaceMessage(summary_fmt_, song, u"<br />"_s, true));
  if (top_text != textedit_top_->Text()) {
    textedit_top_->SetText(top_text);
  }

  if (action_show_data_->isChecked()) {
    if (song.filetype() != song_playing_.filetype()) label_filetype_->setText(song.TextForFiletype());
    if (song.length_nanosec() != song_playing_.length_nanosec()) {
      if (song.length_nanosec() <= 0) {
        label_length_title_->hide();
        label_length_->hide();
        label_length_->clear();
      }
      else {
        label_length_title_->show();
        label_length_->show();
        label_length_->setText(Utilities::PrettyTimeNanosec(song.length_nanosec()));
      }
    }
    if (song.samplerate() != song_playing_.samplerate()) {
      if (song.samplerate() <= 0) {
        label_samplerate_title_->hide();
        label_samplerate_->hide();
        label_samplerate_->clear();
      }
      else {
        label_samplerate_title_->show();
        label_samplerate_->show();
        SetLabelText(label_samplerate_, song.samplerate(), u"Hz"_s);
      }
    }
    if (song.bitdepth() != song_playing_.bitdepth()) {
      if (song.bitdepth() <= 0) {
        label_bitdepth_title_->hide();
        label_bitdepth_->hide();
        label_bitdepth_->clear();
      }
      else {
        label_bitdepth_title_->show();
        label_bitdepth_->show();
        SetLabelText(label_bitdepth_, song.bitdepth(), u"Bit"_s);
      }
    }
    if (song.bitrate() != song_playing_.bitrate()) {
      if (song.bitrate() <= 0) {
        label_bitrate_title_->hide();
        label_bitrate_->hide();
        label_bitrate_->clear();
      }
      else {
        label_bitrate_title_->show();
        label_bitrate_->show();
        SetLabelText(label_bitrate_, song.bitrate(), tr("kbps"));
      }
    }
  }

  song_playing_ = song;

  widget_stacked_->updateGeometry();

}

void ContextView::ResetSong() {

  for (QLabel *l : std::as_const(labels_play_data_)) {
    l->clear();
  }

  for (QTextEdit *l : std::as_const(textedit_play_)) {
    l->clear();
  }

  widget_play_data_->hide();
  textedit_play_lyrics_->hide();
  timer_synced_lyrics_->stop();

}

void ContextView::UpdateLyrics(const quint64 id, const LyricsSearchResult &result) {

  if (static_cast<qint64>(id) != lyrics_id_) return;

  // Fetched lyrics are kept apart from the lyrics from the tags, so the tags are still used if the settings change.
  if (result.lyrics.isEmpty() && result.synced_lyrics.isEmpty()) {
    fetched_lyrics_ = "No lyrics found.\n"_L1;
    fetched_synced_lyrics_.clear();
    fetched_lyrics_provider_.clear();
  }
  else {
    fetched_lyrics_ = result.lyrics;
    fetched_synced_lyrics_ = result.synced_lyrics;
    fetched_lyrics_provider_ = result.provider;
  }
  lyrics_id_ = -1;

  SetLyricsText();

}

void ContextView::SetLyricsText() {

  if (!action_show_lyrics_->isChecked()) {
    synced_lyrics_lines_.clear();
    synced_lyrics_line_ = -1;
    synced_lyrics_shown_.clear();
    timer_synced_lyrics_->stop();
    textedit_play_lyrics_->clear();
    textedit_play_lyrics_->hide();
    return;
  }

  // Prefer synchronized lyrics from the tags over fetched ones, and fall back to plain lyrics if they are disabled or not valid LRC.
  Utilities::LRCLines lrc_lines;
  QString synced_lyrics;
  QString synced_lyrics_provider;
  if (synced_lyrics_enabled_) {
    const std::optional<Utilities::LRCLines> tag_lrc_lines = Utilities::ParseLRC(synced_lyrics_);
    if (tag_lrc_lines.has_value() && !tag_lrc_lines->isEmpty()) {
      lrc_lines = *tag_lrc_lines;
      synced_lyrics = synced_lyrics_;
      // Lyrics from the tags of a CUE track are for the whole media file, while the position is relative to the start of the track.
      synced_lyrics_offset_msec_ = song_playing_.beginning_nanosec() / kNsecPerMsec;
    }
    else {
      const std::optional<Utilities::LRCLines> fetched_lrc_lines = Utilities::ParseLRC(fetched_synced_lyrics_);
      if (fetched_lrc_lines.has_value() && !fetched_lrc_lines->isEmpty()) {
        lrc_lines = *fetched_lrc_lines;
        synced_lyrics = fetched_synced_lyrics_;
        synced_lyrics_provider = fetched_lyrics_provider_;
        // Fetched lyrics are for the track itself.
        synced_lyrics_offset_msec_ = 0;
      }
    }
  }

  if (!lrc_lines.isEmpty()) {
    // Only replace the text if the lyrics changed, so a selection in the lyrics is kept.
    const QString synced_lyrics_shown = synced_lyrics + u'\n' + synced_lyrics_provider;
    if (synced_lyrics_shown != synced_lyrics_shown_ || textedit_play_lyrics_->isHidden()) {
      synced_lyrics_shown_ = synced_lyrics_shown;
      synced_lyrics_lines_ = lrc_lines;
      synced_lyrics_line_ = -1;
      SetSyncedLyricsText(synced_lyrics_provider);
      textedit_play_lyrics_->show();
    }
    UpdateSyncedLyricsPosition();
    return;
  }

  synced_lyrics_lines_.clear();
  synced_lyrics_line_ = -1;
  synced_lyrics_shown_.clear();
  timer_synced_lyrics_->stop();

  QString text;
  if (!lyrics_.isEmpty()) {
    text = lyrics_;
  }
  else if (!fetched_lyrics_.isEmpty()) {
    text = fetched_lyrics_;
    if (!fetched_lyrics_provider_.isEmpty()) {
      text += "\n\n(Lyrics from "_L1 + fetched_lyrics_provider_ + ")\n"_L1;
    }
  }

  if (text.isEmpty()) {
    textedit_play_lyrics_->clear();
    textedit_play_lyrics_->hide();
  }
  else {
    textedit_play_lyrics_->SetText(text);
    textedit_play_lyrics_->show();
  }

}

void ContextView::SetSyncedLyricsText(const QString &provider) {

  // One block per line, so the block number is the line number when marking the active line.
  textedit_play_lyrics_->clear();
  QTextCursor cursor(textedit_play_lyrics_->document());
  const QTextCharFormat char_format;
  for (qsizetype i = 0; i < synced_lyrics_lines_.size(); ++i) {
    if (i > 0) cursor.insertBlock();
    cursor.insertText(synced_lyrics_lines_.at(i).text, char_format);
  }

  if (!provider.isEmpty()) {
    cursor.insertBlock();
    cursor.insertBlock();
    cursor.insertText("(Lyrics from "_L1 + provider + u')', char_format);
  }

  textedit_play_lyrics_->updateGeometry();

}

void ContextView::SetSyncedLyricsLineBold(const qsizetype line, const bool bold) {

  if (line < 0 || line >= synced_lyrics_lines_.size()) return;

  // The active line is the last of the lines with the same timestamp, for example from a multiline SYLT entry, so mark all of them.
  qsizetype first_line = line;
  while (first_line > 0 && synced_lyrics_lines_.at(first_line - 1).time == synced_lyrics_lines_.at(line).time) {
    --first_line;
  }

  const QTextBlock first_text_block = textedit_play_lyrics_->document()->findBlockByNumber(static_cast<int>(first_line));
  const QTextBlock last_text_block = textedit_play_lyrics_->document()->findBlockByNumber(static_cast<int>(line));
  if (!first_text_block.isValid() || !last_text_block.isValid()) return;

  // Use a separate cursor, the formatting is changed without touching the text or the user's selection.
  QTextCursor cursor(first_text_block);
  cursor.setPosition(last_text_block.position() + last_text_block.length() - 1, QTextCursor::KeepAnchor);
  QTextCharFormat char_format;
  char_format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
  cursor.mergeCharFormat(char_format);

  // Bold text is wider and can wrap differently.
  textedit_play_lyrics_->updateGeometry();

}

qint64 ContextView::SyncedLyricsPositionMsec() const {

  return (player_ ? player_->engine()->position_nanosec() / kNsecPerMsec : 0) + synced_lyrics_offset_msec_;

}

qsizetype ContextView::SyncedLyricsLine(const qint64 position_msec) const {

  // The active line is the last line which started at or before the current position, none before the first line.
  const Utilities::LRCLines::const_iterator it = std::upper_bound(synced_lyrics_lines_.cbegin(), synced_lyrics_lines_.cend(), position_msec, [](const qint64 position, const Utilities::LRCLine &lrc_line) { return position < static_cast<qint64>(lrc_line.time); });

  return std::distance(synced_lyrics_lines_.cbegin(), it) - 1;

}

void ContextView::UpdateSyncedLyricsPosition() {

  if (synced_lyrics_lines_.isEmpty()) return;

  const qint64 position_msec = SyncedLyricsPositionMsec();
  const qsizetype line = SyncedLyricsLine(position_msec);

  // Only change the formatting of the previous and new active line.
  if (line != synced_lyrics_line_) {
    SetSyncedLyricsLineBold(synced_lyrics_line_, false);
    SetSyncedLyricsLineBold(line, true);
    synced_lyrics_line_ = line;
  }

  // Wake up exactly when the next line starts instead of polling, but check at least regularly to follow seeking and pausing.
  // Only while playing, the position does not advance when paused, so the timer would otherwise keep firing right away.
  qint64 next_update_msec = kSyncedLyricsMaxUpdateTimeMs;
  if (player_ && player_->GetState() == EngineBase::State::Playing && line + 1 < synced_lyrics_lines_.size()) {
    next_update_msec = std::clamp(static_cast<qint64>(synced_lyrics_lines_.at(line + 1).time) - position_msec, 1LL, kSyncedLyricsMaxUpdateTimeMs);
  }
  timer_synced_lyrics_->start(static_cast<int>(next_update_msec));

}

void ContextView::contextMenuEvent(QContextMenuEvent *e) {

  if (menu_options_ && widget_stacked_->currentWidget() == widget_stop_) {
    menu_options_->popup(mapToGlobal(e->pos()));
  }
  else {
    QWidget::contextMenuEvent(e);
  }

}

void ContextView::dragEnterEvent(QDragEnterEvent *e) {

  if (song_playing_.is_valid() && AlbumCoverChoiceController::CanAcceptDrag(e)) {
    e->acceptProposedAction();
  }

  QWidget::dragEnterEvent(e);

}

void ContextView::dropEvent(QDropEvent *e) {

  if (song_playing_.is_valid()) {
    album_cover_choice_controller_->SaveCover(&song_playing_, e);
  }

  QWidget::dropEvent(e);

}

void ContextView::AlbumCoverLoaded(const Song &song, const QImage &image) {

  if (song != song_playing_ || image == image_original_) return;

  widget_album_->SetImage(image);
  image_original_ = image;

}

void ContextView::ActionShowAlbum() {

  Settings s;
  s.beginGroup(ContextSettings::kSettingsGroup);
  s.setValue(ContextSettings::kAlbum, action_show_album_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

}

void ContextView::ActionShowData() {

  Settings s;
  s.beginGroup(ContextSettings::kSettingsGroup);
  s.setValue(ContextSettings::kTechnicalData, action_show_data_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

}

void ContextView::ActionShowLyrics() {

  Settings s;
  s.beginGroup(ContextSettings::kSettingsGroup);
  s.setValue(ContextSettings::kSongLyrics, action_show_lyrics_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

  SearchLyrics();

}

void ContextView::ActionSearchLyrics() {

  Settings s;
  s.beginGroup(ContextSettings::kSettingsGroup);
  s.setValue(ContextSettings::kSearchLyrics, action_search_lyrics_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

  SearchLyrics();

}
