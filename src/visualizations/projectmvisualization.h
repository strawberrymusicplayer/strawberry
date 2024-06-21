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

#ifndef PROJECTMVISUALIZATION_H
#define PROJECTMVISUALIZATION_H

#include "config.h"

#include <memory>
#include <vector>

#ifdef HAVE_PROJECTM4
#  include <projectM-4/types.h>
#  include <projectM-4/playlist_types.h>
#else
#  include <libprojectM/projectM.hpp>
#endif

#include <QString>
#include <QStringList>
#include <QGraphicsScene>

#include "engine/gstbufferconsumer.h"

class projectM;
class QPainter;
class ProjectMPresetModel;
class VisualizationContainer;

class ProjectMVisualization : public QGraphicsScene, public GstBufferConsumer {
  Q_OBJECT

 public:
  explicit ProjectMVisualization(VisualizationContainer *container);
  ~ProjectMVisualization();

  enum class Mode {
    Random = 0,
    FromList = 1,
  };

  QString preset_path() const;
  ProjectMPresetModel *preset_model() const { return preset_model_; }

  Mode mode() const { return mode_; }
  int duration() const { return duration_; }

  void Init();

  // BufferConsumer
  void ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format, const int channels) override;

 public slots:
  void SetTextureSize(const int size);
  void SetDuration(const int seconds);

  void SetSelected(const QStringList &paths, const bool selected);
  void ClearSelected();
  void SetImmediatePreset(const int index);
  void SetImmediatePreset(const QString &path);
  void SetMode(const Mode mode);

  void Lock(const bool lock);

 protected:
  // QGraphicsScene
  void drawBackground(QPainter *painter, const QRectF &rect) override;

 private slots:
  void SceneRectChanged(const QRectF &rect);

 private:
  void Load();
  void Save();

  int IndexOfPreset(const QString &preset_path) const;

  void Resize(const qreal width, const qreal height, const qreal pixel_ratio);

 private:
  VisualizationContainer *container_;
  ProjectMPresetModel *preset_model_;
#ifdef HAVE_PROJECTM4
  projectm_handle projectm_instance_;
  projectm_playlist_handle projectm_playlist_instance_;
#else
  std::unique_ptr<projectM> projectm_;
#endif
  Mode mode_;
  int duration_;
  std::vector<int> default_rating_list_;
  int texture_size_;
  QString preset_path_;
};

#endif  // PROJECTMVISUALIZATION_H
