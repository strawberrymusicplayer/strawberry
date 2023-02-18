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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QList>
#include <QVariant>
#include <QString>
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

#include "core/application.h"
#include "core/player.h"
#include "core/song.h"
#include "core/iconloader.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "widgets/resizabletextedit.h"
#include "engine/engine_fwd.h"
#include "engine/enginebase.h"
#include "engine/enginetype.h"
#include "engine/devicefinders.h"
#include "engine/devicefinder.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "collection/collectionview.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "lyrics/lyricsfetcher.h"
#include "settings/contextsettingspage.h"

#include "contextview.h"
#include "contextalbum.h"

const int ContextView::kWidgetSpacing = 50;

ContextView::ContextView(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      collectionview_(nullptr),
      album_cover_choice_controller_(nullptr),
      lyrics_fetcher_(nullptr),
      menu_options_(new QMenu(this)),
      action_show_album_(nullptr),
      action_show_data_(nullptr),
      action_show_output_(nullptr),
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
      widget_play_output_(new QWidget(this)),
      layout_play_data_(new QGridLayout()),
      layout_play_output_(new QGridLayout()),
      textedit_play_lyrics_(new ResizableTextEdit(this)),
      spacer_play_output_(new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed)),
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
      label_device_title_(new QLabel(this)),
      label_engine_title_(new QLabel(this)),
      label_device_space_(new QLabel(this)),
      label_engine_space_(new QLabel(this)),
      label_device_(new QLabel(this)),
      label_engine_(new QLabel(this)),
      label_device_icon_(new QLabel(this)),
      label_engine_icon_(new QLabel(this)),
      lyrics_tried_(false),
      lyrics_id_(-1),
      font_size_headline_(0),
      font_size_normal_(0) {

  setLayout(layout_container_);

  layout_container_->setObjectName("context-layout-container");
  layout_container_->setContentsMargins(0, 0, 0, 0);
  layout_container_->addWidget(scrollarea_);

  scrollarea_->setObjectName("context-scrollarea");
  scrollarea_->setWidgetResizable(true);
  scrollarea_->setWidget(widget_scrollarea_);
  scrollarea_->setContentsMargins(0, 0, 0, 0);
  scrollarea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollarea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  widget_scrollarea_->setObjectName("context-widget-scrollarea");
  widget_scrollarea_->setLayout(layout_scrollarea_);
  widget_scrollarea_->setContentsMargins(0, 0, 0, 0);

  textedit_top_->setReadOnly(true);
  textedit_top_->setFrameShape(QFrame::NoFrame);

  layout_scrollarea_->setObjectName("context-layout-scrollarea");
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

  label_engine_title_->setText(tr("Engine"));
  label_device_title_->setText(tr("Device"));
  label_engine_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_device_title_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_engine_space_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_device_space_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_engine_space_->setMinimumWidth(24);
  label_device_space_->setMinimumWidth(24);
  label_engine_icon_->setMinimumSize(32, 32);
  label_device_icon_->setMaximumSize(32, 32);
  label_engine_icon_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_device_icon_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  label_engine_->setWordWrap(true);
  label_device_->setWordWrap(true);

  layout_play_output_->setContentsMargins(0, 0, 0, 0);

  layout_play_output_->addWidget(label_engine_title_, 0, 0);
  layout_play_output_->addWidget(label_engine_space_, 0, 1);
  layout_play_output_->addWidget(label_engine_icon_, 0, 2);
  layout_play_output_->addWidget(label_engine_, 0, 3);

  layout_play_output_->addWidget(label_device_title_, 1, 0);
  layout_play_output_->addWidget(label_device_space_, 1, 1);
  layout_play_output_->addWidget(label_device_icon_, 1, 2);
  layout_play_output_->addWidget(label_device_, 1, 3);

  widget_play_output_->setLayout(layout_play_output_);

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
  textedit_play_lyrics_->hide();

  layout_play_->setContentsMargins(0, 0, 0, 0);
  layout_play_->addWidget(widget_play_output_);
  layout_play_->addSpacerItem(spacer_play_output_);
  layout_play_->addWidget(widget_play_data_);
  layout_play_->addSpacerItem(spacer_play_data_);
  layout_play_->addWidget(textedit_play_lyrics_);
  layout_play_->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Expanding));

  labels_play_ << label_engine_title_
               << label_device_title_
               << label_filetype_title_
               << label_length_title_
               << label_samplerate_title_
               << label_bitdepth_title_
               << label_bitrate_title_;

  labels_play_data_ << label_engine_icon_
                    << label_engine_
                    << label_device_
                    << label_device_icon_
                    << label_filetype_
                    << label_length_
                    << label_samplerate_
                    << label_bitdepth_
                    << label_bitrate_;

  labels_play_all_ = labels_play_ << labels_play_data_;

  textedit_play_ << textedit_play_lyrics_;

  QObject::connect(widget_album_, &ContextAlbum::FadeStopFinished, this, &ContextView::FadeStopFinished);

}

void ContextView::Init(Application *app, CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller) {

  app_ = app;
  collectionview_ = collectionview;
  album_cover_choice_controller_ = album_cover_choice_controller;

  widget_album_->Init(this, album_cover_choice_controller_);
  lyrics_fetcher_ = new LyricsFetcher(app_->lyrics_providers(), this);

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

  action_show_output_ = new QAction(tr("Show engine and device"), this);
  action_show_output_->setCheckable(true);
  action_show_output_->setChecked(true);

  action_show_lyrics_ = new QAction(tr("Show song lyrics"), this);
  action_show_lyrics_->setCheckable(true);
  action_show_lyrics_->setChecked(true);

  action_search_lyrics_ = new QAction(tr("Automatically search for song lyrics"), this);
  action_search_lyrics_->setCheckable(true);
  action_search_lyrics_->setChecked(true);

  menu_options_->addAction(action_show_album_);
  menu_options_->addAction(action_show_data_);
  menu_options_->addAction(action_show_output_);
  menu_options_->addAction(action_show_lyrics_);
  menu_options_->addAction(action_search_lyrics_);
  menu_options_->addSeparator();

  ReloadSettings();

  QObject::connect(action_show_album_, &QAction::triggered, this, &ContextView::ActionShowAlbum);
  QObject::connect(action_show_data_, &QAction::triggered, this, &ContextView::ActionShowData);
  QObject::connect(action_show_output_, &QAction::triggered, this, &ContextView::ActionShowOutput);
  QObject::connect(action_show_lyrics_, &QAction::triggered, this, &ContextView::ActionShowLyrics);
  QObject::connect(action_search_lyrics_, &QAction::triggered, this, &ContextView::ActionSearchLyrics);

}

void ContextView::ReloadSettings() {

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  title_fmt_ = s.value(ContextSettingsPage::kSettingsTitleFmt, "%title% - %artist%").toString();
  summary_fmt_ = s.value(ContextSettingsPage::kSettingsSummaryFmt, "%album%").toString();
  action_show_album_->setChecked(s.value(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::ALBUM)], true).toBool());
  action_show_data_->setChecked(s.value(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::TECHNICAL_DATA)], false).toBool());
  action_show_output_->setChecked(s.value(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::ENGINE_AND_DEVICE)], false).toBool());
  action_show_lyrics_->setChecked(s.value(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::SONG_LYRICS)], true).toBool());
  action_search_lyrics_->setChecked(s.value(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::SEARCH_LYRICS)], true).toBool());
  font_headline_ = s.value("font_headline", font().family()).toString();
  font_normal_ = s.value("font_normal", font().family()).toString();
  font_size_headline_ = s.value("font_size_headline", ContextSettingsPage::kDefaultFontSizeHeadline).toReal();
  font_size_normal_ = s.value("font_size_normal", font().pointSizeF()).toReal();
  s.endGroup();

  UpdateFonts();

  if (widget_stacked_->currentWidget() == widget_stop_) {
    NoSong();
  }
  else {
    SetSong();
  }

}

void ContextView::resizeEvent(QResizeEvent *e) {

  if (e->size().width() != e->oldSize().width()) {
    widget_album_->UpdateWidth(width() - kWidgetSpacing);
  }

  QWidget::resizeEvent(e);

}

void ContextView::Playing() {}

void ContextView::Stopped() {

  song_playing_ = Song();
  song_prev_ = Song();
  lyrics_.clear();
  image_original_ = QImage();
  widget_album_->SetImage();

}

void ContextView::Error() {}

void ContextView::SongChanged(const Song &song) {

  if (widget_stacked_->currentWidget() == widget_play_ && song_playing_.is_valid() && song == song_playing_ && song.title() == song_playing_.title() && song.album() == song_playing_.album() && song.artist() == song_playing_.artist()) {
    UpdateSong(song);
  }
  else {
    song_prev_ = song_playing_;
    song_playing_ = song;
    lyrics_ = song.lyrics();
    lyrics_id_ = -1;
    lyrics_tried_ = false;
    SetSong();
  }

  SearchLyrics();

}

void ContextView::SearchLyrics() {

  if (lyrics_.isEmpty() && action_show_lyrics_->isChecked() && action_search_lyrics_->isChecked() && !song_playing_.artist().isEmpty() && !song_playing_.title().isEmpty() && !lyrics_tried_ && lyrics_id_ == -1) {
    lyrics_fetcher_->Clear();
    lyrics_tried_ = true;
    lyrics_id_ = static_cast<qint64>(lyrics_fetcher_->Search(song_playing_.effective_albumartist(), song_playing_.album(), song_playing_.title()));
  }

}

void ContextView::FadeStopFinished() {

  widget_stacked_->setCurrentWidget(widget_stop_);
  NoSong();
  ResetSong();
  widget_stacked_->updateGeometry();

}

void ContextView::SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def) {
  label->setText(value <= 0 ? def : (QString::number(value) + " " + suffix));
}

void ContextView::UpdateNoSong() {
  if (widget_stacked_->currentWidget() == widget_stop_) NoSong();
}

void ContextView::NoSong() {

  if (!widget_album_->isVisible()) {
    widget_album_->show();
  }

  textedit_top_->setFont(QFont(font_headline_, font_size_headline_ * 1.6));
  textedit_top_->SetText(tr("No song playing"));

  QString html;
  if (collectionview_->TotalSongs() == 1) html += tr("%1 song").arg(collectionview_->TotalSongs());
  else html += tr("%1 songs").arg(collectionview_->TotalSongs());
  html += "<br />";

  if (collectionview_->TotalArtists() == 1) html += tr("%1 artist").arg(collectionview_->TotalArtists());
  else html += tr("%1 artists").arg(collectionview_->TotalArtists());
  html += "<br />";

  if (collectionview_->TotalAlbums() == 1) html += tr("%1 album").arg(collectionview_->TotalAlbums());
  else html += tr("%1 albums").arg(collectionview_->TotalAlbums());
  html += "<br />";

  label_stop_summary_->setFont(QFont(font_normal_, font_size_normal_));
  label_stop_summary_->setText(html);

}

void ContextView::UpdateFonts() {

  QFont font(font_normal_, font_size_normal_);
  font.setBold(false);
  for (QLabel *l : labels_play_all_) {
    l->setFont(font);
  }
  for (QTextEdit *e : textedit_play_) {
    e->setFont(font);
  }

}

void ContextView::SetSong() {

  textedit_top_->setFont(QFont(font_headline_, font_size_headline_));
  textedit_top_->SetText(QString("<b>%1</b><br />%2").arg(Utilities::ReplaceMessage(title_fmt_, song_playing_, "<br />", true), Utilities::ReplaceMessage(summary_fmt_, song_playing_, "<br />", true)));

  label_stop_summary_->clear();

  bool widget_album_changed = !song_prev_.is_valid();
  if (action_show_album_->isChecked() && !widget_album_->isVisible()) {
    widget_album_->show();
    widget_album_changed = true;
  }
  else if (!action_show_album_->isChecked() && widget_album_->isVisible()) {
    widget_album_->hide();
    widget_album_changed = true;
  }
  if (widget_album_changed) emit AlbumEnabledChanged();

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
      SetLabelText(label_samplerate_, song_playing_.samplerate(), "Hz");
    }
    if (song_playing_.bitdepth() <= 0) {
      label_bitdepth_title_->hide();
      label_bitdepth_->hide();
      label_bitdepth_->clear();
    }
    else {
      label_bitdepth_title_->show();
      label_bitdepth_->show();
      SetLabelText(label_bitdepth_, song_playing_.bitdepth(), "Bit");
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

  if (action_show_output_->isChecked()) {
    widget_play_output_->show();
    Engine::EngineType enginetype(Engine::EngineType::None);
    if (app_->player()->engine()) enginetype = app_->player()->engine()->type();
    QIcon icon_engine = IconLoader::Load(EngineName(enginetype), true, 32);

    label_engine_icon_->setPixmap(icon_engine.pixmap(QSize(32, 32)));
    label_engine_->setText(EngineDescription(enginetype));
    spacer_play_output_->changeSize(20, 20, QSizePolicy::Fixed);

    DeviceFinder::Device device;
    for (DeviceFinder *f : app_->device_finders()->ListFinders()) {
      for (const DeviceFinder::Device &d : f->ListDevices()) {
        if (d.value != app_->player()->engine()->device()) continue;
        device = d;
        break;
      }
    }
    if (device.value.isValid()) {
      label_device_title_->show();
      label_device_icon_->show();
      label_device_->show();
      QIcon icon_device = IconLoader::Load(device.iconname, true, 32);
      label_device_icon_->setPixmap(icon_device.pixmap(QSize(32, 32)));
      label_device_->setText(device.description);
    }
    else {
      label_device_title_->hide();
      label_device_icon_->hide();
      label_device_->hide();
      label_device_icon_->clear();
      label_device_->clear();
    }
  }
  else {
    widget_play_output_->hide();
    label_engine_icon_->clear();
    label_engine_->clear();
    label_device_icon_->clear();
    label_device_->clear();
    spacer_play_output_->changeSize(0, 0, QSizePolicy::Fixed);
  }

  if (action_show_lyrics_->isChecked() && !lyrics_.isEmpty()) {
    textedit_play_lyrics_->SetText(lyrics_);
    textedit_play_lyrics_->show();
  }
  else {
    textedit_play_lyrics_->clear();
    textedit_play_lyrics_->hide();
  }

  widget_stacked_->setCurrentWidget(widget_play_);
  widget_stacked_->updateGeometry();

}

void ContextView::UpdateSong(const Song &song) {

  textedit_top_->SetText(QString("<b>%1</b><br />%2").arg(Utilities::ReplaceMessage(title_fmt_, song, "<br />", true), Utilities::ReplaceMessage(summary_fmt_, song, "<br />", true)));

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
        SetLabelText(label_samplerate_, song.samplerate(), "Hz");
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
        SetLabelText(label_bitdepth_, song.bitdepth(), "Bit");
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

  for (QLabel *l : labels_play_data_) {
    l->clear();
  }

  for (QTextEdit *l : textedit_play_) {
    l->clear();
  }

  widget_play_output_->hide();
  widget_play_data_->hide();
  textedit_play_lyrics_->hide();

}

void ContextView::UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics) {

  if (static_cast<qint64>(id) != lyrics_id_) return;

  lyrics_ = lyrics + "\n\n(Lyrics from " + provider + ")\n";
  lyrics_id_ = -1;

  if (action_show_lyrics_->isChecked() && !lyrics_.isEmpty()) {
    textedit_play_lyrics_->SetText(lyrics_);
    textedit_play_lyrics_->show();
  }
  else {
    textedit_play_lyrics_->clear();
    textedit_play_lyrics_->hide();
  }

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

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  s.setValue(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::ALBUM)], action_show_album_->isChecked());
  s.endGroup();
  if (song_playing_.is_valid()) SetSong();

}

void ContextView::ActionShowData() {

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  s.setValue(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::TECHNICAL_DATA)], action_show_data_->isChecked());
  s.endGroup();
  if (song_playing_.is_valid()) SetSong();

}

void ContextView::ActionShowOutput() {

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  s.setValue(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::ENGINE_AND_DEVICE)], action_show_output_->isChecked());
  s.endGroup();
  if (song_playing_.is_valid()) SetSong();

}

void ContextView::ActionShowLyrics() {

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  s.setValue(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::SONG_LYRICS)], action_show_lyrics_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

  SearchLyrics();

}

void ContextView::ActionSearchLyrics() {

  QSettings s;
  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  s.setValue(ContextSettingsPage::kSettingsGroupEnable[static_cast<int>(ContextSettingsPage::ContextSettingsOrder::SEARCH_LYRICS)], action_search_lyrics_->isChecked());
  s.endGroup();

  if (song_playing_.is_valid()) SetSong();

  SearchLyrics();

}
