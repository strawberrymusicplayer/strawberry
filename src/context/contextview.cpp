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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QIODevice>
#include <QFile>
#include <QByteArray>
#include <QList>
#include <QVariant>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QMovie>
#include <QAction>
#include <QPainter>
#include <QPalette>
#include <QBrush>
#include <QMenu>
#include <QSettings>
#include <QSizePolicy>
#include <QTimeLine>
#include <QtEvents>
#include <QFontDatabase>

#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"
#include "core/song.h"
#include "core/utilities.h"
#include "core/iconloader.h"
#include "engine/engine_fwd.h"
#include "engine/enginebase.h"
#include "engine/enginetype.h"
#include "engine/enginedevice.h"
#include "engine/devicefinder.h"
#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "collection/collectionmodel.h"
#include "collection/collectionview.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentartloader.h"
#include "lyrics/lyricsfetcher.h"

#include "contextview.h"
#include "contextalbumsmodel.h"
#include "ui_contextviewcontainer.h"

using std::unique_ptr;

const char *ContextView::kSettingsGroup = "ContextView";

ContextView::ContextView(QWidget *parent) :
    QWidget(parent),
    ui_(new Ui_ContextViewContainer),
    app_(nullptr),
    collectionview_(nullptr),
    album_cover_choice_controller_(nullptr),
    lyrics_fetcher_(nullptr),
    menu_(new QMenu(this)),
    timeline_fade_(new QTimeLine(1000, this)),
    image_strawberry_(":/pictures/strawberry.png"),
    active_(false),
    downloading_covers_(false),
    lyrics_id_(-1)
  {

  ui_->setupUi(this);
  ui_->widget_stacked->setCurrentWidget(ui_->widget_stop);
  ui_->label_play_album->installEventFilter(this);

  QFontDatabase::addApplicationFont(":/fonts/HumongousofEternitySt.ttf");

  connect(timeline_fade_, SIGNAL(valueChanged(qreal)), SLOT(FadePreviousTrack(qreal)));
  timeline_fade_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

  cover_loader_options_.desired_height_ = 300;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;
  pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_strawberry_));

}

ContextView::~ContextView() { delete ui_; }

void ContextView::Init(Application *app, CollectionView *collectionview, AlbumCoverChoiceController *album_cover_choice_controller) {

  app_ = app;
  collectionview_ = collectionview;
  album_cover_choice_controller_ = album_cover_choice_controller;

  ui_->widget_play_albums->SetApplication(app_);
  lyrics_fetcher_ = new LyricsFetcher(app_->lyrics_providers(), this);

  connect(collectionview_, SIGNAL(TotalSongCountUpdated_()), this, SLOT(UpdateNoSong()));
  connect(collectionview_, SIGNAL(TotalArtistCountUpdated_()), this, SLOT(UpdateNoSong()));
  connect(collectionview_, SIGNAL(TotalAlbumCountUpdated_()), this, SLOT(UpdateNoSong()));
  connect(lyrics_fetcher_, SIGNAL(LyricsFetched(const quint64, const QString&, const QString&)), this, SLOT(UpdateLyrics(const quint64, const QString&, const QString&)));
  connect(app_->current_art_loader(), SIGNAL(ArtLoaded(Song, QString, QImage)), SLOT(AlbumArtLoaded(Song, QString, QImage)));
  connect(album_cover_choice_controller_, SIGNAL(AutomaticCoverSearchDone()), this, SLOT(AutomaticCoverSearchDone()));
  connect(album_cover_choice_controller_->search_cover_auto_action(), SIGNAL(triggered()), this, SLOT(SearchCoverAutomatically()));

  AddActions();

}

void ContextView::AddActions() {

  action_show_data_ = new QAction(tr("Show song technical data"), this);
  action_show_data_->setCheckable(true);
  action_show_data_->setChecked(true);

  action_show_output_ = new QAction(tr("Show engine and device"), this);
  action_show_output_->setCheckable(true);
  action_show_output_->setChecked(true);

  action_show_albums_ = new QAction(tr("Show albums by artist"), this);
  action_show_albums_->setCheckable(true);
  action_show_albums_->setChecked(true);

  action_show_lyrics_ = new QAction(tr("Show song lyrics"), this);
  action_show_lyrics_->setCheckable(true);
  action_show_lyrics_->setChecked(false);

  menu_->addAction(action_show_data_);
  menu_->addAction(action_show_output_);
  menu_->addAction(action_show_albums_);
  menu_->addAction(action_show_lyrics_);
  menu_->addSeparator();

  QList<QAction*> cover_actions = album_cover_choice_controller_->GetAllActions();
  cover_actions.append(album_cover_choice_controller_->search_cover_auto_action());
  menu_->addActions(cover_actions);
  menu_->addSeparator();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  action_show_data_->setChecked(s.value("show_data", true).toBool());
  action_show_output_->setChecked(s.value("show_output", true).toBool());
  action_show_albums_->setChecked(s.value("show_albums", true).toBool());
  action_show_lyrics_->setChecked(s.value("show_lyrics", false).toBool());
  s.endGroup();

  connect(action_show_data_, SIGNAL(triggered()), this, SLOT(ActionShowData()));
  connect(action_show_output_, SIGNAL(triggered()), this, SLOT(ActionShowOutput()));
  connect(action_show_albums_, SIGNAL(triggered()), this, SLOT(ActionShowAlbums()));
  connect(action_show_lyrics_, SIGNAL(triggered()), this, SLOT(ActionShowLyrics()));

}

void ContextView::Playing() {}

void ContextView::Stopped() {

  active_ = false;
  song_playing_ = Song();
  song_ = Song();
  downloading_covers_ = false;
  song_prev_ = Song();
  lyrics_ = QString();
  SetImage(image_strawberry_);

}

void ContextView::Error() {}

void ContextView::UpdateNoSong() {
  NoSong();
}

void ContextView::SongChanged(const Song &song) {

  if (song_playing_.is_valid() && song.id() == song_playing_.id() && song.url() == song_playing_.url()) {
    UpdateSong(song);
  }
  else {
    song_prev_ = song_playing_;
    lyrics_ = song.lyrics();
    lyrics_id_ = -1;
    song_playing_ = song;
    song_ = song;
    SetSong(song);
    if (lyrics_.isEmpty() && action_show_lyrics_->isChecked()) {
      lyrics_fetcher_->Clear();
      lyrics_id_ = lyrics_fetcher_->Search(song.effective_albumartist(), song.album(), song.title());
    }
  }

}

void ContextView::SetLabelEnabled(QLabel *label) {
  label->setEnabled(true);
  label->setVisible(true);
  label->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

void ContextView::SetLabelText(QLabel *label, int value, const QString &suffix, const QString &def) {
  label->setText(value <= 0 ? def : (QString::number(value) + " " + suffix));
}

void ContextView::SetLabelDisabled(QLabel *label) {
  label->setEnabled(false);
  label->setVisible(false);
  label->setMaximumSize(0, 0);
}

void ContextView::NoSong() {

  ui_->label_stop_top->setStyleSheet(
                                     "font: 20pt \"Humongous of Eternity St\";"
                                     "font-weight: Regular;"
                                     );

  ui_->label_stop_top->setText(tr("No song playing"));

  QString html = tr(
                    "%1 songs<br />\n"
                    "%2 artists<br />\n"
                    "%3 albums<br />\n"
                    )
                    .arg(collectionview_->TotalSongs())
                    .arg(collectionview_->TotalArtists())
                    .arg(collectionview_->TotalAlbums())
                    ;

  ui_->label_stop_summary->setStyleSheet(
                                         "font: 12pt;"
                                         "font-weight: regular;"
                                        );
  ui_->label_stop_summary->setText(html);

}

void ContextView::SetSong(const Song &song) {

  QList <QLabel *> labels_play_data;

  labels_play_data << ui_->label_filetype
                   << ui_->filetype
                   << ui_->label_length
                   << ui_->length
                   << ui_->label_samplerate
                   << ui_->samplerate
                   << ui_->label_bitdepth
                   << ui_->bitdepth
                   << ui_->label_bitrate
                   << ui_->bitrate;

  ui_->label_play_top->setStyleSheet(
                                     "font: 11pt;"
                                     "font-weight: regular;"
                                     );
  ui_->label_play_top->setText( QString("<b>%1 - %2</b><br/>%3").arg(song.PrettyTitle().toHtmlEscaped(), song.artist().toHtmlEscaped(), song.album().toHtmlEscaped()));

  if (action_show_data_->isChecked()) {
    ui_->layout_play_data->setEnabled(true);
    SetLabelEnabled(ui_->label_filetype);
    SetLabelEnabled(ui_->filetype);
    SetLabelEnabled(ui_->label_length);
    SetLabelEnabled(ui_->length);
    ui_->filetype->setText(song.TextForFiletype());
    ui_->length->setText(Utilities::PrettyTimeNanosec(song.length_nanosec()));
    if (song.samplerate() <= 0) {
      SetLabelDisabled(ui_->label_samplerate);
      SetLabelDisabled(ui_->samplerate);
      ui_->samplerate->clear();
    }
    else {
      SetLabelEnabled(ui_->label_samplerate);
      SetLabelEnabled(ui_->samplerate);
      SetLabelText(ui_->samplerate, song.samplerate(), "Hz");
    }
    if (song.bitdepth() <= 0) {
      SetLabelDisabled(ui_->label_bitdepth);
      SetLabelDisabled(ui_->bitdepth);
      ui_->bitdepth->clear();
    }
    else {
      SetLabelEnabled(ui_->label_bitdepth);
      SetLabelEnabled(ui_->bitdepth);
      SetLabelText(ui_->bitdepth, song.bitdepth(), "Bit");
    }
    if (song.bitrate() <= 0) {
      SetLabelDisabled(ui_->label_bitrate);
      SetLabelDisabled(ui_->bitrate);
      ui_->bitrate->clear();
    }
    else {
      SetLabelEnabled(ui_->label_bitrate);
      SetLabelEnabled(ui_->bitrate);
      SetLabelText(ui_->bitrate, song.bitrate(), tr("kbps"));
    }
    ui_->spacer_play_data->changeSize(20, 20, QSizePolicy::Fixed);
  }
  else {
    ui_->filetype->clear();
    ui_->length->clear();
    ui_->samplerate->clear();
    ui_->bitdepth->clear();
    ui_->bitrate->clear();
    for (QLabel *l : labels_play_data) {
      SetLabelDisabled(l);
    }
    ui_->layout_play_data->setEnabled(false);
    ui_->spacer_play_data->changeSize(0, 0, QSizePolicy::Fixed);
  }

  if (action_show_output_->isChecked()) {
    Engine::EngineType enginetype(Engine::None);
    if (app_->player()->engine()) enginetype = app_->player()->engine()->type();
    QIcon icon_engine = IconLoader::Load(EngineName(enginetype), 32);

    ui_->label_engine->setVisible(true);
    ui_->label_engine->setMaximumSize(60, QWIDGETSIZE_MAX);
    ui_->label_engine_icon->setVisible(true);
    ui_->label_engine_icon->setPixmap(icon_engine.pixmap(icon_engine.availableSizes().last()));
    ui_->label_engine_icon->setMinimumSize(32, 32);
    ui_->label_engine_icon->setMaximumSize(32, 32);
    ui_->engine->setVisible(true);
    ui_->engine->setText(EngineDescription(enginetype));
    ui_->engine->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    ui_->spacer_play_output->changeSize(20, 20, QSizePolicy::Fixed);

    DeviceFinder::Device device;
    for (DeviceFinder *f : app_->enginedevice()->device_finders_) {
      for (const DeviceFinder::Device &d : f->ListDevices()) {
        if (d.value != app_->player()->engine()->device()) continue;
        device = d;
        break;
      }
    }
    if (device.value.isValid()) {
      ui_->label_device->setVisible(true);
      ui_->label_device->setMaximumSize(60, QWIDGETSIZE_MAX);
      ui_->label_device_icon->setVisible(true);
      ui_->label_device_icon->setMinimumSize(32, 32);
      ui_->label_device_icon->setMaximumSize(32, 32);
      ui_->device->setVisible(true);
      ui_->device->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
      QIcon icon_device = IconLoader::Load(device.iconname, 32);
      ui_->label_device_icon->setPixmap(icon_device.pixmap(icon_device.availableSizes().last()));
      ui_->device->setText(device.description);
    }
    else {
      ui_->label_device->setVisible(false);
      ui_->label_device->setMaximumSize(0, 0);
      ui_->label_device_icon->setVisible(false);
      ui_->label_device_icon->setMinimumSize(0, 0);
      ui_->label_device_icon->setMaximumSize(0, 0);
      ui_->label_device_icon->clear();
      ui_->device->clear();
      ui_->device->setVisible(false);
      ui_->device->setMaximumSize(0, 0);
    }
  }
  else {
    ui_->label_engine->setVisible(false);
    ui_->label_engine->setMaximumSize(0, 0);
    ui_->label_engine_icon->clear();
    ui_->label_engine_icon->setVisible(false);
    ui_->label_engine_icon->setMinimumSize(0, 0);
    ui_->label_engine_icon->setMaximumSize(0, 0);
    ui_->engine->clear();
    ui_->engine->setVisible(false);
    ui_->engine->setMaximumSize(0, 0);
    ui_->spacer_play_output->changeSize(0, 0, QSizePolicy::Fixed);
    ui_->label_device->setVisible(false);
    ui_->label_device->setMaximumSize(0, 0);
    ui_->label_device_icon->setVisible(false);
    ui_->label_device_icon->setMinimumSize(0, 0);
    ui_->label_device_icon->setMaximumSize(0, 0);
    ui_->label_device_icon->clear();
    ui_->device->clear();
    ui_->device->setVisible(false);
    ui_->device->setMaximumSize(0, 0);
  }

  if (action_show_albums_->isChecked() && song_prev_.artist() != song.artist()) {
    const QueryOptions opt;
    CollectionBackend::AlbumList albumlist;
    ui_->widget_play_albums->albums_model()->Reset();
    albumlist = app_->collection_backend()->GetAlbumsByArtist(song.artist(), opt);
    if (albumlist.count() > 1) {
      ui_->label_play_albums->setVisible(true);
      ui_->label_play_albums->setMinimumSize(0, 20);
      ui_->label_play_albums->setText(tr("<b>Albums by %1</b>").arg( song.artist().toHtmlEscaped()));
      ui_->label_play_albums->setStyleSheet("background-color: #3DADE8; color: rgb(255, 255, 255); font: 11pt;");
      for (CollectionBackend::Album album : albumlist) {
        SongList songs = app_->collection_backend()->GetSongs(song.artist(), album.album_name, opt);
        ui_->widget_play_albums->albums_model()->AddSongs(songs);
      }
      ui_->widget_play_albums->setEnabled(true);
      ui_->widget_play_albums->setVisible(true);
      ui_->spacer_play_albums1->changeSize(20, 10, QSizePolicy::Fixed);
      ui_->spacer_play_albums2->changeSize(20, 20, QSizePolicy::Fixed);
    }
    else {
      ui_->label_play_albums->clear();
      ui_->label_play_albums->setVisible(false);
      ui_->label_play_albums->setMinimumSize(0, 0);
      ui_->widget_play_albums->setEnabled(false);
      ui_->widget_play_albums->setVisible(false);
      ui_->spacer_play_albums1->changeSize(0, 0, QSizePolicy::Fixed);
      ui_->spacer_play_albums2->changeSize(0, 0, QSizePolicy::Fixed);
    }
  }
  else if (!action_show_albums_->isChecked()) {
    ui_->label_play_albums->clear();
    ui_->label_play_albums->setVisible(false);
    ui_->label_play_albums->setMinimumSize(0, 0);
    ui_->widget_play_albums->albums_model()->Reset();
    ui_->widget_play_albums->setEnabled(false);
    ui_->widget_play_albums->setVisible(false);
    ui_->spacer_play_albums1->changeSize(0, 0, QSizePolicy::Fixed);
    ui_->spacer_play_albums2->changeSize(0, 0, QSizePolicy::Fixed);
  }

  if (action_show_lyrics_->isChecked()) {
    ui_->label_play_lyrics->setText(lyrics_);
  }
  else {
    ui_->label_play_lyrics->clear();
  }

  ui_->widget_stacked->setCurrentWidget(ui_->widget_play);

}

void ContextView::UpdateSong(const Song &song) {

  if (song.artist() != song_playing_.artist() || song.album() != song_playing_.album() || song.title() != song_playing_.title()) {
    ui_->label_play_top->setText( QString("<b>%1 - %2</b><br/>%3").arg(song.PrettyTitle().toHtmlEscaped(), song.artist().toHtmlEscaped(), song.album().toHtmlEscaped()));
  }

  if (action_show_data_->isChecked()) {
    if (song.filetype() != song_playing_.filetype()) ui_->filetype->setText(song.TextForFiletype());
    if (song.length_nanosec() != song_playing_.length_nanosec()) ui_->label_length->setText(Utilities::PrettyTimeNanosec(song.length_nanosec()));
    if (song.samplerate() != song_playing_.samplerate()) {
      if (song.samplerate() <= 0) {
        SetLabelDisabled(ui_->label_samplerate);
        SetLabelDisabled(ui_->samplerate);
        ui_->samplerate->clear();
      }
      else {
        SetLabelEnabled(ui_->label_samplerate);
        SetLabelEnabled(ui_->samplerate);
        SetLabelText(ui_->samplerate, song.samplerate(), "Hz");
      }
    }
    if (song.bitdepth() != song_playing_.bitdepth()) {
      if (song.bitdepth() <= 0) {
        SetLabelDisabled(ui_->label_bitdepth);
        SetLabelDisabled(ui_->bitdepth);
        ui_->bitdepth->clear();
      }
      else {
        SetLabelEnabled(ui_->label_bitdepth);
        SetLabelEnabled(ui_->bitdepth);
        SetLabelText(ui_->bitdepth, song.bitdepth(), "Bit");
      }
    }
    if (song.bitrate() != song_playing_.bitrate()) {
      if (song.bitrate() <= 0) {
        SetLabelDisabled(ui_->label_bitrate);
        SetLabelDisabled(ui_->bitrate);
        ui_->bitrate->clear();
      }
      else {
        SetLabelEnabled(ui_->label_bitrate);
        SetLabelEnabled(ui_->bitrate);
        SetLabelText(ui_->bitrate, song.bitrate(), tr("kbps"));
      }
    }
  }
  song_playing_ = song;
  song_ = song;

}

void ContextView::UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics) {

  if (id != lyrics_id_) return;
  lyrics_ = lyrics + "\n\n(Lyrics from " + provider + ")\n";
  lyrics_id_ = -1;
  if (action_show_lyrics_->isChecked()) {
    ui_->label_play_lyrics->setText(lyrics_);
  }
  else ui_->label_play_lyrics->clear();

}

bool ContextView::eventFilter(QObject *object, QEvent *event) {

  switch(event->type()) {
    case QEvent::Paint:{
      handlePaintEvent(object, event);
    }
    default:{
      return QObject::eventFilter(object, event);
    }
  }

  return(true);

}

void ContextView::handlePaintEvent(QObject *object, QEvent *event) {

  if (object == ui_->label_play_album) {
    PaintEventAlbum(event);
  }

  return;

}

void ContextView::PaintEventAlbum(QEvent *event) {

  QPainter p(ui_->label_play_album);

  DrawImage(&p);

  // Draw the previous track's image if we're fading
  if (!pixmap_previous_.isNull()) {
    p.setOpacity(pixmap_previous_opacity_);
    p.drawPixmap(0, 0, pixmap_previous_);
  }
}

void ContextView::DrawImage(QPainter *p) {

  p->drawPixmap(0, 0, 300, 300, pixmap_current_);
  if ((downloading_covers_) && (spinner_animation_)) {
    p->drawPixmap(50, 50, 16, 16, spinner_animation_->currentPixmap());
  }

}

void ContextView::FadePreviousTrack(qreal value) {

  pixmap_previous_opacity_ = value;
  if (qFuzzyCompare(pixmap_previous_opacity_, qreal(0.0))) {
    image_previous_ = QImage();
    pixmap_previous_ = QPixmap();
  }
  update();

  if (value == 0 && !active_) {
    ui_->widget_stacked->setCurrentWidget(ui_->widget_stop);
    NoSong();
  }

}

void ContextView::contextMenuEvent(QContextMenuEvent *e) {
  if (menu_ && ui_->widget_stacked->currentWidget() == ui_->widget_play) menu_->popup(mapToGlobal(e->pos()));
}

void ContextView::mouseReleaseEvent(QMouseEvent *) {
}

void ContextView::dragEnterEvent(QDragEnterEvent *e) {
  QWidget::dragEnterEvent(e);
}

void ContextView::dropEvent(QDropEvent *e) {
  QWidget::dropEvent(e);
}

void ContextView::ScaleCover() {

  pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_original_));
  update();

}

void ContextView::AlbumArtLoaded(const Song &song, const QString&, const QImage &image) {

  if (song.id() != song_playing_.id() || song.url() != song_playing_.url()) return;
  if (song.effective_albumartist() != song_playing_.effective_albumartist() || song.effective_album() != song_playing_.effective_album() || song.title() != song_playing_.title()) return;
  if (image == image_original_) return;

  active_ = true;
  downloading_covers_ = false;
  song_ = song;
  SetImage(image);
  GetCoverAutomatically();

}

void ContextView::SetImage(const QImage &image) {

  // Cache the current pixmap so we can fade between them
  pixmap_previous_ = QPixmap(size());
  pixmap_previous_.fill(palette().background().color());
  pixmap_previous_opacity_ = 1.0;

  QPainter p(&pixmap_previous_);
  DrawImage(&p);
  p.end();

  image_previous_ = image_original_;
  image_original_ = image;

  ScaleCover();

  // Were we waiting for this cover to load before we started fading?
  if (!pixmap_previous_.isNull() && timeline_fade_) {
    timeline_fade_->stop();
    timeline_fade_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0
    timeline_fade_->start();
  }

}

void ContextView::GetCoverAutomatically() {

  // Search for cover automatically?
  bool search =
               album_cover_choice_controller_->search_cover_auto_action()->isChecked() &&
               !song_.has_manually_unset_cover() &&
               song_.art_automatic().isEmpty() &&
               song_.art_manual().isEmpty() &&
               !song_.effective_albumartist().isEmpty() &&
               !song_.effective_album().isEmpty();

  if (search) {
    downloading_covers_ = true;
    // This is done in mainwindow instead to avoid searching multiple times (ContextView & PlayingWidget)
    //album_cover_choice_controller_->SearchCoverAutomatically(song_);

    // Show a spinner animation
    spinner_animation_.reset(new QMovie(":/pictures/spinner.gif", QByteArray(), this));
    connect(spinner_animation_.get(), SIGNAL(updated(const QRect&)), SLOT(update()));
    spinner_animation_->start();
    update();
  }

}

void ContextView::AutomaticCoverSearchDone() {

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}

void ContextView::ActionShowData() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_data", action_show_data_->isChecked());
  s.endGroup();
  SetSong(song_);
}

void ContextView::ActionShowOutput() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_output", action_show_output_->isChecked());
  s.endGroup();
  SetSong(song_);
}

void ContextView::ActionShowAlbums() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_albums", action_show_albums_->isChecked());
  s.endGroup();
  song_prev_ = Song();
  SetSong(song_);
}

void ContextView::ActionShowLyrics() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_lyrics", action_show_lyrics_->isChecked());
  s.endGroup();
  SetSong(song_);
  if (lyrics_.isEmpty() && action_show_lyrics_->isChecked()) {
    lyrics_fetcher_->Clear();
    lyrics_id_ = lyrics_fetcher_->Search(song_.artist(), song_.album(), song_.title());
  }
}

void ContextView::SearchCoverAutomatically() {
  GetCoverAutomatically();
}
