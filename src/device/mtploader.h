/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MTPLOADER_H
#define MTPLOADER_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"

class QThread;
class TaskManager;
class CollectionBackend;
class MtpConnection;

class MtpLoader : public QObject {
  Q_OBJECT

 public:
  explicit MtpLoader(const QUrl &url, const SharedPtr<TaskManager> task_manager, const SharedPtr<CollectionBackend> backend, QObject *parent = nullptr);
  ~MtpLoader() override;

  bool Init();
  void Abort() { abort_ = true; }

 public Q_SLOTS:
  void LoadDatabase();

 Q_SIGNALS:
  void Error(const QString &message);
  void TaskStarted(const int task_id);
  void LoadFinished(const bool success, MtpConnection *connection);

 private:
  bool TryLoad();

 private:
  QUrl url_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<CollectionBackend> backend_;
  ScopedPtr<MtpConnection> connection_;
  QThread *original_thread_;
  bool abort_;

};

#endif  // MTPLOADER_H
