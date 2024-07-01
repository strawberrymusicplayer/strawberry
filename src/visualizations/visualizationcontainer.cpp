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

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QOpenGLWidget>
#else
#  include <QGLWidget>
#endif

#include <QGraphicsProxyWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QActionGroup>

#include "core/logging.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "engine/gstengine.h"
#include "visualizationcontainer.h"
#include "visualizationopenglwidget.h"
#include "visualizationoverlay.h"
#include "visualizationselector.h"
#include "projectmvisualization.h"

const char *VisualizationContainer::kSettingsGroup = "Visualizations";

namespace {
constexpr int kLowFramerate = 15;
constexpr int kMediumFramerate = 25;
constexpr int kHighFramerate = 35;
constexpr int kSuperHighFramerate = 60;

constexpr int kDefaultWidth = 828;
constexpr int kDefaultHeight = 512;
constexpr int kDefaultFps = kHighFramerate;
constexpr int kDefaultTextureSize = 512;
}  // namespace

VisualizationContainer::VisualizationContainer(QWidget *parent)
    : QGraphicsView(parent),
      projectm_visualization_(new ProjectMVisualization(this)),
      overlay_(new VisualizationOverlay),
      selector_(new VisualizationSelector(this)),
      overlay_proxy_(nullptr),
      engine_(nullptr),
      menu_(new QMenu(this)),
      fps_(kDefaultFps),
      size_(kDefaultTextureSize) {

  setWindowTitle(tr("Visualizations"));
  setWindowIcon(IconLoader::Load(QStringLiteral("strawberry")));
  setMinimumSize(64, 64);

  {
    Settings s;
    s.beginGroup(QLatin1String(kSettingsGroup));
    if (!restoreGeometry(s.value("geometry").toByteArray())) {
      resize(kDefaultWidth, kDefaultHeight);
    }
    fps_ = s.value("fps", kDefaultFps).toInt();
    size_ = s.value("size", kDefaultTextureSize).toInt();
    s.endGroup();
  }

  QShortcut *close = new QShortcut(QKeySequence::Close, this);
  QObject::connect(close, &QShortcut::activated, this, &VisualizationContainer::close);

  // Set up the graphics view
  setScene(projectm_visualization_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  setViewport(new VisualizationOpenGLWidget(projectm_visualization_));
#else
  setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
#endif
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFrameStyle(QFrame::NoFrame);

  // Add the overlay
  overlay_proxy_ = scene()->addWidget(overlay_);
  QObject::connect(overlay_, &VisualizationOverlay::OpacityChanged, this, &VisualizationContainer::ChangeOverlayOpacity);
  QObject::connect(overlay_, &VisualizationOverlay::ShowPopupMenu, this, &VisualizationContainer::ShowPopupMenu);
  ChangeOverlayOpacity(0.0);

  projectm_visualization_->SetTextureSize(size_);
  SizeChanged();

  // Selector
  selector_->SetVisualization(projectm_visualization_);

  // Settings menu
  menu_->addAction(IconLoader::Load(QStringLiteral("view-fullscreen")), tr("Toggle fullscreen"), this, &VisualizationContainer::ToggleFullscreen);

  QMenu *fps_menu = menu_->addMenu(tr("Framerate"));
  QActionGroup *fps_group = new QActionGroup(this);
  AddFramerateMenuItem(tr("Low (%1 fps)").arg(kLowFramerate), kLowFramerate, fps_, fps_group);
  AddFramerateMenuItem(tr("Medium (%1 fps)").arg(kMediumFramerate), kMediumFramerate, fps_, fps_group);
  AddFramerateMenuItem(tr("High (%1 fps)").arg(kHighFramerate), kHighFramerate, fps_, fps_group);
  AddFramerateMenuItem(tr("Super high (%1 fps)").arg(kSuperHighFramerate), kSuperHighFramerate, fps_, fps_group);
  fps_menu->addActions(fps_group->actions());

  QMenu *quality_menu = menu_->addMenu(tr("Quality", "Visualization quality"));
  QActionGroup *quality_group = new QActionGroup(this);
  AddQualityMenuItem(tr("Low (256x256)"), 256, size_, quality_group);
  AddQualityMenuItem(tr("Medium (512x512)"), 512, size_, quality_group);
  AddQualityMenuItem(tr("High (1024x1024)"), 1024, size_, quality_group);
  AddQualityMenuItem(tr("Super high (2048x2048)"), 2048, size_, quality_group);
  quality_menu->addActions(quality_group->actions());

  menu_->addAction(tr("Select visualizations..."), selector_, &VisualizationContainer::show);

  menu_->addSeparator();
  menu_->addAction(IconLoader::Load(QStringLiteral("application-exit")), tr("Close visualization"), this, &VisualizationContainer::hide);

}

void VisualizationContainer::AddFramerateMenuItem(const QString &name, const int value, const int def, QActionGroup *group) {

  QAction *action = group->addAction(name);
  action->setCheckable(true);
  action->setChecked(value == def);
  QObject::connect(action, &QAction::triggered, this, [this, value]() { SetFps(value); });

}

void VisualizationContainer::AddQualityMenuItem(const QString &name, const int value, const int def, QActionGroup *group) {

  QAction *action = group->addAction(name);
  action->setCheckable(true);
  action->setChecked(value == def);
  QObject::connect(action, &QAction::triggered, this, [this, value]() { SetQuality(value); });

}

void VisualizationContainer::SetEngine(GstEngine *engine) {

  engine_ = engine;

  if (isVisible()) engine_->AddBufferConsumer(projectm_visualization_);

}

void VisualizationContainer::showEvent(QShowEvent *e) {

  qLog(Debug) << "Showing visualization";

  QGraphicsView::showEvent(e);

  update_timer_.start(1000 / fps_, this);

  if (engine_) engine_->AddBufferConsumer(projectm_visualization_);

}

void VisualizationContainer::hideEvent(QHideEvent *e) {

  qLog(Debug) << "Hiding visualization";

  QGraphicsView::hideEvent(e);

  update_timer_.stop();

  if (engine_) engine_->RemoveBufferConsumer(projectm_visualization_);

}

void VisualizationContainer::closeEvent(QCloseEvent *e) {

  Q_UNUSED(e);

  // Don't close the window. Just hide it.
  e->ignore();
  hide();

}

void VisualizationContainer::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  SizeChanged();
}

void VisualizationContainer::SizeChanged() {

  // Save the geometry
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("geometry", saveGeometry());

  // Resize the scene
  if (scene()) scene()->setSceneRect(QRect(QPoint(0, 0), size()));

  // Resize the overlay
  if (overlay_) overlay_->resize(size());

}

void VisualizationContainer::timerEvent(QTimerEvent *e) {

  QGraphicsView::timerEvent(e);
  if (e->timerId() == update_timer_.timerId()) scene()->update();

}

void VisualizationContainer::SetActions(QAction *previous, QAction *play_pause, QAction *stop, QAction *next) {
  overlay_->SetActions(previous, play_pause, stop, next);
}

void VisualizationContainer::SongMetadataChanged(const Song &metadata) {
  overlay_->SetSongTitle(QStringLiteral("%1 - %2").arg(metadata.artist(), metadata.title()));
}

void VisualizationContainer::Stopped() {
  overlay_->SetSongTitle(tr("strawberry"));
}

void VisualizationContainer::ChangeOverlayOpacity(const qreal value) {

  overlay_proxy_->setOpacity(value);

  // Hide the cursor if the overlay is hidden
  if (value < 0.5) {
    viewport()->setCursor(Qt::BlankCursor);
  }
  else {
    viewport()->unsetCursor();
  }


}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VisualizationContainer::enterEvent(QEnterEvent *e) {
#else
void VisualizationContainer::enterEvent(QEvent *e) {
#endif

  QGraphicsView::enterEvent(e);

  overlay_->SetVisible(true);

}

void VisualizationContainer::leaveEvent(QEvent *e) {
  QGraphicsView::leaveEvent(e);
  overlay_->SetVisible(false);
}

void VisualizationContainer::mouseMoveEvent(QMouseEvent *e) {
  QGraphicsView::mouseMoveEvent(e);
  overlay_->SetVisible(true);
}

void VisualizationContainer::mouseDoubleClickEvent(QMouseEvent *e) {
  QGraphicsView::mouseDoubleClickEvent(e);
  ToggleFullscreen();
}

void VisualizationContainer::contextMenuEvent(QContextMenuEvent *event) {
  QGraphicsView::contextMenuEvent(event);
  ShowPopupMenu(event->pos());
}

void VisualizationContainer::keyReleaseEvent(QKeyEvent *event) {

  if (event->matches(QKeySequence::Close) || event->key() == Qt::Key_Escape) {
    if (isFullScreen()) {
      ToggleFullscreen();
    }
    else {
      hide();
    }
    return;
  }

  QGraphicsView::keyReleaseEvent(event);

}

void VisualizationContainer::ToggleFullscreen() {

  setWindowState(windowState() ^ Qt::WindowFullScreen);

}

void VisualizationContainer::SetFps(const int fps) {

  fps_ = fps;

  // Save settings
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("fps", fps_);

  update_timer_.stop();
  update_timer_.start(1000 / fps_, this);

}

void VisualizationContainer::ShowPopupMenu(const QPoint &pos) {
  menu_->popup(mapToGlobal(pos));
}

void VisualizationContainer::SetQuality(const int size) {

  size_ = size;

  // Save settings
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("size", size_);

  projectm_visualization_->SetTextureSize(size_);

}
