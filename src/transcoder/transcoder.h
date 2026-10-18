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
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QEvent>

#include "includes/shared_ptr.h"
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

class Transcoder : public QObject {
  Q_OBJECT

  friend class TranscoderTest;

 public:
  explicit Transcoder(QObject *parent = nullptr, const QString &settings_postfix = QLatin1String(""));

  static TranscoderPreset PresetForFileType(const Song::FileType filetype);
  static QList<TranscoderPreset> GetAllPresets();
  static Song::FileType PickBestFormat(const QList<Song::FileType> &supported);

  int max_threads() const { return max_threads_; }
  void set_max_threads(int count) { max_threads_ = count; }

  static QString GetFile(const QString &input, const TranscoderPreset &preset, const QString &output = QString());
  void AddJob(const QString &input, const TranscoderPreset &preset, const QString &output);

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
    TranscoderPreset preset;
  };

  // State held by a job and shared across gstreamer callbacks - lives in the job's thread.
  struct JobState {
    explicit JobState(const quint64 id, const Job &job, Transcoder *parent)
        : id_(id),
          job_(job),
          parent_(parent),
          pipeline_(nullptr),
          convert_element_(nullptr),
          finished_posted_(false) {}
    ~JobState();

    void PostFinished(const bool success);
    void ReportError(GstMessage *msg) const;

    const quint64 id_;
    Job job_;
    Transcoder *parent_;
    GstElement *pipeline_;
    GstElement *convert_element_;
    std::atomic<bool> finished_posted_;

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

  GstElement *CreateElement(const QString &factory_name, GstElement *bin = nullptr, const QString &name = QString());
  GstElement *CreateElementForMimeType(GstElementFactoryListType element_type, const QString &mime_type, GstElement *bin = nullptr);
  void SetElementProperties(const QString &name, GObject *object);

  static void NewPadCallback(GstElement *element, GstPad *pad, gpointer data);
  static GstBusSyncReply BusCallbackSync(GstBus *bus, GstMessage *msg, gpointer data);

 private:
  using JobStateList = QList<SharedPtr<JobState>>;

  int max_threads_;
  quint64 next_job_id_;
  QList<Job> queued_jobs_;
  JobStateList current_jobs_;
  QString settings_postfix_;
};

#endif  // TRANSCODER_H
