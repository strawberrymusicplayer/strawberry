/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QWidget>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QMenu>
#include <QMovie>
#include <QPainter>
#include <QPalette>
#include <QBrush>
#include <QSignalMapper>
#include <QTextDocument>
#include <QTimeLine>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QtEvents>

#include "core/application.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentartloader.h"
#include "playingwidget.h"

const char *PlayingWidget::kSettingsGroup = "PlayingWidget";

// Space between the cover and the details in small mode
const int PlayingWidget::kPadding = 2;

// Width of the transparent to black gradient above and below the text in large mode
const int PlayingWidget::kGradientHead = 40;
const int PlayingWidget::kGradientTail = 20;

// Maximum height of the cover in large mode, and offset between the
// bottom of the cover and bottom of the widget
const int PlayingWidget::kMaxCoverSize = 260;
const int PlayingWidget::kBottomOffset = 0;

// Border for large mode
const int PlayingWidget::kTopBorder = 4;


PlayingWidget::PlayingWidget(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
      mode_(SmallSongDetails),
      menu_(new QMenu(this)),
      fit_cover_width_action_(nullptr),
      enabled_(false),
      visible_(false),
      active_(false),
      small_ideal_height_(0),
      fit_width_(false),
      show_hide_animation_(new QTimeLine(500, this)),
      fade_animation_(new QTimeLine(1000, this)),
      details_(new QTextDocument(this)),
      previous_track_opacity_(0.0),
      downloading_covers_(false) {

  // Load settings
  QSettings s;
  s.beginGroup(kSettingsGroup);
  mode_ = Mode(s.value("mode", LargeSongDetails).toInt());
  album_cover_choice_controller_->search_cover_auto_action()->setChecked(s.value("search_for_cover_auto", true).toBool());
  fit_width_ = s.value("fit_cover_width", false).toBool();

  // Accept drops for setting album art
  setAcceptDrops(true);

  // Context menu
  QActionGroup *mode_group = new QActionGroup(this);
  QSignalMapper *mode_mapper = new QSignalMapper(this);
  connect(mode_mapper, SIGNAL(mapped(int)), SLOT(SetMode(int)));
  CreateModeAction(SmallSongDetails, tr("Small album cover"), mode_group, mode_mapper);
  CreateModeAction(LargeSongDetails, tr("Large album cover"), mode_group, mode_mapper);

  menu_->addActions(mode_group->actions());

  fit_cover_width_action_ = menu_->addAction(tr("Fit cover to width"));

  fit_cover_width_action_->setCheckable(true);
  fit_cover_width_action_->setEnabled(true);
  connect(fit_cover_width_action_, SIGNAL(toggled(bool)), SLOT(FitCoverWidth(bool)));
  fit_cover_width_action_->setChecked(fit_width_);
  menu_->addSeparator();

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

  // Animations
  connect(show_hide_animation_, SIGNAL(frameChanged(int)), SLOT(SetHeight(int)));
  setMaximumHeight(0);

  connect(fade_animation_, SIGNAL(valueChanged(qreal)), SLOT(FadePreviousTrack(qreal)));
  fade_animation_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

  // add placeholder text to get the correct height
  if (mode_ == LargeSongDetails) {
  details_->setDefaultStyleSheet(
        "p {"
        "  font-size: small;"
        "  color: black;"
        "}");
    details_->setHtml(QString("<p align=center><i></i><br/><br/></p>"));
  }

  UpdateHeight();

  connect(album_cover_choice_controller_, SIGNAL(AutomaticCoverSearchDone()), this, SLOT(AutomaticCoverSearchDone()));
  
}

PlayingWidget::~PlayingWidget() {
}

void PlayingWidget::SetApplication(Application *app) {

  app_ = app;

  album_cover_choice_controller_->SetApplication(app_);
  connect(app_->current_art_loader(), SIGNAL(ArtLoaded(Song, QString, QImage)), SLOT(AlbumArtLoaded(Song, QString, QImage)));

}

void PlayingWidget::CreateModeAction(Mode mode, const QString &text, QActionGroup *group, QSignalMapper* mapper) {

  QAction* action = new QAction(text, group);
  action->setCheckable(true);
  mapper->setMapping(action, mode);
  connect(action, SIGNAL(triggered()), mapper, SLOT(map()));

  if (mode == mode_) action->setChecked(true);

}

void PlayingWidget::set_ideal_height(int height) {

  small_ideal_height_ = height;
  UpdateHeight();

}

QSize PlayingWidget::sizeHint() const {
    
  return QSize(cover_loader_options_.desired_height_, total_height_);

}

void PlayingWidget::UpdateHeight() {

  switch (mode_) {
    case SmallSongDetails:
      cover_loader_options_.desired_height_ = small_ideal_height_;
      total_height_ = small_ideal_height_;
      break;
    case LargeSongDetails:
      if (fit_width_) cover_loader_options_.desired_height_ = width();
      else cover_loader_options_.desired_height_ = qMin(kMaxCoverSize, width());
      total_height_ = kTopBorder + cover_loader_options_.desired_height_ + kBottomOffset + details_->size().height();
      break;
  }

  // Update the animation settings and resize the widget now if we're visible
  show_hide_animation_->setFrameRange(0, total_height_);
  if (visible_ && show_hide_animation_->state() != QTimeLine::Running) setMaximumHeight(total_height_);

  // Re-scale the current image
  if (metadata_.is_valid()) {
    ScaleCover();
  }

  // Tell Qt we've changed size
  updateGeometry();
  
}

void PlayingWidget::Stopped() {

  active_ = false;
  SetVisible(false);

}

void PlayingWidget::UpdateDetailsText() {
  
  QString html;

  switch (mode_) {
    case SmallSongDetails:
      details_->setTextWidth(-1);
      details_->setDefaultStyleSheet("");
      html += "<p>";
      break;
    case LargeSongDetails:
      details_->setTextWidth(cover_loader_options_.desired_height_);
      if (fit_width_) {
        details_->setDefaultStyleSheet(
            "p {"
            "  font-size: small;"
            "}");
      }
      else {
        details_->setDefaultStyleSheet(
            "p {"
            "  font-size: small;"
            "  color: black;"
            "}");
      }
      html += "<p align=center>";
      break;
  }

  // TODO: Make this configurable
  html += QString("<i>%1</i><br/>%2<br/>%3").arg(metadata_.PrettyTitle().toHtmlEscaped(), metadata_.artist().toHtmlEscaped(), metadata_.album().toHtmlEscaped());

  html += "</p>";
  details_->setHtml(html);

  // if something spans multiple lines the height needs to change
  if (mode_ == LargeSongDetails) UpdateHeight();

}

void PlayingWidget::ScaleCover() {

  cover_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, original_));
  update();

}

void PlayingWidget::AlbumArtLoaded(const Song &metadata, const QString &, const QImage &image) {
  
  active_ = true;

  metadata_ = metadata;
  downloading_covers_ = false;

  SetImage(image);

  // Search for cover automatically?
  GetCoverAutomatically();

}

void PlayingWidget::SetImage(const QImage &image) {

  active_ = true;

  if (visible_) {
    // Cache the current pixmap so we can fade between them
    previous_track_ = QPixmap(size());
    previous_track_.fill(palette().background().color());
    previous_track_opacity_ = 1.0;
    QPainter p(&previous_track_);
    DrawContents(&p);
    p.end();
  }

  original_ = image;

  UpdateDetailsText();
  ScaleCover();

  if (enabled_ == true) SetVisible(true);

  // Were we waiting for this cover to load before we started fading?
  if (!previous_track_.isNull()) {
    fade_animation_->start();
  }
}

void PlayingWidget::SetHeight(int height) {

  setMaximumHeight(height);
  
}

void PlayingWidget::SetVisible(bool visible) {

  if (visible == visible_) return;
  visible_ = visible;

  show_hide_animation_->setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
  show_hide_animation_->start();

}

void PlayingWidget::paintEvent(QPaintEvent *e) {
  
  QPainter p(this);

  DrawContents(&p);

  // Draw the previous track's image if we're fading
  if (!previous_track_.isNull()) {
    p.setOpacity(previous_track_opacity_);
    p.drawPixmap(0, 0, previous_track_);
  }
}

void PlayingWidget::DrawContents(QPainter *p) {

  switch (mode_) {
    case SmallSongDetails:
      // Draw the cover
      p->drawPixmap(0, 0, small_ideal_height_, small_ideal_height_, cover_);
      if (downloading_covers_) {
        p->drawPixmap(small_ideal_height_ - 18, 6, 16, 16, spinner_animation_->currentPixmap());
      }

      // Draw the details
      p->translate(small_ideal_height_ + kPadding, 0);
      details_->drawContents(p);
      p->translate(-small_ideal_height_ - kPadding, 0);
      break;

    case LargeSongDetails:
      // Work out how high the text is going to be
      const int text_height = details_->size().height();
      const int cover_size = fit_width_ ? width() : qMin(kMaxCoverSize, width());
      const int x_offset = (width() - cover_loader_options_.desired_height_) / 2;

      if (!fit_width_) {
        // Draw the black background
        //p->fillRect(QRect(0, kTopBorder, width(), height() - kTopBorder), Qt::black);
      }

      // Draw the cover
      p->drawPixmap(x_offset, kTopBorder, cover_size, cover_size, cover_);
      if (downloading_covers_) {
        p->drawPixmap(x_offset + 45, 35, 16, 16, spinner_animation_->currentPixmap());
      }

      // Draw the text below
      p->translate(x_offset, height() - text_height);
      details_->drawContents(p);
      p->translate(-x_offset, -height() + text_height);
      break;
  }
  
}

void PlayingWidget::FadePreviousTrack(qreal value) {
  
  previous_track_opacity_ = value;
  if (qFuzzyCompare(previous_track_opacity_, qreal(0.0))) {
    previous_track_ = QPixmap();
  }

  update();
  
}

void PlayingWidget::SetMode(int mode) {
  
  mode_ = Mode(mode);

  if (mode_ == SmallSongDetails) {
    fit_cover_width_action_->setEnabled(false);
  }
  else {
    fit_cover_width_action_->setEnabled(true);
  }

  UpdateHeight();
  UpdateDetailsText();
  update();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("mode", mode_);
  
}

void PlayingWidget::resizeEvent(QResizeEvent* e) {
  
  if (visible_ && e->oldSize() != e->size()) {
    if (mode_ == LargeSongDetails) {
      UpdateHeight();
      UpdateDetailsText();
    }
  }
  
}

void PlayingWidget::contextMenuEvent(QContextMenuEvent* e) {

  // show the menu
  menu_->popup(mapToGlobal(e->pos()));
}

void PlayingWidget::mouseReleaseEvent(QMouseEvent*) {
  // Same behaviour as right-click > Show Fullsize

}

void PlayingWidget::FitCoverWidth(bool fit) {
  
  fit_width_ = fit;
  UpdateHeight();
  update();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("fit_cover_width", fit_width_);
}

void PlayingWidget::LoadCoverFromFile() {
  album_cover_choice_controller_->LoadCoverFromFile(&metadata_);
}

void PlayingWidget::LoadCoverFromURL() {
  album_cover_choice_controller_->LoadCoverFromURL(&metadata_);
}

void PlayingWidget::SearchForCover() {
  album_cover_choice_controller_->SearchForCover(&metadata_);
}

void PlayingWidget::SaveCoverToFile() {
  album_cover_choice_controller_->SaveCoverToFile(metadata_, original_);
}

void PlayingWidget::UnsetCover() {
  album_cover_choice_controller_->UnsetCover(&metadata_);
}

void PlayingWidget::ShowCover() {
  album_cover_choice_controller_->ShowCover(metadata_);
}

void PlayingWidget::SearchCoverAutomatically() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("search_for_cover_auto", album_cover_choice_controller_->search_cover_auto_action()->isChecked());

  GetCoverAutomatically();

}

void PlayingWidget::dragEnterEvent(QDragEnterEvent *e) {
  
  if (AlbumCoverChoiceController::CanAcceptDrag(e)) {
    e->acceptProposedAction();
  }

  QWidget::dragEnterEvent(e);
  
}

void PlayingWidget::dropEvent(QDropEvent *e) {
  
  album_cover_choice_controller_->SaveCover(&metadata_, e);

  QWidget::dropEvent(e);
  
}

bool PlayingWidget::GetCoverAutomatically() {
  
  // Search for cover automatically?
  bool search =
      album_cover_choice_controller_->search_cover_auto_action()->isChecked() &&
      !metadata_.has_manually_unset_cover() &&
      metadata_.art_automatic().isEmpty() && metadata_.art_manual().isEmpty() &&
      !metadata_.artist().isEmpty() && !metadata_.album().isEmpty();

  if (search) {
    //qLog(Debug) << "GetCoverAutomatically";
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

void PlayingWidget::AutomaticCoverSearchDone() {

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}

void PlayingWidget::SetEnabled() {

  if (enabled_ == true) return;
  
  if ((active_ == true) && (visible_ == false)) SetVisible(true);

  enabled_ = true;

}

void PlayingWidget::SetDisabled() {

  if (enabled_ == false) return;

  if (visible_ == true) SetVisible(false);
  
  enabled_ = false;

}
