/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <optional>

#include <QObject>
#include <QBasicTimer>
#include <QFileInfo>
#include <QSet>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "organizeformat.h"

class QThread;
class QTimer;
class QTimerEvent;

class TaskManager;
class TagReaderClient;
class MusicStorage;
class Transcoder;

class Organize : public QObject {
  Q_OBJECT

 public:
  struct NewSongInfo {
    explicit NewSongInfo() : unique_filename_(false) {}
    explicit NewSongInfo(const Song &song, const QString &new_filename, const bool unique_filename) : song_(song), new_filename_(new_filename), unique_filename_(unique_filename) {}
    Song song_;
    QString new_filename_;
    bool unique_filename_;
  };
  using NewSongInfoList = QList<NewSongInfo>;

  explicit Organize(const SharedPtr<TaskManager> task_manager,
                    const SharedPtr<TagReaderClient> tagreader_client,
                    const SharedPtr<MusicStorage> destination,
                    const OrganizeFormat &format,
                    const bool copy,
                    const bool overwrite,
                    const bool albumcover,
                    const NewSongInfoList &songs,
                    const bool eject_after,
                    const QString &playlist = QString(),
                    QObject *parent = nullptr);

  ~Organize() override;

  void Start();

  // Makes the destination filenames of the organized songs unique, so they don't overwrite each other.
  class UniqueFilenames {
   public:
    // The current files of the songs in the destination are reserved, so other songs don't get their filename, which would overwrite them before they are organized.
    explicit UniqueFilenames(const QString &root = QString(), const SongList &songs = SongList());

    // Returns the filename, or the filename with a number added when it's used by another song, like "foo(2).mp3".
    // The song can keep its own current file, source_filename.
    QString Make(const QString &filename, const QString &source_filename = QString());

    // Releases the reservation of the current file of a song, when it was moved, so other songs can use its filename.
    void Release(const QString &source_filename);

   private:
    // The filenames are compared case-insensitively and in the same Unicode normalization form, since they are the same files on filesystems which don't distinguish them.
    static QString Key(const QString &filename);
    bool IsAvailable(const QString &filename, const QString &key, const QString &source_filename, const QString &source_key) const;
    QString RootRelativeKey(const QString &source_filename) const;

    QString root_;
    QString root_key_;
    QSet<QString> used_;
    QHash<QString, int> reserved_;
    QHash<QString, int> next_number_;
  };

 Q_SIGNALS:
  void Finished(const QStringList &files_with_errors, const QStringList &log, const bool success);
  void FileCopied(const int database_id);
  void SongPathChanged(const Song &song, const QFileInfo &new_file, const std::optional<int> new_collection_directory_id);

 protected:
  void timerEvent(QTimerEvent *e) override;

 private Q_SLOTS:
  void ProcessSomeFiles();
  void FileTranscoded(const QString &input, const QString &output, const bool success);
  void LogLine(const QString &message);

 private:
  void SetSongProgress(const float progress, const bool transcoded = false);
  void UpdateProgress();
  Song::FileType CheckTranscode(const Song::FileType original_type) const;
  bool ShouldSkipFile(const QString &filename) const;

 private:
  struct Task {
    explicit Task(const NewSongInfo &song_info = NewSongInfo())
        : song_info_(song_info),
          transcode_progress_(0.0),
          new_filetype_(Song::FileType::Unknown) {}

    NewSongInfo song_info_;
    float transcode_progress_;
    QString transcoded_filename_;
    QString new_extension_;
    Song::FileType new_filetype_;
  };

  QThread *thread_;
  QThread *original_thread_;
  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  Transcoder *transcoder_;
  QTimer *process_files_timer_;
  const SharedPtr<MusicStorage> destination_;
  QList<Song::FileType> supported_filetypes_;
  // The destination filenames used so far, the extension of transcoded files is only known when they are copied.
  UniqueFilenames unique_filenames_;

  const OrganizeFormat format_;
  const bool copy_;
  const bool overwrite_;
  const bool albumcover_;
  const bool eject_after_;
  quint64 task_count_;
  const QString playlist_;

  QBasicTimer transcode_progress_timer_;
  QList<Task> tasks_pending_;
  QMap<QString, Task> tasks_transcoding_;
  int tasks_complete_;

  bool started_;

  int task_id_;
  int current_copy_progress_;
  bool finished_;

  QStringList files_with_errors_;
  QStringList log_;
};

#endif  // ORGANISE_H
