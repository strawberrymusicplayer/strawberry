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

#include <string>
#include <memory>

#ifdef HAVE_PROJECTM4
#  include <projectM-4/types.h>
#  include <projectM-4/core.h>
#  include <projectM-4/parameters.h>
#  include <projectM-4/memory.h>
#  include <projectM-4/audio.h>
#  include <projectM-4/render_opengl.h>
#  include <projectM-4/playlist_types.h>
#  include <projectM-4/playlist_core.h>
#  include <projectM-4/playlist_memory.h>
#  include <projectM-4/playlist_items.h>
#  include <projectM-4/playlist_playback.h>
#else
#  include <libprojectM/projectM.hpp>
#endif  // HAVE_PROJECTM4

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QPainter>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QTimerEvent>

#include "core/logging.h"
#include "core/settings.h"
#include "projectmvisualization.h"
#include "projectmpresetmodel.h"
#include "visualizationcontainer.h"

ProjectMVisualization::ProjectMVisualization(VisualizationContainer *container)
    : QObject(container),
      container_(container),
      preset_model_(nullptr),
#ifdef HAVE_PROJECTM4
      projectm_instance_(nullptr),
      projectm_playlist_instance_(nullptr),
#endif
      mode_(Mode::Random),
      duration_(15),
      texture_size_(512),
      pixel_ratio_(container->devicePixelRatio()) {

#ifndef HAVE_PROJECTM4
  for (int i = 0; i < TOTAL_RATING_TYPES; ++i) {
    default_rating_list_.push_back(3);
  }
#endif  // HAVE_PROJECTM4

}

ProjectMVisualization::~ProjectMVisualization() {

#ifdef HAVE_PROJECTM4
  if (projectm_playlist_instance_) {
    projectm_playlist_destroy(projectm_playlist_instance_);
  }
  if (projectm_instance_) {
    projectm_destroy(projectm_instance_);
  }
#endif  // HAVE_PROJECTM4

}

void ProjectMVisualization::Init() {

#ifdef HAVE_PROJECTM4
  if (projectm_instance_) {
    return;
  }
#else
  if (projectm_) {
    return;
  }
#endif  // HAVE_PROJECTM4

  // Find the projectM presets

  QStringList data_paths = QStringList() << QStringLiteral("/usr/share")
                                         << QStringLiteral("/usr/local/share")
                                         << QLatin1String(CMAKE_INSTALL_PREFIX) + QLatin1String("/share");

  const QStringList xdg_data_dirs = QString::fromUtf8(qgetenv("XDG_DATA_DIRS")).split(QLatin1Char(':'));
  for (const QString &xdg_data_dir : xdg_data_dirs) {
    if (!data_paths.contains(xdg_data_dir)) {
      data_paths.append(xdg_data_dir);
    }
  }

#if defined(Q_OS_WIN32)
  data_paths.prepend(QCoreApplication::applicationDirPath());
#endif

  const QStringList projectm_paths = QStringList() << QStringLiteral("projectM/presets")
                                                   << QStringLiteral("projectm-presets");

  QStringList preset_paths;
  for (const QString &data_path : std::as_const(data_paths)) {
    for (const QString &projectm_path : projectm_paths) {
      const QString path = data_path + QLatin1Char('/') + projectm_path;
      if (!QFileInfo::exists(path) || QDir(path).entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        preset_paths << path;
        continue;
      }
      preset_path_ = path;
      break;
    }
  }

  // Create projectM settings
#ifdef HAVE_PROJECTM4
  Q_ASSERT(projectm_instance_ == nullptr);
  Q_ASSERT(projectm_playlist_instance_ == nullptr);
  projectm_instance_ = projectm_create();
  projectm_set_preset_duration(projectm_instance_, duration_);
  projectm_set_mesh_size(projectm_instance_, 32, 24);
  projectm_set_fps(projectm_instance_, 35);
  //projectm_set_window_size(projectm_instance_, 512, 512);
  const char *texture_search_paths[] = { "/usr/local/share/projectM/textures" };
  projectm_set_texture_search_paths(projectm_instance_, texture_search_paths, 1);
  projectm_playlist_instance_ = projectm_playlist_create(projectm_instance_);
#else
  projectM::Settings s;
  s.presetURL = preset_path_.toStdString();
  s.meshX = 32;
  s.meshY = 24;
  s.textureSize = texture_size_;
  s.fps = 35;
  s.windowWidth = 512;
  s.windowHeight = 512;
  s.smoothPresetDuration = 5;
  s.presetDuration = duration_;
  s.shuffleEnabled = true;
  s.softCutRatingsEnabled = false;
  s.easterEgg = 0;
  projectm_ = std::make_unique<projectM>(s);
#endif  // HAVE_PROJECTM4

  Q_ASSERT(preset_model_ == nullptr);
  preset_model_ = new ProjectMPresetModel(this, this);

  Load();

  // Start at a random preset.
#ifdef HAVE_PROJECTM4
  const uint count = projectm_playlist_size(projectm_playlist_instance_);
  if (count > 0) {
    const uint position = QRandomGenerator::global()->bounded(count);
    projectm_playlist_set_position(projectm_playlist_instance_, position, true);
  }
#else
  const uint count = projectm_->getPlaylistSize();
  if (count > 0) {
    const uint selection = QRandomGenerator::global()->bounded(count);
    projectm_->selectPreset(selection, true);
  }
#endif  // HAVE_PROJECTM4

  if (preset_path_.isEmpty()) {
    qWarning("ProjectM presets could not be found, search path was:\n  %s", preset_paths.join(QLatin1String("\n  ")).toLocal8Bit().constData());
    QMessageBox::warning(nullptr, tr("Missing projectM presets"), tr("Strawberry could not load any projectM visualizations.  Check that you have installed Strawberry properly."));
  }

}

void ProjectMVisualization::RenderFrame(const int width, const int height) {

#ifdef HAVE_PROJECTM4
  if (!projectm_instance_) {
    Init();
  }
  Q_ASSERT(projectm_instance_);
#else
  if (!projectm_) {
    Init();
  }
  Q_ASSERT(projectm_);
#endif

  //Resize(width, height);

#ifdef HAVE_PROJECTM4
  projectm_opengl_render_frame(projectm_instance_);
#else
  projectm_->renderFrame();
#endif

}

void ProjectMVisualization::Resize(const int width, const int height) {

#ifdef HAVE_PROJECTM4
  if (projectm_instance_) {
    projectm_set_window_size(projectm_instance_, static_cast<size_t>(width * pixel_ratio_), static_cast<size_t>(height * pixel_ratio_));
  }
#else
  if (projectm_) {
    projectm_->projectM_resetGL(static_cast<int>(width * pixel_ratio_), static_cast<int>(height * pixel_ratio_));
  }
#endif  // HAVE_PROJECTM4

}

void ProjectMVisualization::SceneRectChanged(const QRectF &rect) {

  // NOTE: This should be updated on a QScreen dpi change signal.
  // Accessing the QScreen becomes a lot easier in Qt 5.14 with QWidget::screen().
  pixel_ratio_ = container_->devicePixelRatio();

  //Resize(rect.width(), rect.height());

}

void ProjectMVisualization::SetTextureSize(const int size) {

  texture_size_ = size;

#ifndef HAVE_PROJECTM4
  if (projectm_) {
    projectm_->changeTextureSize(texture_size_);
  }
#endif  // HAVE_PROJECTM4

}

void ProjectMVisualization::SetDuration(const int seconds) {

  duration_ = seconds;

#ifdef HAVE_PROJECTM4
  if (projectm_instance_) {
    projectm_set_preset_duration(projectm_instance_, duration_);
  }
#else
  if (projectm_) {
    projectm_->changePresetDuration(duration_);
  }
#endif  // HAVE_PROJECTM4

  Save();

}

void ProjectMVisualization::ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) {

  Q_UNUSED(pipeline_id);
  Q_UNUSED(format);

  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);

#ifdef HAVE_PROJECTM4
  if (projectm_instance_) {
    const unsigned int samples_per_channel = map.size / sizeof(int16_t) / PROJECTM_STEREO;
    const int16_t *data = reinterpret_cast<int16_t*>(map.data);
    projectm_pcm_add_int16(projectm_instance_, data, samples_per_channel, PROJECTM_STEREO);
  }
#else
  if (projectm_) {
    const short samples_per_channel = static_cast<short>(map.size) / sizeof(short) / 2;
    const short *data = reinterpret_cast<short*>(map.data);
    projectm_->pcm()->addPCM16Data(data, samples_per_channel);
  }
#endif  // HAVE_PROJECTM4

  gst_buffer_unmap(buffer, &map);
  gst_buffer_unref(buffer);

}

void ProjectMVisualization::SetSelected(const QStringList &paths, const bool selected) {

  for (const QString &path : paths) {
    const int index = IndexOfPreset(path);
    if (selected && index == -1) {
#ifdef HAVE_PROJECTM4
      projectm_playlist_add_preset(projectm_playlist_instance_, path.toUtf8().constData(), true);
#else
      projectm_->addPresetURL(path.toStdString(), std::string(), default_rating_list_);
#endif
    }
    else if (!selected && index != -1) {
#ifdef HAVE_PROJECTM4
      projectm_playlist_remove_preset(projectm_playlist_instance_, index);
#else
      projectm_->removePreset(index);
#endif
    }
  }

  Save();

}

void ProjectMVisualization::ClearSelected() {

#ifdef HAVE_PROJECTM4
  projectm_playlist_clear(projectm_playlist_instance_);
#else
  projectm_->clearPlaylist();
#endif

  Save();

}

int ProjectMVisualization::IndexOfPreset(const QString &preset_path) const {

#ifdef HAVE_PROJECTM4
  const uint count = projectm_playlist_size(projectm_playlist_instance_);
  for (uint i = 0; i < count; ++i) {
    char *projectm_preset_path = projectm_playlist_item(projectm_playlist_instance_, i);
    if (projectm_preset_path) {
      const QScopeGuard projectm_preset_path_deleter = qScopeGuard([projectm_preset_path](){ projectm_playlist_free_string(projectm_preset_path); });
      if (QLatin1String(projectm_preset_path) == preset_path) {
        return static_cast<int>(i);
      }
    }
  }
#else
  const uint count = projectm_->getPlaylistSize();
  for (uint i = 0; i < count; ++i) {
    if (QString::fromStdString(projectm_->getPresetURL(i)) == preset_path) return static_cast<int>(i);
  }
#endif  // HAVE_PROJECTM4

  return -1;

}

void ProjectMVisualization::Load() {

  Settings s;
  s.beginGroup(QLatin1String(VisualizationContainer::kSettingsGroup));
  mode_ = Mode(s.value("mode", 0).toInt());
  duration_ = s.value("duration", duration_).toInt();
  s.endGroup();

#ifdef HAVE_PROJECTM4
  projectm_set_preset_duration(projectm_instance_, duration_);
  projectm_playlist_clear(projectm_playlist_instance_);
#else
  projectm_->changePresetDuration(duration_);
  projectm_->clearPlaylist();
#endif  // HAVE_PROJECTM4

  switch (mode_) {
    case Mode::Random:{
      for (int i = 0; i < preset_model_->all_presets_.count(); ++i) {
#ifdef HAVE_PROJECTM4
        projectm_playlist_add_preset(projectm_playlist_instance_, preset_model_->all_presets_[i].path_.toUtf8().constData(), false);
#else
        projectm_->addPresetURL(preset_model_->all_presets_[i].path_.toStdString(), std::string(), default_rating_list_);
#endif
        preset_model_->all_presets_[i].selected_ = true;
      }
      break;
    }
    case Mode::FromList:{
      s.beginGroup(QLatin1String(VisualizationContainer::kSettingsGroup));
      const QStringList paths = s.value("preset_paths").toStringList();
      s.endGroup();
      for (const QString &path : paths) {
#ifdef HAVE_PROJECTM4
        projectm_playlist_add_preset(projectm_playlist_instance_, path.toUtf8().constData(), true);
#else
        projectm_->addPresetURL(path.toStdString(), std::string(), default_rating_list_);
#endif
        preset_model_->MarkSelected(path, true);
      }
    }
  }

}

void ProjectMVisualization::Save() {

  QStringList paths;

  for (const ProjectMPresetModel::Preset &preset : std::as_const(preset_model_->all_presets_)) {
    if (preset.selected_) paths << preset.path_;
  }

  Settings s;
  s.beginGroup(VisualizationContainer::kSettingsGroup);
  s.setValue("preset_paths", paths);
  s.setValue("mode", static_cast<int>(mode_));
  s.setValue("duration", duration_);
  s.endGroup();

}

void ProjectMVisualization::SetMode(const Mode mode) {

  mode_ = mode;
  Save();

}

QString ProjectMVisualization::preset_path() const {

#ifdef HAVE_PROJECTM4
  return preset_path_;
#else
  if (projectm_) {
    return QString::fromStdString(projectm_->settings().presetURL);
  }
  return QString();
#endif  // HAVE_PROJECTM4

}

void ProjectMVisualization::SetImmediatePreset(const int index) {

#ifdef HAVE_PROJECTM4
  if (projectm_playlist_instance_) {
    projectm_playlist_set_position(projectm_playlist_instance_, index, true);
  }
#else
  if (projectm_) {
    projectm_->selectPreset(index, true);
  }
#endif  // HAVE_PROJECTM4

}

void ProjectMVisualization::SetImmediatePreset(const QString &path) {

  const int index = IndexOfPreset(path);
  if (index != -1) {
    SetImmediatePreset(index);
  }

}

void ProjectMVisualization::Lock(const bool lock) {

#ifdef HAVE_PROJECTM4
  if (projectm_instance_) {
    projectm_set_preset_locked(projectm_instance_, lock);
  }
#else
  if (projectm_) {
    projectm_->setPresetLock(lock);
  }
#endif  // HAVE_PROJECTM4

  if (!lock) Load();

}
