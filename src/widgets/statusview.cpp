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

#include <unistd.h>

#include <QApplication>
#include <QFile>
#include <QTextDocument>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPaintEvent>
#include <QMovie>
#include <QTimeLine>

#include "statusview.h"

#include "core/utilities.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/application.h"
#include "core/player.h"
#include "core/mainwindow.h"

#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectionview.h"
#include "collection/collectionviewcontainer.h"

#include "covermanager/albumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/currentartloader.h"
#include "covermanager/albumcoverchoicecontroller.h"

#include "engine/enginetype.h"
#include "engine/enginebase.h"

const char *StatusView::kSettingsGroup = "StatusView";

StatusView::StatusView(CollectionViewContainer *collectionviewcontainer, QWidget *parent) :

    QWidget(parent),
    layout_(new QVBoxLayout),
    scrollarea_(new QScrollArea),
    container_layout_(new QVBoxLayout),
    container_widget_(new QWidget),

    widget_stopped_ (nullptr),
    widget_playing_ (nullptr),
    layout_playing_(nullptr),
    layout_stopped_(nullptr),
    label_stopped_top_ (nullptr),
    label_stopped_logo_(nullptr),
    label_stopped_text_(nullptr),
    label_playing_top_(nullptr),
    label_playing_album_(nullptr),
    label_playing_text_(nullptr),

    album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
    show_hide_animation_(new QTimeLine(500, this)),
    fade_animation_(new QTimeLine(1000, this)),
    image_blank_(""),
    image_nosong_(":/icons/full/strawberry.png"),
    widgetstate_(None),
    menu_(new QMenu(this))
 {
     
  //qLog(Debug) << __PRETTY_FUNCTION__;

  collectionview_ = collectionviewcontainer->view();
  connect(collectionview_, SIGNAL(TotalSongCountUpdated_()), this, SLOT(UpdateNoSong()));
  connect(collectionview_, SIGNAL(TotalArtistCountUpdated_()), this, SLOT(UpdateNoSong()));
  connect(collectionview_, SIGNAL(TotalAlbumCountUpdated_()), this, SLOT(UpdateNoSong()));

  connect(fade_animation_, SIGNAL(valueChanged(qreal)), SLOT(FadePreviousTrack(qreal)));
  fade_animation_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

  cover_loader_options_.desired_height_ = 300;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;
  pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_blank_));

  CreateWidget();
  NoSongWidget();
  NoSong();
  AddActions();

}

StatusView::~StatusView() {
}

void StatusView::AddActions() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  QList<QAction*> actions = album_cover_choice_controller_->GetAllActions();

  // Here we add the search automatically action, too!
  actions.append(album_cover_choice_controller_->search_cover_auto_action());

  connect(album_cover_choice_controller_->cover_from_file_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromFile()));
  connect(album_cover_choice_controller_->cover_to_file_action(), SIGNAL(triggered()), this, SLOT(SaveCoverToFile()));
  connect(album_cover_choice_controller_->cover_from_url_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromURL()));
  connect(album_cover_choice_controller_->search_for_cover_action(), SIGNAL(triggered()), this, SLOT(SearchForCover()));
  connect(album_cover_choice_controller_->unset_cover_action(), SIGNAL(triggered()), this, SLOT(UnsetCover()));
  connect(album_cover_choice_controller_->show_cover_action(), SIGNAL(triggered()), this, SLOT(ShowCover()));
  connect(album_cover_choice_controller_->search_cover_auto_action(), SIGNAL(triggered()), this, SLOT(SearchCoverAutomatically()));

  menu_->addActions(actions);
  menu_->addSeparator();
    
}

void StatusView::CreateWidget() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  setLayout(layout_);
  setStyleSheet("background-color: white;");

  layout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(6);
  layout_->addWidget(scrollarea_);

  scrollarea_->setWidget(container_widget_);
  scrollarea_->setWidgetResizable(true);
  scrollarea_->setStyleSheet("background-color: white;");
  scrollarea_->setVisible(true);

  container_widget_->setLayout(container_layout_);
  container_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  container_widget_->setBackgroundRole(QPalette::Base);

  container_layout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
  container_layout_->setContentsMargins(0, 0, 0, 0);
  container_layout_->setSpacing(6);
  container_layout_->addStretch();
    
}

void StatusView::SetApplication(Application *app) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  app_ = app;

  album_cover_choice_controller_->SetApplication(app_);
  connect(app_->current_art_loader(), SIGNAL(ArtLoaded(Song, QString, QImage)), SLOT(AlbumArtLoaded(Song, QString, QImage)));

}

void StatusView::NoSongWidget() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (widgetstate_ == Playing) {
    container_layout_->removeWidget(widget_playing_);
    widget_playing_->setVisible(false);
    delete label_playing_top_;
    delete label_playing_album_;
    delete label_playing_text_;
    delete layout_playing_;
    delete widget_playing_;
  }
  widget_stopped_ = new QWidget;
  layout_stopped_ = new QVBoxLayout;
  label_stopped_top_ = new QLabel;
  label_stopped_logo_ = new QLabel;
  label_stopped_text_ = new QLabel;

  layout_stopped_->addWidget(label_stopped_top_);
  layout_stopped_->addWidget(label_stopped_logo_);
  layout_stopped_->addWidget(label_stopped_text_);
  layout_stopped_->addStretch();
  
  label_stopped_top_->setFixedHeight(40);
  label_stopped_top_->setFixedWidth(300);
  label_stopped_top_->setAlignment(Qt::AlignTop & Qt::AlignCenter);

  widget_stopped_->setVisible(true);
  widget_stopped_->setLayout(layout_stopped_);
  container_layout_->insertWidget(0, widget_stopped_);
  widgetstate_ = Stopped;

}

void StatusView::SongWidget() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (widgetstate_ == Stopped) {
    container_layout_->removeWidget(widget_stopped_);
    widget_stopped_->setVisible(false);
    delete label_stopped_top_ ;
    delete label_stopped_logo_;
    delete label_stopped_text_;
    delete layout_stopped_;
    delete widget_stopped_;
  }
  widget_playing_ = new QWidget;
  widget_playing_ = new QWidget;
  layout_playing_ = new QVBoxLayout;
  label_playing_top_ = new QLabel;
  label_playing_album_ = new QLabel;
  label_playing_text_ = new QLabel;

  layout_playing_->addWidget(label_playing_top_);
  layout_playing_->addWidget(label_playing_album_);
  layout_playing_->addWidget(label_playing_text_);
  layout_playing_->addStretch();

  label_playing_top_->setAlignment(Qt::AlignTop & Qt::AlignCenter);
  label_playing_top_->setFixedHeight(40);  
  label_playing_top_->setFixedWidth(300);
  label_playing_top_->setWordWrap(true);

  label_playing_text_->setAlignment(Qt::AlignTop);
  label_playing_text_->setFixedWidth(300);
  label_playing_text_->setWordWrap(true);
  
  label_playing_album_->setFixedHeight(300);
  label_playing_album_->setFixedWidth(300);
  label_playing_album_->setStyleSheet("background-color: transparent;");
  label_playing_album_->installEventFilter(this);
  
  widget_playing_->setVisible(true);
  widget_playing_->setLayout(layout_playing_);
  container_layout_->insertWidget(0, widget_playing_);
  
  QFile stylesheet(":/style/statusview.css");
  if (stylesheet.open(QIODevice::ReadOnly)) {
    setStyleSheet(QString::fromLatin1(stylesheet.readAll()));
    label_playing_text_->setStyleSheet(QString::fromLatin1(stylesheet.readAll()));
  }
  
  widgetstate_ = Playing;

}

void StatusView::SwitchWidgets(WidgetState state) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  if (widgetstate_ == None) NoSongWidget();

  if ((state == Stopped) && (widgetstate_ != Stopped)) {
    NoSongWidget();
  }
  if ((widgetstate_ != Playing) && (state == Playing)) {
    SongWidget();

  }
    
}

void StatusView::UpdateSong(const Song &song) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  SwitchWidgets(Playing);

  const Song *song_ = &song;
  const QueryOptions opt;
  CollectionBackend::AlbumList albumlist;
  Engine::EngineType enginetype = app_->player()->engine()->type();
  QString EngineName = EngineNameFromType(enginetype);
  
  label_playing_top_->setText("");
  label_playing_text_->setText("");

  QString html;
  QString html_albums;
  html += QString("<b>%1 - %2</b><br/>%3").arg(song_->PrettyTitle().toHtmlEscaped(), song_->artist().toHtmlEscaped(), song_->album().toHtmlEscaped());
  label_playing_top_->setText(html);

  html = "";
  
  html += QString("Filetype: %1<br />\n").arg(song_->TextForFiletype());
  html += QString("Length: %1<br />\n").arg(Utilities::PrettyTimeNanosec(song.length_nanosec()));
  html += QString("Bitrate: %1 kbps<br />\n").arg(song_->bitrate());
  html += QString("Samplerate: %1 hz / %2 bit<br />\n").arg(song_->samplerate()).arg(song_->bitdepth());

  if (enginetype != Engine::EngineType::None) {
    html += QString("<br />");
    html += QString("Engine: %1<br />").arg(EngineName);
  }

  html += QString("<br />");
    
  html_albums += QString("<b>Albums by %1:</b>").arg( song_->artist().toHtmlEscaped() );

  albumlist = app_->collection_backend()->GetAlbumsByArtist(song_->artist(), opt);

  html_albums +=      QString("<ul>");
  int i=0;
  for (CollectionBackend::Album album : albumlist) {
    i++;
    html_albums +=      QString("<li>%1</li>\n").arg(album.album_name.toHtmlEscaped());
   }
   
   html_albums +=      	QString("</ul>");
   html_albums +=	QString("");
   
   if (i > 1) html += html_albums;
   
   label_playing_text_->setText(html);
    
}

void StatusView::NoSong() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  QString html;
  QImage image_logo(":/icons/full/strawberry.png");
  QImage image_logo_scaled = image_logo.scaled(300, 300, Qt::KeepAspectRatio);
  QPixmap pixmap_logo(QPixmap::fromImage(image_logo_scaled));

  SwitchWidgets(Stopped);

  label_stopped_top_->setText("<b>No Track Playing</b>");
  label_stopped_logo_->setPixmap(pixmap_logo);

  html += QString(
		    "<html>\n"
		    "<head>\n"
		    "<style type=\"text/css\">:/style/statusview.css</style>\n"
		    "</head>\n"
		    "<body>\n"
		    "%1 songs<br />\n"
		    "%2 artists<br />\n"
		    "%3 albums<br />\n"
		    "</body>\n"
		    "</html>\n"
		    )
		    .arg(collectionview_->TotalSongs())
		    .arg(collectionview_->TotalArtists())
		    .arg(collectionview_->TotalAlbums())
		    ;
		    
  label_stopped_text_->setText(html);

}

void StatusView::SongChanged(const Song &song) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  stopped_ = false;
  metadata_ = song;
  const Song *song_ = &song;

  UpdateSong(*song_);

  update();

}

void StatusView::SongFinished() {
  
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  stopped_ = true;
  SetImage(image_blank_);
  
}

bool StatusView::eventFilter(QObject *object, QEvent *event) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
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

void StatusView::handlePaintEvent(QObject *object, QEvent *event) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__ << object->objectName();

  if (object == label_playing_album_) {
      paintEvent_album(event);
  }
  
  return;

}

void StatusView::paintEvent_album(QEvent *event) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  QPainter p(label_playing_album_);

  DrawImage(&p);

  // Draw the previous track's image if we're fading
  if (!pixmap_previous_.isNull()) {
    p.setOpacity(pixmap_previous_opacity_);
    p.drawPixmap(0, 0, pixmap_previous_);
  }
}

void StatusView::DrawImage(QPainter *p) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  p->drawPixmap(0, 0, 300, 300, pixmap_current_);
  if ((downloading_covers_) && (spinner_animation_ != nullptr)) {
      p->drawPixmap(50, 50, 16, 16, spinner_animation_->currentPixmap());
  }
  
}

void StatusView::FadePreviousTrack(qreal value) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  pixmap_previous_opacity_ = value;
  if (qFuzzyCompare(pixmap_previous_opacity_, qreal(0.0))) {
    pixmap_previous_ = QPixmap();
  }

  update();
  
  if ((value == 0) && (stopped_ == true)) {
    SwitchWidgets(Stopped);
    NoSong();
  }
  
}

void StatusView::contextMenuEvent(QContextMenuEvent *e) {

  // show the menu
  menu_->popup(mapToGlobal(e->pos()));
}

void StatusView::mouseReleaseEvent(QMouseEvent *) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
}

void StatusView::dragEnterEvent(QDragEnterEvent *e) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  QWidget::dragEnterEvent(e);
  
}

void StatusView::dropEvent(QDropEvent *e) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  QWidget::dropEvent(e);
  
}

void StatusView::ScaleCover() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, original_));
  update();

}

void StatusView::AlbumArtLoaded(const Song &metadata, const QString&, const QImage &image) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  SwitchWidgets(Playing);
  
  label_playing_album_->clear();

  metadata_ = metadata;
  downloading_covers_ = false;

  SetImage(image);

  // Search for cover automatically?
  GetCoverAutomatically();

}

void StatusView::SetImage(const QImage &image) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  // Cache the current pixmap so we can fade between them
  pixmap_previous_ = QPixmap(size());
  pixmap_previous_.fill(palette().background().color());
  pixmap_previous_opacity_ = 1.0;

  QPainter p(&pixmap_previous_);
  DrawImage(&p);
  p.end();

  original_ = image;

  ScaleCover();

  // Were we waiting for this cover to load before we started fading?
  if (!pixmap_previous_.isNull() && fade_animation_ != nullptr) {
    fade_animation_->start();
  }

}

bool StatusView::GetCoverAutomatically() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  SwitchWidgets(Playing);
  
  // Search for cover automatically?
  bool search =
		!metadata_.has_manually_unset_cover() &&
		metadata_.art_automatic().isEmpty() &&
		metadata_.art_manual().isEmpty() &&
		!metadata_.artist().isEmpty() &&
		!metadata_.album().isEmpty();

  if (search) {
    qLog(Debug) << "GetCoverAutomatically";
    downloading_covers_ = true;
    album_cover_choice_controller_->SearchCoverAutomatically(metadata_);

    // Show a spinner animation
    spinner_animation_.reset(new QMovie(":/pictures/spinner.gif", QByteArray(), this));
    connect(spinner_animation_.get(), SIGNAL(updated(const QRect&)), SLOT(update()));
    spinner_animation_->start();
    update();
  }

  return search;

}

void StatusView::AutomaticCoverSearchDone() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}

void StatusView::UpdateNoSong() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (widgetstate_ == Playing) return;

  NoSong();

}

void StatusView::LoadCoverFromFile() {
  album_cover_choice_controller_->LoadCoverFromFile(&metadata_);
}

void StatusView::LoadCoverFromURL() {
  album_cover_choice_controller_->LoadCoverFromURL(&metadata_);
}

void StatusView::SearchForCover() {
  album_cover_choice_controller_->SearchForCover(&metadata_);
}

void StatusView::SaveCoverToFile() {
  album_cover_choice_controller_->SaveCoverToFile(metadata_, original_);
}

void StatusView::UnsetCover() {
  album_cover_choice_controller_->UnsetCover(&metadata_);
}

void StatusView::ShowCover() {
  album_cover_choice_controller_->ShowCover(metadata_);
}

void StatusView::SearchCoverAutomatically() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("search_for_cover_auto", album_cover_choice_controller_->search_cover_auto_action()->isChecked());

  // Search for cover automatically?
  GetCoverAutomatically();

}
