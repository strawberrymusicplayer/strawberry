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

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include "config.h"

#include <atomic>

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <QObject>
#include <QList>
#include <QSet>
#include <QHash>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QEvent>

#include "includes/shared_ptr.h"
#include "includes/scoped_ptr.h"
#include "core/song.h"

struct TranscoderPreset {
  explicit TranscoderPreset() : filetype_(Song::FileType::Unknown) {}
  TranscoderPreset(const Song::FileType filetype, const QString &name, const QString &extension, const QString &codec_mimetype, const QString &muxer_mimetype_ = QString());

  Song::FileType filetype_;
  QString name_;
  QString extension_;
  QString codec_mimetype_;
  QString muxer_mimetype_;
};
Q_DECLARE_METATYPE(TranscoderPreset)

class QTemporaryDir;

class Transcoder : public QObject {
  Q_OBJECT

  friend class TranscoderTest;

 public:
  explicit Transcoder(QObject *parent = nullptr, const QString &settings_postfix = QLatin1String(""));
  ~Transcoder() override;

  static TranscoderPreset PresetForFileType(const Song::FileType filetype);
  static QList<TranscoderPreset> GetAllPresets();
  static Song::FileType PickBestFormat(const QList<Song::FileType> &supported);

  int max_threads() const { return max_threads_; }
  void set_max_threads(int count) { max_threads_ = count; }

  // Returns an output filename in the cache directory of this transcoder, which is not an existing file, or the output of a queued or running job.
  // Only this transcoder uses the directory, so the outputs of other transcoders are never used either.
  // Returns an empty filename when the directory couldn't be created.
  QString GetFile(const QString &input, const TranscoderPreset &preset);
  void AddJob(const QString &input, const TranscoderPreset &preset, const QString &output);

  // Returns the absolute directory with the symbolic links resolved, and the redundant "." and ".." components removed, also when it doesn't exist yet.
  static QString ResolvedDirectory(const QString &path);
  // Returns the absolute resolved directory of the output.
  static QString OutputDirectory(const QString &output);
  // Returns the key for comparing outputs, the same output written differently gets the same key.
  static QString OutputKey(const QString &output);
  // Returns the key for comparing outputs for a filename in a directory from OutputDirectory().
  static QString OutputKey(const QString &output_directory, const QString &filename);

  QMap<QString, float> GetProgress() const;
  qint64 QueuedJobsCount() const { return queued_jobs_.count(); }

 public Q_SLOTS:
  void Start();
  void Cancel();

 Q_SIGNALS:
  void JobComplete(const QString &input, const QString &output, const bool success);
  void LogLine(const QString &message);
  void AllJobsComplete();

 protected:
  bool event(QEvent *e) override;

 private:
  // The description of a file to transcode - lives in the main thread.
  struct Job {
    QString input;
    QString output;
    // The key from OutputKey() when the job was added, the same key is released, also if the resolved output directory changed.
    QString output_key;
    TranscoderPreset preset;
  };

  // State held by a job and shared across gstreamer callbacks - lives in the job's thread.
  struct JobState {
    enum class Result {
      None,
      Success,
      Failure,
    };

    explicit JobState(const quint64 id, const Job &job, Transcoder *parent)
        : id_(id),
          job_(job),
          parent_(parent),
          pipeline_(nullptr),
          convert_element_(nullptr),
          output_existed_(false),
          result_(Result::None) {}
    ~JobState();

    void PostFinished(const bool success);
    void ReportError(GstMessage *msg) const;

    const quint64 id_;
    Job job_;
    Transcoder *parent_;
    GstElement *pipeline_;
    GstElement *convert_element_;
    // The output file existed before the job started, then it's not removed when the job fails, since it might not have been written.
    bool output_existed_;
    // The result of the job, set when the finished event is posted, it's one atomic value, so the result is never seen as posted before the success is set.
    std::atomic<Result> result_;

   private:
    Q_DISABLE_COPY(JobState)
  };

  // Event passed from a GStreamer callback to the Transcoder when a job finishes.
  struct JobFinishedEvent : public QEvent {
    explicit JobFinishedEvent(const quint64 job_id, const bool success);

    static int sEventType;

    // The job is identified by its ID, not by a pointer to its state, since the state can be freed before the event is handled, and a new state can be allocated at the same address.
    quint64 job_id_;
    bool success_;

   private:
    Q_DISABLE_COPY(JobFinishedEvent)
  };

  enum class StartJobStatus {
    StartedSuccessfully,
    FailedToStart,
    NoMoreJobs,
    AllThreadsBusy,
  };

  StartJobStatus MaybeStartNextJob();
  bool StartJob(const Job &job);
  bool OutputUsedByJob(const QString &output) const;
  QString CacheDirectory();
  void ReserveOutput(const QString &output_key);
  void ReleaseOutput(const QString &output_key);
  // Removes the output file of a job which failed or was canceled, the pipeline must be stopped, so the file is closed.
  static void RemovePartialOutput(const QString &output, const bool output_existed);

  GstElement *CreateElement(const QString &factory_name, GstElement *bin = nullptr, const QString &name = QString());
  GstElement *CreateElementForMimeType(GstElementFactoryListType element_type, const QString &mime_type, GstElement *bin = nullptr);
  void SetElementProperties(const QString &name, GObject *object);

  static void NewPadCallback(GstElement *element, GstPad *pad, gpointer data);
  static GstBusSyncReply BusCallbackSync(GstBus *bus, GstMessage *msg, gpointer data);

 private:
  using JobStateList = QList<SharedPtr<JobState>>;

  int max_threads_;
  quint64 next_job_id_;
  // The directory for the output files when no output is given, it's declared before the jobs, so it's removed after the jobs are stopped.
  ScopedPtr<QTemporaryDir> cache_dir_;
  QList<Job> queued_jobs_;
  JobStateList current_jobs_;
  QString settings_postfix_;
  // The number of queued and running jobs for each output, by OutputKey().
  QHash<QString, int> used_outputs_;
  // The next number to try for each output, so all the numbers are not tried again for each file with the same name.
  QHash<QString, int> next_output_numbers_;
};

#endif  // TRANSCODER_H
