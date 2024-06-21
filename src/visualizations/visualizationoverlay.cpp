/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QBasicTimer>
#include <QTimeLine>
#include <QGraphicsProxyWidget>
#include <QTimerEvent>
#include <QMouseEvent>

#include "core/iconloader.h"
#include "visualizationoverlay.h"
#include "ui_visualizationoverlay.h"

namespace {
constexpr int kFadeDuration = 500;
constexpr int kFadeTimeout = 5000;
}

VisualizationOverlay::VisualizationOverlay(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_VisualizationOverlay),
      fade_timeline_(new QTimeLine(kFadeDuration, this)),
      visible_(false) {

  ui_->setupUi(this);

  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);

  ui_->settings->setIcon(IconLoader::Load(QStringLiteral("configure")));

  QObject::connect(ui_->settings, &QToolButton::clicked, this, &VisualizationOverlay::ShowSettingsMenu);
  QObject::connect(fade_timeline_, &QTimeLine::valueChanged, this, &VisualizationOverlay::OpacityChanged);

}

VisualizationOverlay::~VisualizationOverlay() { delete ui_; }

QGraphicsProxyWidget *VisualizationOverlay::title(QGraphicsProxyWidget *proxy) const {
  return proxy->createProxyForChildWidget(ui_->song_title);
}

void VisualizationOverlay::SetActions(QAction *previous, QAction *play_pause, QAction *stop, QAction *next) {

  ui_->previous->setDefaultAction(previous);
  ui_->play_pause->setDefaultAction(play_pause);
  ui_->stop->setDefaultAction(stop);
  ui_->next->setDefaultAction(next);

}

void VisualizationOverlay::ShowSettingsMenu() {

  emit ShowPopupMenu(ui_->settings->mapToGlobal(ui_->settings->rect().bottomLeft()));

}

void VisualizationOverlay::timerEvent(QTimerEvent *e) {

  QWidget::timerEvent(e);

  if (e->timerId() == fade_out_timeout_.timerId()) {
    SetVisible(false);
  }

}

void VisualizationOverlay::SetVisible(const bool visible) {

  // If we're showing the overlay, then fade out again in a little while
  fade_out_timeout_.stop();
  if (visible) fade_out_timeout_.start(kFadeTimeout, this);

  // Don't change to the state we're in already
  if (visible == visible_) return;
  visible_ = visible;

  // If there's already another fader running then start from the same time that one was already at.
  int start_time = visible ? 0 : fade_timeline_->duration();
  if (fade_timeline_->state() == QTimeLine::Running)
    start_time = fade_timeline_->currentTime();

  fade_timeline_->stop();
  fade_timeline_->setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
  fade_timeline_->setCurrentTime(start_time);
  fade_timeline_->resume();

}

void VisualizationOverlay::SetSongTitle(const QString &title) {

  ui_->song_title->setText(title);
  SetVisible(true);

}
