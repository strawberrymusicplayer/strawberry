/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ORGANISE_H
#define ORGANISE_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QObject>
#include <QThread>
#include <QBasicTimer>
#include <QFileInfo>
#include <QList>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>

#include "core/song.h"
#include "organiseformat.h"

class QTimerEvent;

class MusicStorage;
class TaskManager;
#ifdef HAVE_GSTREAMER
class Transcoder;
#endif

class Organise : public QObject {
  Q_OBJECT

 public:
  struct NewSongInfo {
    NewSongInfo(const Song &song = Song(), const QString &new_filename = QString()) : song_(song), new_filename_(new_filename) {}
    Song song_;
    QString new_filename_;
  };
  typedef QList<NewSongInfo> NewSongInfoList;

  Organise(TaskManager *task_manager, std::shared_ptr<MusicStorage> destination, const OrganiseFormat &format, bool copy, bool overwrite, bool mark_as_listened, bool albumcover, const NewSongInfoList &songs, bool eject_after, const QString &playlist = QString());
  ~Organise();

  static const int kBatchSize;
#ifdef HAVE_GSTREAMER
  static const int kTranscodeProgressInterval;
#endif

  void Start();

 signals:
  void Finished(const QStringList &files_with_errors, QStringList);
  void FileCopied(int database_id);
  void SongPathChanged(const Song &song, const QFileInfo &new_file);

 protected:
  void timerEvent(QTimerEvent *e);

 private slots:
  void ProcessSomeFiles();
#ifdef HAVE_GSTREAMER
  void FileTranscoded(const QString &input, const QString &output, bool success);
#endif
  void LogLine(const QString message);

 private:
  void SetSongProgress(float progress, bool transcoded = false);
  void UpdateProgress();
#ifdef HAVE_GSTREAMER
  Song::FileType CheckTranscode(Song::FileType original_type) const;
#endif

 private:
  struct Task {
    explicit Task(const NewSongInfo &song_info = NewSongInfo()) :
      song_info_(song_info),
      transcode_progress_(0.0)
      {}

    NewSongInfo song_info_;
    float transcode_progress_;
    QString transcoded_filename_;
    QString new_extension_;
    Song::FileType new_filetype_;
  };

  QThread *thread_;
  QThread *original_thread_;
  TaskManager *task_manager_;
#ifdef HAVE_GSTREAMER
  Transcoder *transcoder_;
#endif
  std::shared_ptr<MusicStorage> destination_;
  QList<Song::FileType> supported_filetypes_;

  const OrganiseFormat format_;
  const bool copy_;
  const bool overwrite_;
  const bool mark_as_listened_;
  const bool albumcover_;
  const bool eject_after_;
  int task_count_;
  const QString playlist_;

#ifdef HAVE_GSTREAMER
  QBasicTimer transcode_progress_timer_;
#endif

  QList<Task> tasks_pending_;
#ifdef HAVE_GSTREAMER
  QMap<QString, Task> tasks_transcoding_;
#endif
  int tasks_complete_;

  bool started_;

  int task_id_;
  int current_copy_progress_;

  QStringList files_with_errors_;
  QStringList log_;
};

#endif  // ORGANISE_H
