/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QtGlobal>
#include <QWidget>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QMenu>
#include <QMovie>
#include <QPainter>
#include <QTextDocument>
#include <QTimeLine>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QtEvents>

#include "core/settings.h"
#include "utilities/imageutils.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "playingwidget.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;

namespace {

constexpr char kSettingsGroup[] = "PlayingWidget";

// Space between the cover and the details in small mode
constexpr int kPadding = 2;

// Maximum height of the cover in large mode, and offset between the bottom of the cover and bottom of the widget
constexpr int kMaxCoverSize = 260;
constexpr int kBottomOffset = 0;

// Border for large mode
constexpr int kTopBorder = 4;

}  // namespace

PlayingWidget::PlayingWidget(QWidget *parent)
    : QWidget(parent),
      album_cover_choice_controller_(nullptr),
      mode_(Mode::LargeSongDetails),
      menu_(new QMenu(this)),
      above_statusbar_action_(nullptr),
      fit_cover_width_action_(nullptr),
      enabled_(false),
      visible_(false),
      playing_(false),
      active_(false),
      small_ideal_height_(0),
      total_height_(0),
      desired_height_(0),
      fit_width_(false),
      timeline_show_hide_(new QTimeLine(500, this)),
      timeline_fade_(new QTimeLine(1000, this)),
      details_(new QTextDocument(this)),
      pixmap_previous_track_opacity_(0.0),
      downloading_covers_(false) {

  SetHeight(0);

  // Load settings
  Settings s;
  s.beginGroup(kSettingsGroup);
  mode_ = static_cast<Mode>(s.value("mode", static_cast<int>(Mode::LargeSongDetails)).toInt());
  fit_width_ = s.value("fit_cover_width", false).toBool();
  s.endGroup();

  // Accept drops for setting album art
  setAcceptDrops(true);

  // Context menu
  QActionGroup *mode_group = new QActionGroup(this);
  CreateModeAction(Mode::SmallSongDetails, tr("Small album cover"), mode_group);
  CreateModeAction(Mode::LargeSongDetails, tr("Large album cover"), mode_group);
  menu_->addActions(mode_group->actions());

  fit_cover_width_action_ = menu_->addAction(tr("Fit cover to width"));
  fit_cover_width_action_->setCheckable(true);
  fit_cover_width_action_->setEnabled(true);
  QObject::connect(fit_cover_width_action_, &QAction::toggled, this, &PlayingWidget::FitCoverWidth);
  fit_cover_width_action_->setChecked(fit_width_);
  menu_->addSeparator();

  // Animations
  QObject::connect(timeline_show_hide_, &QTimeLine::frameChanged, this, &PlayingWidget::SetHeight);
  QObject::connect(timeline_fade_, &QTimeLine::valueChanged, this, &PlayingWidget::FadePreviousTrack);
  timeline_fade_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

  details_->setUndoRedoEnabled(false);
  // add placeholder text to get the correct height
  if (mode_ == Mode::LargeSongDetails) {
    details_->setDefaultStyleSheet(u"p { font-size: small; font-weight: bold; }"_s);
    details_->setHtml(u"<p align=center><i></i><br/><br/></p>"_s);
  }

  UpdateHeight();

}

void PlayingWidget::Init(AlbumCoverChoiceController *album_cover_choice_controller) {

  album_cover_choice_controller_ = album_cover_choice_controller;

  QList<QAction*> cover_actions = album_cover_choice_controller_->GetAllActions();
  menu_->addActions(cover_actions);
  menu_->addSeparator();
  menu_->addAction(album_cover_choice_controller_->search_cover_auto_action());
  menu_->addSeparator();

  above_statusbar_action_ = menu_->addAction(tr("Show above status bar"));
  above_statusbar_action_->setCheckable(true);
  Settings s;
  s.beginGroup(kSettingsGroup);
  above_statusbar_action_->setChecked(s.value("above_status_bar", false).toBool());
  s.endGroup();
  QObject::connect(above_statusbar_action_, &QAction::toggled, this, &PlayingWidget::ShowAboveStatusBar);

  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::AutomaticCoverSearchDone, this, &PlayingWidget::AutomaticCoverSearchDone);

}

void PlayingWidget::SetEnabled(const bool enabled) {

  if (enabled == enabled_) return;

  if (enabled) SetEnabled();
  else SetDisabled();

}

void PlayingWidget::SetEnabled() {

  if (enabled_) return;
  enabled_ = true;

  if (active_) {
    SetVisible(true);
  }

}

void PlayingWidget::SetDisabled() {

  if (!enabled_) return;
  enabled_ = false;

  SetVisible(false);

}

void PlayingWidget::SetVisible(const bool visible) {

  if (timeline_show_hide_->state() == QTimeLine::Running) {
    if (timeline_show_hide_->direction() == QTimeLine::Backward && enabled_ && active_) {
      timeline_show_hide_->toggleDirection();
    }
    else if (timeline_show_hide_->direction() == QTimeLine::Forward && (!enabled_ || !active_)) {
      timeline_show_hide_->toggleDirection();
    }
    return;
  }

  if (visible != visible_) {
    timeline_show_hide_->setFrameRange(0, total_height_);
    timeline_show_hide_->setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
    timeline_show_hide_->start();
  }

}

void PlayingWidget::set_ideal_height(const int height) {

  small_ideal_height_ = height;
  UpdateHeight();

}

QSize PlayingWidget::sizeHint() const {
  return QSize(desired_height_, total_height_);
}

void PlayingWidget::CreateModeAction(const Mode mode, const QString &text, QActionGroup *group) {

  QAction *action = new QAction(text, group);
  action->setCheckable(true);
  QObject::connect(action, &QAction::triggered, this, [this, mode]() { SetMode(mode); });

  if (mode == mode_) action->setChecked(true);

}

void PlayingWidget::SetMode(const Mode mode) {

  mode_ = static_cast<Mode>(mode);

  fit_cover_width_action_->setEnabled(mode_ != Mode::SmallSongDetails);

  UpdateHeight();
  UpdateDetailsText();
  update();

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("mode", static_cast<int>(mode_));
  s.endGroup();

}

void PlayingWidget::FitCoverWidth(const bool fit) {

  fit_width_ = fit;
  UpdateHeight();
  update();

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("fit_cover_width", fit_width_);
  s.endGroup();

}

void PlayingWidget::ShowAboveStatusBar(const bool above) {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("above_status_bar", above);
  s.endGroup();

  Q_EMIT ShowAboveStatusBarChanged(above);

}

void PlayingWidget::Playing() {}

void PlayingWidget::Stopped() {

  playing_ = false;
  active_ = false;
  song_playing_ = Song();
  song_ = Song();
  image_current_ = QImage();
  SetVisible(false);

}

void PlayingWidget::Error() {
  active_ = false;
}

void PlayingWidget::SongChanged(const Song &song) {

  bool changed = (song.artist() != song_playing_.artist() || song.album() != song_playing_.album() || song.title() != song_playing_.title());

  playing_ = true;
  song_playing_ = song;
  song_ = song;

  if (changed) UpdateDetailsText();

}

void PlayingWidget::AlbumCoverLoaded(const Song &song, const QImage &image) {

  if (!playing_ || song != song_playing_ || image == image_current_) return;

  active_ = true;
  downloading_covers_ = false;
  song_ = song;
  image_current_ = image;

  SetImage(image);

}

void PlayingWidget::SetImage(const QImage &image) {

  if (enabled_ && visible_ && active_) {
    // Cache the current pixmap so we can fade between them
    QSize psize;
    psize.setWidth(static_cast<int>(size().width() * devicePixelRatioF()));
    if (size().height() > 0) {
      psize.setHeight(static_cast<int>(size().height() * devicePixelRatioF()));
    }
    else {
      psize.setHeight(static_cast<int>(total_height_ * devicePixelRatioF()));
    }
    pixmap_previous_track_ = QPixmap(psize);
    pixmap_previous_track_.setDevicePixelRatio(devicePixelRatioF());
    pixmap_previous_track_.fill(palette().window().color());
    pixmap_previous_track_opacity_ = 1.0;
    QPainter p(&pixmap_previous_track_);
    DrawContents(&p);
    p.end();
  }
  else {
    pixmap_previous_track_ = QPixmap();
  }

  image_original_ = image;
  UpdateDetailsText();
  ScaleCover();

  if (enabled_ && active_) {
    SetVisible(true);
    // Were we waiting for this cover to load before we started fading?
    if (!pixmap_previous_track_.isNull()) {
      timeline_fade_->stop();
      timeline_fade_->start();
    }
  }

}

void PlayingWidget::ScaleCover() {

  QImage image = ImageUtils::ScaleImage(image_original_, QSize(desired_height_, desired_height_), devicePixelRatioF(), true);
  if (image.isNull()) pixmap_cover_ = QPixmap();
  else pixmap_cover_ = QPixmap::fromImage(image);
  update();

}

void PlayingWidget::SetHeight(int height) {

  setMaximumHeight(height);
  update();

  if (height >= total_height_ - 5) visible_ = true;
  if (height <= 5) visible_ = false;

  if (timeline_show_hide_->state() == QTimeLine::Running) {
    if (timeline_show_hide_->direction() == QTimeLine::Backward && enabled_ && active_) {
      timeline_show_hide_->toggleDirection();
    }
    if (timeline_show_hide_->direction() == QTimeLine::Forward && (!enabled_ || !active_)) {
      timeline_show_hide_->toggleDirection();
    }
  }

}

void PlayingWidget::UpdateHeight() {

  switch (mode_) {
    case Mode::SmallSongDetails:
      desired_height_ = small_ideal_height_;
      total_height_ = small_ideal_height_;
      break;
    case Mode::LargeSongDetails:
      if (fit_width_) desired_height_ = width();
      else desired_height_ = qMin(kMaxCoverSize, width());
      total_height_ = kTopBorder + desired_height_ + kBottomOffset + static_cast<int>(details_->size().height());
      break;
  }

  // Update the animation settings and resize the widget now if we're visible
  timeline_show_hide_->setFrameRange(0, total_height_);
  if (visible_ && active_ && timeline_show_hide_->state() != QTimeLine::Running) {
    setMaximumHeight(total_height_);
  }

  // Re-scale the current image
  if (song_.is_valid()) {
    ScaleCover();
  }

  // Tell Qt we've changed size
  updateGeometry();

}

void PlayingWidget::UpdateDetailsText() {

  QString html;
  details_->setDefaultStyleSheet(u"p { font-size: small; font-weight: bold; }"_s);
  switch (mode_) {
    case Mode::SmallSongDetails:
      details_->setTextWidth(-1);
      html += "<p>"_L1;
      break;
    case Mode::LargeSongDetails:
      details_->setTextWidth(desired_height_);
      html += "<p align=center>"_L1;
      break;
  }

  html += QStringLiteral("%1<br/>%2<br/>%3").arg(song_.PrettyTitle().toHtmlEscaped(), song_.artist().toHtmlEscaped(), song_.album().toHtmlEscaped());
  html += "</p>"_L1;

  details_->setHtml(html);

  // if something spans multiple lines the height needs to change
  if (mode_ == Mode::LargeSongDetails) UpdateHeight();

  update();

}

void PlayingWidget::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e);

  QPainter p(this);

  DrawContents(&p);

  // Draw the previous track's image if we're fading
  if (!pixmap_previous_track_.isNull()) {
    p.setOpacity(pixmap_previous_track_opacity_);
    p.drawPixmap(0, 0, pixmap_previous_track_);
  }

}

void PlayingWidget::DrawContents(QPainter *p) {

  p->setRenderHint(QPainter::SmoothPixmapTransform);

  switch (mode_) {
    case Mode::SmallSongDetails:
      // Draw the cover
      p->drawPixmap(0, 0, small_ideal_height_, small_ideal_height_, pixmap_cover_);
      if (downloading_covers_) {
        p->drawPixmap(small_ideal_height_ - 18, 6, 16, 16, spinner_animation_->currentPixmap());
      }

      // Draw the details
      p->translate(small_ideal_height_ + kPadding, 0);
      details_->drawContents(p);
      p->translate(-small_ideal_height_ - kPadding, 0);
      break;

    case Mode::LargeSongDetails:
      // Work out how high the text is going to be
      const int text_height = static_cast<int>(details_->size().height());
      const int cover_size = fit_width_ ? width() : qMin(kMaxCoverSize, width());
      const int x_offset = (width() - desired_height_) / 2;

      // Draw the cover
      p->drawPixmap(x_offset, kTopBorder, cover_size, cover_size, pixmap_cover_);
      if (downloading_covers_) {
        p->drawPixmap(x_offset + 45, 35, 16, 16, spinner_animation_->currentPixmap());
      }

      // Draw the text below
      if (timeline_show_hide_->state() != QTimeLine::Running) {
        p->translate(x_offset, height() - text_height);
        details_->drawContents(p);
        p->translate(-x_offset, -height() + text_height);
      }

      break;
  }

}

void PlayingWidget::FadePreviousTrack(const qreal value) {

  if (!visible_) return;

  pixmap_previous_track_opacity_ = value;
  if (qFuzzyCompare(pixmap_previous_track_opacity_, static_cast<qreal>(0.0))) {
    pixmap_previous_track_ = QPixmap();
  }

  update();

}

void PlayingWidget::resizeEvent(QResizeEvent *e) {

  //if (visible_ && e->oldSize() != e->size()) {
  if (e->oldSize() != e->size()) {
    if (mode_ == Mode::LargeSongDetails) {
      UpdateHeight();
      UpdateDetailsText();
    }
  }

}

void PlayingWidget::contextMenuEvent(QContextMenuEvent *e) {

  // show the menu
  menu_->popup(mapToGlobal(e->pos()));
}

void PlayingWidget::mouseDoubleClickEvent(QMouseEvent *e) {

  // Same behaviour as right-click > Show Fullsize
  if (e->button() == Qt::LeftButton && song_.is_valid()) {
    album_cover_choice_controller_->ShowCover(song_, image_original_);
  }

}

void PlayingWidget::dragEnterEvent(QDragEnterEvent *e) {

  if (AlbumCoverChoiceController::CanAcceptDrag(e)) {
    e->acceptProposedAction();
  }

  QWidget::dragEnterEvent(e);

}

void PlayingWidget::dropEvent(QDropEvent *e) {

  album_cover_choice_controller_->SaveCover(&song_, e);

  QWidget::dropEvent(e);

}

void PlayingWidget::SearchCoverInProgress() {

  downloading_covers_ = true;

  // Show a spinner animation
  spinner_animation_ = make_unique<QMovie>(u":/pictures/spinner.gif"_s, QByteArray(), this);
  QObject::connect(&*spinner_animation_, &QMovie::updated, this, &PlayingWidget::Update);
  spinner_animation_->start();
  update();

}

void PlayingWidget::AutomaticCoverSearchDone() {

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}
