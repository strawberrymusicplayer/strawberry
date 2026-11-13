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

#include "config.h"

#include <QtGlobal>

#include <algorithm>
#include <memory>

#include <glib.h>
#include <glib/gtypes.h>
#include <gst/gst.h>

#include <QThread>
#include <QCoreApplication>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "includes/scoped_ptr.h"
#include "core/logging.h"
#include "core/standardpaths.h"
#include "core/signalchecker.h"
#include "core/settings.h"
#include "constants/transcodersettings.h"
#include "engine/gstpadutils.h"
#include "transcoder.h"

using std::make_shared;
using std::make_unique;
using namespace Qt::Literals::StringLiterals;

int Transcoder::JobFinishedEvent::sEventType = -1;

TranscoderPreset::TranscoderPreset(const Song::FileType filetype, const QString &name, const QString &extension, const QString &codec_mimetype, const QString &muxer_mimetype)
    : filetype_(filetype),
      name_(name),
      extension_(extension),
      codec_mimetype_(codec_mimetype),
      muxer_mimetype_(muxer_mimetype) {}


GstElement *Transcoder::CreateElement(const QString &factory_name, GstElement *bin, const QString &name) {

  GstElement *ret = gst_element_factory_make(factory_name.toLatin1().constData(), name.isNull() ? factory_name.toLatin1().constData() : name.toLatin1().constData());

  if (ret && bin) gst_bin_add(GST_BIN(bin), ret);

  if (ret) {
    SetElementProperties(factory_name, G_OBJECT(ret));
  }
  else {
    Q_EMIT LogLine(tr("Could not create the GStreamer element \"%1\" - make sure you have all the required GStreamer plugins installed").arg(factory_name));
  }

  return ret;

}

struct SuitableElement {

  explicit SuitableElement(const QString &name = QString(), int rank = 0) : name_(name), rank_(rank) {}

  bool operator<(const SuitableElement &other) const {
    return rank_ < other.rank_;
  }

  QString name_;
  int rank_;

};

QString Transcoder::BestElementForMimeType(const GstElementFactoryListType element_type, const QString &mime_type, int *best_rank) {

  if (mime_type.isEmpty()) return QString();

  // Keep track of all the suitable elements we find and figure out which is the best at the end.
  QList<SuitableElement> suitable_elements_;

  // The caps we're trying to find
  GstCaps *target_caps = gst_caps_from_string(mime_type.toUtf8().constData());

  GstRegistry *registry = gst_registry_get();
  GList *const features = gst_registry_get_feature_list(registry, GST_TYPE_ELEMENT_FACTORY);

  for (GList *f = features; f; f = g_list_next(f)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(f->data);

    // Is this the right type of plugin?
    if (gst_element_factory_list_is_type(factory, element_type)) {
      // check if the element factory supports the target caps
      if (gst_element_factory_can_src_any_caps(factory, target_caps)) {
        const QString name = QString::fromUtf8(GST_OBJECT_NAME(factory));
        int rank = static_cast<int>(gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(factory)));
        if (name.startsWith("avmux"_L1) || name.startsWith("avenc"_L1)) {
          rank = -1;  // ffmpeg usually sucks
        }
        suitable_elements_ << SuitableElement(name, rank);
      }
    }
  }

  gst_plugin_feature_list_free(features);
  gst_caps_unref(target_caps);

  if (suitable_elements_.isEmpty()) return QString();

  // Sort by rank
  std::sort(suitable_elements_.begin(), suitable_elements_.end());
  const SuitableElement &best = suitable_elements_.last();

  if (best_rank) *best_rank = best.rank_;

  return best.name_;

}

GstElement *Transcoder::CreateElementForMimeType(GstElementFactoryListType element_type, const QString &mime_type, GstElement *bin) {

  if (mime_type.isEmpty()) return nullptr;

  // HACK: Force mp4mux because it doesn't set any useful src caps
  if (mime_type == "audio/mp4"_L1) {
    Q_EMIT LogLine(QStringLiteral("Using '%1' (rank %2)").arg("mp4mux"_L1).arg(-1));
    return CreateElement(u"mp4mux"_s, bin);
  }

  int best_rank = 0;
  const QString best_name = BestElementForMimeType(element_type, mime_type, &best_rank);
  if (best_name.isEmpty()) return nullptr;

  Q_EMIT LogLine(QStringLiteral("Using '%1' (rank %2)").arg(best_name).arg(best_rank));

  if (best_name == "lamemp3enc"_L1) {
    // Special case: we need to add xingmux and id3v2mux to the pipeline when using lamemp3enc because it doesn't write the VBR or ID3v2 headers itself.

    Q_EMIT LogLine(u"Adding xingmux and id3v2mux to the pipeline"_s);

    // Create the bin
    GstElement *mp3bin = gst_bin_new("mp3bin");
    gst_bin_add(GST_BIN(bin), mp3bin);

    // Create the elements
    GstElement *lame = CreateElement(u"lamemp3enc"_s, mp3bin);
    GstElement *xing = CreateElement(u"xingmux"_s, mp3bin);
    GstElement *id3v2 = CreateElement(u"id3v2mux"_s, mp3bin);

    if (!lame || !xing || !id3v2) {
      return nullptr;
    }

    // Link the elements together
    if (!gst_element_link_many(lame, xing, id3v2, nullptr)) {
      qLog(Error) << "Failed to link lamemp3enc, xingmux and id3v2mux elements.";
      return nullptr;
    }

    // Link the bin's ghost pads to the elements on each end
    GstPad *pad = gst_element_get_static_pad(lame, "sink");
    gst_element_add_pad(mp3bin, gst_ghost_pad_new("sink", pad));
    gst_object_unref(GST_OBJECT(pad));

    pad = gst_element_get_static_pad(id3v2, "src");
    gst_element_add_pad(mp3bin, gst_ghost_pad_new("src", pad));
    gst_object_unref(GST_OBJECT(pad));

    return mp3bin;
  }
  else {
    return CreateElement(best_name, bin);
  }

}

Transcoder::JobFinishedEvent::JobFinishedEvent(const quint64 job_id, const bool success)
    : QEvent(static_cast<QEvent::Type>(sEventType)), job_id_(job_id), success_(success) {}

void Transcoder::JobState::PostFinished(const bool success) {

  // Only post the first result, a pipeline can post more than one error message, or an error message and end of stream.
  Result expected_result = Result::None;
  if (!result_.compare_exchange_strong(expected_result, success ? Result::Success : Result::Failure)) return;

  if (success) {
    Q_EMIT parent_->LogLine(tr("Successfully written %1").arg(QDir::toNativeSeparators(job_.output)));
  }

  QCoreApplication::postEvent(parent_, new Transcoder::JobFinishedEvent(id_, success));

}

Transcoder::Transcoder(QObject *parent, const QString &settings_postfix)
    : QObject(parent),
      max_threads_(QThread::idealThreadCount()),
      next_job_id_(0),
      settings_postfix_(settings_postfix) {

  if (JobFinishedEvent::sEventType == -1)
    JobFinishedEvent::sEventType = QEvent::registerEventType();

  // Initialize some settings for the lamemp3enc element.
  Settings s;
  s.beginGroup("Transcoder/lamemp3enc"_L1 + settings_postfix_);

  if (s.value(TranscoderSettings::LameMP3Settings::kTarget).isNull()) {
    s.setValue(TranscoderSettings::LameMP3Settings::kTarget, 1);  // 1 == bitrate
  }
  if (s.value(TranscoderSettings::LameMP3Settings::kCbr).isNull()) {
    s.setValue(TranscoderSettings::LameMP3Settings::kCbr, false);
  }

  s.endGroup();

  // faac uses VBR by default, which ignores the bitrate, so the rate control the AAC options show by default is saved.
  // The rate control wasn't saved before, so a bitrate saved before is used with ABR too.
  // The bitrate is not saved here, since the other AAC encoders use the saved bitrate, they keep their own default until the bitrate is saved in the AAC options.
  s.beginGroup("Transcoder/faac"_L1 + settings_postfix_);

  if (s.value(TranscoderSettings::FaacSettings::kRateControl).isNull()) {
    s.setValue(TranscoderSettings::FaacSettings::kRateControl, TranscoderSettings::FaacSettings::kRateControlABR);
  }

  s.endGroup();

}

Transcoder::~Transcoder() {

  // Stop the running jobs, so their partial output files are removed.
  Cancel();

}

QList<TranscoderPreset> Transcoder::GetAllPresets() {

  QList<TranscoderPreset> ret;
  ret << PresetForFileType(Song::FileType::WAV);
  ret << PresetForFileType(Song::FileType::FLAC);
  ret << PresetForFileType(Song::FileType::WavPack);
  ret << PresetForFileType(Song::FileType::OggFlac);
  ret << PresetForFileType(Song::FileType::OggVorbis);
  ret << PresetForFileType(Song::FileType::OggOpus);
  ret << PresetForFileType(Song::FileType::OggSpeex);
  ret << PresetForFileType(Song::FileType::MPEG);
  ret << PresetForFileType(Song::FileType::MP4);
  ret << PresetForFileType(Song::FileType::ASF);
  ret << PresetForFileType(Song::FileType::ALAC);

  return ret;

}

TranscoderPreset Transcoder::PresetForFileType(const Song::FileType filetype) {

  switch (filetype) {
    case Song::FileType::WAV:
      return TranscoderPreset(filetype, u"Wav"_s,                    u"wav"_s,  QString(), u"audio/x-wav"_s);
    case Song::FileType::FLAC:
      return TranscoderPreset(filetype, u"FLAC"_s,                   u"flac"_s, u"audio/x-flac"_s);
    case Song::FileType::WavPack:
      return TranscoderPreset(filetype, u"WavPack"_s,                u"wv"_s,   u"audio/x-wavpack"_s);
    case Song::FileType::OggFlac:
      return TranscoderPreset(filetype, u"Ogg FLAC"_s,               u"ogg"_s,  u"audio/x-flac"_s, u"application/ogg"_s);
    case Song::FileType::OggVorbis:
      return TranscoderPreset(filetype, u"Ogg Vorbis"_s,             u"ogg"_s,  u"audio/x-vorbis"_s, u"application/ogg"_s);
    case Song::FileType::OggOpus:
      return TranscoderPreset(filetype, u"Ogg Opus"_s,               u"opus"_s, u"audio/x-opus"_s, u"application/ogg"_s);
    case Song::FileType::OggSpeex:
      return TranscoderPreset(filetype, u"Ogg Speex"_s,              u"spx"_s,  u"audio/x-speex"_s, u"application/ogg"_s);
    case Song::FileType::MPEG:
      return TranscoderPreset(filetype, u"MP3"_s,                    u"mp3"_s,  u"audio/mpeg, mpegversion=(int)1, layer=(int)3"_s);
    case Song::FileType::MP4:
      return TranscoderPreset(filetype, u"M4A AAC"_s,                u"mp4"_s,  u"audio/mpeg, mpegversion=(int)4"_s, u"audio/mp4"_s);
    case Song::FileType::ASF:
      return TranscoderPreset(filetype, u"Windows Media audio"_s,    u"wma"_s,  u"audio/x-wma, wmaversion=(int)2"_s, u"video/x-ms-asf"_s);
    case Song::FileType::ALAC:
      return TranscoderPreset(filetype, u"ALAC"_s,                   u"m4a"_s,  u"audio/x-alac"_s, u"audio/mp4"_s);
    default:
      qLog(Warning) << "Unsupported format in PresetForFileType:" << static_cast<int>(filetype);
      return TranscoderPreset();
  }

}

Song::FileType Transcoder::PickBestFormat(const QList<Song::FileType> &supported) {

  if (supported.isEmpty()) return Song::FileType::Unknown;

  const QList<Song::FileType> best_formats = QList<Song::FileType>() << Song::FileType::FLAC << Song::FileType::OggFlac << Song::FileType::WavPack;
  for (Song::FileType type : best_formats) {
    if (supported.contains(type)) return type;
  }

  return supported[0];

}

QString Transcoder::GetFile(const QString &input, const TranscoderPreset &preset) {

  const QString cache_directory = CacheDirectory();
  if (cache_directory.isEmpty()) return QString();

  const QFileInfo fileinfo_input(input);
  QFileInfo fileinfo_output(cache_directory + QLatin1Char('/') + fileinfo_input.completeBaseName() + QLatin1Char('.') + preset.extension_);

  // Never overwrite existing files, or the output files of other jobs, which might not be written yet.
  // The directory is only resolved once, since all the filenames which are tried are in the same directory.
  const QString output_directory = OutputDirectory(fileinfo_output.filePath());
  // Symbolic links are never used, also dangling links, since they could point to the output file of another job.
  const auto is_used = [this, &output_directory](const QFileInfo &fileinfo) { return fileinfo.exists() || fileinfo.isSymLink() || used_outputs_.contains(OutputKey(output_directory, fileinfo.fileName())); };
  if (is_used(fileinfo_output)) {
    QString path = fileinfo_output.path();
    QString filename = fileinfo_output.completeBaseName();
    QString suffix = fileinfo_output.suffix();
    const QString original_key = OutputKey(output_directory, fileinfo_output.fileName());
    int i = next_output_numbers_.value(original_key, 0);
    for (; i < 1000000; ++i) {
      QString new_filename = QStringLiteral("%1/%2-%3.%4").arg(path, filename, QString::number(i), suffix);
      fileinfo_output.setFile(new_filename);
      if (!is_used(fileinfo_output)) {
        break;
      }

    }
    next_output_numbers_.insert(original_key, i + 1);
  }

  return fileinfo_output.filePath();

}

QString Transcoder::CacheDirectory() {

  if (!cache_dir_ || !cache_dir_->isValid()) {
    const QString cache_path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + "/transcoder"_L1;
    if (!QDir(cache_path).exists()) QDir().mkpath(cache_path);
    // Each transcoder uses its own directory, since the jobs are only reserved per transcoder, so the jobs of other transcoders, like when organizing to several devices at once, never use the same output files.
    // The shared directory is never used instead, since the jobs of other transcoders could use the same output files there.
    cache_dir_ = make_unique<QTemporaryDir>(cache_path + "/XXXXXX"_L1);
    if (!cache_dir_->isValid()) {
      qLog(Error) << "Failed to create the transcoder cache directory in" << cache_path << cache_dir_->errorString();
      return QString();
    }
  }

  return cache_dir_->path();

}

QString Transcoder::OutputDirectory(const QString &output) {

  // The directory is resolved, since the same output can be written differently, as a relative path, or through symbolic links.
  // The directory is made absolute without QFileInfo::absolutePath(), since it removes the ".." components before the symbolic links are resolved.
  QString path = QFileInfo(output).path();
  if (QDir::isRelativePath(path)) {
    path = QDir::currentPath() + QLatin1Char('/') + path;
  }

  return ResolvedDirectory(path);

}

QString Transcoder::OutputKey(const QString &output) {

  return OutputKey(OutputDirectory(output), QFileInfo(output).fileName());

}

QString Transcoder::OutputKey(const QString &output_directory, const QString &filename) {

  // The paths are normalized and compared case-insensitively, since the output can be on a normalization-insensitive or case-insensitive filesystem.
  return QString(output_directory + QLatin1Char('/') + filename).normalized(QString::NormalizationForm_C).toCaseFolded();

}

QString Transcoder::ResolvedDirectory(const QString &path) {

  // The directory is resolved, since the same directory can be written with redundant "." or ".." components, or through symbolic links.
  // The components are resolved one by one, since ".." after a symbolic link is the parent of the target of the link, and a directory which doesn't exist yet can be followed by ".." and symbolic links.
  // The directories which don't exist are kept as they are, so the result is the same when they are created.
  const QString directory = QDir::fromNativeSeparators(path);

  QString resolved;
  qsizetype start = 0;
#ifdef Q_OS_WIN
  if (directory.startsWith("//"_L1)) {
    // Network paths are only resolved as a whole.
    // On the other platforms, paths starting with "//" are local paths.
    const QString canonical_path = QFileInfo(path).canonicalFilePath();
    return canonical_path.isEmpty() ? QDir::cleanPath(path) : canonical_path;
  }
#endif
  if (directory.startsWith(QLatin1Char('/'))) {
    resolved = u"/"_s;
    start = 1;
  }
  else if (directory.length() >= 2 && directory.at(1) == QLatin1Char(':')) {
    resolved = directory.left(2) + QLatin1Char('/');
    start = 2;
  }
  else {
    // Relative paths are only cleaned, OutputDirectory() makes the paths absolute.
    return QDir::cleanPath(path);
  }

  const auto append = [](const QString &parent, const QString &component) { return parent.endsWith(QLatin1Char('/')) ? QString(parent + component) : QString(parent + QLatin1Char('/') + component); };

  bool exists = true;
  const QStringList components = directory.mid(start).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &component : components) {
    if (component == "."_L1) continue;
    if (component == ".."_L1) {
      // The resolved path doesn't contain symbolic links, so ".." can be removed.
      resolved = QDir::cleanPath(append(resolved, component));
      // When ".." removed a directory which doesn't exist, the parent can exist, then the following symbolic links are resolved again.
      if (!exists) exists = QFileInfo::exists(resolved);
      continue;
    }
    const QString component_path = append(resolved, component);
    if (exists) {
      const QString canonical_path = QFileInfo(component_path).canonicalFilePath();
      if (!canonical_path.isEmpty()) {
        resolved = canonical_path;
        continue;
      }
      exists = false;
    }
    resolved = component_path;
  }

  return resolved;

}

bool Transcoder::OutputUsedByJob(const QString &output) const {

  return used_outputs_.contains(OutputKey(output));

}

void Transcoder::ReserveOutput(const QString &output_key) {

  ++used_outputs_[output_key];

}

void Transcoder::ReleaseOutput(const QString &output_key) {

  QHash<QString, int>::iterator it = used_outputs_.find(output_key);
  if (it == used_outputs_.end()) return;
  if (--it.value() <= 0) {
    used_outputs_.erase(it);
  }

  // Start with the first number again when there are no more jobs, since the output files might have been removed.
  if (used_outputs_.isEmpty()) {
    next_output_numbers_.clear();
  }

}

void Transcoder::AddJob(const QString &input, const TranscoderPreset &preset, const QString &output) {

  Job job;
  job.input = input;
  job.preset = preset;
  job.output = output;
  job.output_key = OutputKey(output);
  queued_jobs_ << job;
  ReserveOutput(job.output_key);

}

void Transcoder::Start() {

  Q_EMIT LogLine(tr("Transcoding %1 files using %2 threads").arg(queued_jobs_.count()).arg(max_threads()));

  Q_FOREVER {
    StartJobStatus status = MaybeStartNextJob();
    if (status == StartJobStatus::AllThreadsBusy || status == StartJobStatus::NoMoreJobs) break;
  }

}

Transcoder::StartJobStatus Transcoder::MaybeStartNextJob() {

  if (current_jobs_.count() >= max_threads()) return StartJobStatus::AllThreadsBusy;
  if (queued_jobs_.isEmpty()) {
    if (current_jobs_.isEmpty()) {
      Q_EMIT AllJobsComplete();
    }

    return StartJobStatus::NoMoreJobs;
  }

  Job job = queued_jobs_.takeFirst();
  if (StartJob(job)) {
    return StartJobStatus::StartedSuccessfully;
  }

  ReleaseOutput(job.output_key);
  Q_EMIT JobComplete(job.input, job.output, false);
  return StartJobStatus::FailedToStart;

}

void Transcoder::NewPadCallback(GstElement *element, GstPad *pad, gpointer data) {

  Q_UNUSED(element)

  JobState *state = reinterpret_cast<JobState*>(data);

  // Only link the audio, not pads for video or subtitles, the audio converter can't take them.
  if (!GstPadIsAudio(pad)) return;

  GstPad *const audiopad = gst_element_get_static_pad(state->convert_element_, "sink");
  if (!audiopad) return;

  if (GST_PAD_IS_LINKED(audiopad)) {
    qLog(Debug) << "Audiopad is already linked, unlinking old pad";
    gst_pad_unlink(audiopad, GST_PAD_PEER(audiopad));
  }

  gst_pad_link(pad, audiopad);
  gst_object_unref(audiopad);

}

GstBusSyncReply Transcoder::BusCallbackSync(GstBus *bus, GstMessage *msg, gpointer data) {

  Q_UNUSED(bus)

  JobState *state = reinterpret_cast<JobState*>(data);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
      state->PostFinished(true);
      break;

    case GST_MESSAGE_ERROR:
      state->ReportError(msg);
      state->PostFinished(false);
      break;

    default:
      break;
  }

  return GST_BUS_PASS;

}

void Transcoder::JobState::ReportError(GstMessage *msg) const {

  GError *error = nullptr;
  gchar *debugs = nullptr;

  gst_message_parse_error(msg, &error, &debugs);
  QString message;
  if (error) {
    message = QString::fromLocal8Bit(error->message);
    g_error_free(error);
  }
  g_free(debugs);

  Q_EMIT parent_->LogLine(tr("Error processing %1: %2").arg(QDir::toNativeSeparators(job_.input), message));

}

bool Transcoder::StartJob(const Job &job) {

  SharedPtr<JobState> state = make_shared<JobState>(++next_job_id_, job, this);
  state->output_existed_ = QFileInfo::exists(job.output);

  Q_EMIT LogLine(tr("Starting %1").arg(QDir::toNativeSeparators(job.input)));

  // Create the pipeline.
  // This should be a scoped_ptr, but scoped_ptr doesn't support custom destructors.
  state->pipeline_ = gst_pipeline_new("pipeline");
  if (!state->pipeline_) return false;

  // Create all the elements
  GstElement *src      = CreateElement(u"filesrc"_s, state->pipeline_);
  GstElement *decode   = CreateElement(u"decodebin"_s, state->pipeline_);
  GstElement *convert  = CreateElement(u"audioconvert"_s, state->pipeline_);
  GstElement *resample = CreateElement(u"audioresample"_s, state->pipeline_);
  GstElement *codec    = CreateElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, job.preset.codec_mimetype_, state->pipeline_);
  GstElement *muxer    = CreateElementForMimeType(GST_ELEMENT_FACTORY_TYPE_MUXER, job.preset.muxer_mimetype_, state->pipeline_);
  GstElement *sink     = CreateElement(u"filesink"_s, state->pipeline_);

  if (!src || !decode || !convert || !sink) return false;

  if (!codec && !job.preset.codec_mimetype_.isEmpty()) {
    Q_EMIT LogLine(tr("Couldn't find an encoder for %1, check you have the correct GStreamer plugins installed").arg(job.preset.codec_mimetype_));
    return false;
  }

  if (!muxer && !job.preset.muxer_mimetype_.isEmpty()) {
    Q_EMIT LogLine(tr("Couldn't find a muxer for %1, check you have the correct GStreamer plugins installed").arg(job.preset.muxer_mimetype_));
    return false;
  }

  // Join them together
  if (!gst_element_link(src, decode)) {
    qLog(Error) << "Failed to link filesrc to decodebin.";
    return false;
  }
  bool linked = true;
  if (codec && muxer) linked = gst_element_link_many(convert, resample, codec, muxer, sink, nullptr);
  else if (codec) linked = gst_element_link_many(convert, resample, codec, sink, nullptr);
  else if (muxer) linked = gst_element_link_many(convert, resample, muxer, sink, nullptr);
  if (!linked) {
    qLog(Error) << "Failed to link transcoder pipeline elements.";
    return false;
  }

  // Set properties
  g_object_set(src, "location", job.input.toUtf8().constData(), nullptr);
  g_object_set(sink, "location", job.output.toUtf8().constData(), nullptr);

  // Set callbacks
  state->convert_element_ = convert;

  CHECKED_GCONNECT(decode, "pad-added", &NewPadCallback, state.get());
  {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(state->pipeline_));
    if (bus) {
      gst_bus_set_sync_handler(bus, BusCallbackSync, state.get(), nullptr);
      gst_object_unref(bus);
    }
  }

  // Start the pipeline
  gst_element_set_state(state->pipeline_, GST_STATE_PLAYING);

  // GStreamer now transcodes in another thread, so we can return now and do something else.
  // Keep the JobState object around.  It'll post an event to our event loop when it finishes.
  current_jobs_ << state;

  return true;

}

Transcoder::JobState::~JobState() {

  if (pipeline_) {
    // Wait for the pipeline to stop, so the output file is closed before it's removed when the job failed.
    if (gst_element_set_state(pipeline_, GST_STATE_NULL) == GST_STATE_CHANGE_ASYNC) {
      gst_element_get_state(pipeline_, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    }
    gst_object_unref(pipeline_);
  }

}

bool Transcoder::event(QEvent *e) {

  if (e->type() == JobFinishedEvent::sEventType) {
    JobFinishedEvent *finished_event = static_cast<JobFinishedEvent*>(e);

    // Find this job in the list
    JobStateList::const_iterator it = current_jobs_.cbegin();
    for (; it != current_jobs_.cend(); ++it) {
      if ((*it)->id_ == finished_event->job_id_) break;
    }
    if (it == current_jobs_.cend()) {
      // Couldn't find it, maybe GStreamer gave us an event after we'd destroyed the pipeline?
      return true;
    }

    QString input = (*it)->job_.input;
    QString output = (*it)->job_.output;
    const QString output_key = (*it)->job_.output_key;
    const bool output_existed = (*it)->output_existed_;

    // Remove event handlers from the gstreamer pipeline, so they don't get called after the pipeline is shutting down
    {
      GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE((*it)->pipeline_));
      if (bus) {
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
      }
    }

    // Remove it from the list - this will also destroy the GStreamer pipeline
    current_jobs_.erase(it);
    ReleaseOutput(output_key);

    if (!finished_event->success_) {
      RemovePartialOutput(output, output_existed);
    }

    // Emit the finished signal
    Q_EMIT JobComplete(input, output, finished_event->success_);

    // Start some more jobs
    MaybeStartNextJob();

    return true;
  }

  return QObject::event(e);

}

void Transcoder::Cancel() {

  // Remove all pending jobs
  for (const Job &job : std::as_const(queued_jobs_)) {
    ReleaseOutput(job.output_key);
  }
  queued_jobs_.clear();

  // Stop the running ones
  JobStateList::iterator it = current_jobs_.begin();
  while (it != current_jobs_.end()) {
    SharedPtr<JobState> state(*it);

    // Remove event handlers from the gstreamer pipeline, so they don't get called after the pipeline is shutting down
    {
      GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(state->pipeline_));
      if (bus) {
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
      }
    }

    // Stop the pipeline
    if (gst_element_set_state(state->pipeline_, GST_STATE_NULL) == GST_STATE_CHANGE_ASYNC) {
      // Wait for it to finish stopping...
      gst_element_get_state(state->pipeline_, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    }

    // Remove the job, this destroys the GStreamer pipeline too
    it = current_jobs_.erase(it);
    ReleaseOutput(state->job_.output_key);

    // The job might have finished successfully before it was stopped, then the finished event is ignored, but the output is complete.
    if (state->result_ != JobState::Result::Success) {
      RemovePartialOutput(state->job_.output, state->output_existed_);
    }
  }

}

void Transcoder::RemovePartialOutput(const QString &output, const bool output_existed) {

  // An output file which existed before might not have been written, when the job failed before the file was opened.
  if (output_existed || output.isEmpty() || !QFileInfo::exists(output)) return;

  // The job doesn't create symbolic links, so a symbolic link existed before, like a dangling symbolic link to the output file.
  if (QFileInfo(output).isSymLink()) return;

  if (QFile::remove(output)) {
    qLog(Debug) << "Removed partial output file" << output;
  }
  else {
    qLog(Error) << "Failed to remove partial output file" << output;
  }

}

QMap<QString, float> Transcoder::GetProgress() const {

  QMap<QString, float> ret;

  for (const auto &state : current_jobs_) {
    if (!state->pipeline_) continue;

    gint64 position = 0;
    gint64 duration = 0;

    gst_element_query_position(state->pipeline_, GST_FORMAT_TIME, &position);
    gst_element_query_duration(state->pipeline_, GST_FORMAT_TIME, &duration);

    ret[state->job_.input] = duration > 0 ? static_cast<float>(position) / static_cast<float>(duration) : 0.0F;
  }

  return ret;

}

bool Transcoder::IsAACEncoder(GObject *object) {

  if (!GST_IS_ELEMENT(object)) return false;

  GstElementFactory *factory = gst_element_get_factory(GST_ELEMENT(object));
  if (!factory || !gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER)) return false;

  GstCaps *aac_caps = gst_caps_from_string(PresetForFileType(Song::FileType::MP4).codec_mimetype_.toUtf8().constData());
  const bool is_aac_encoder = gst_element_factory_can_src_any_caps(factory, aac_caps);
  gst_caps_unref(aac_caps);

  return is_aac_encoder;

}

void Transcoder::SetElementProperties(const QString &name, GObject *object) {

  // The AAC options are saved for faac, the other AAC encoders, like fdkaacenc, mfaacenc on Windows and atenc on macOS, only have the bitrate in common with it, the other options don't exist or have other meanings.
  if (name != "faac"_L1 && IsAACEncoder(object)) {
    SetElementPropertiesFromSettings(name, u"faac"_s, object, QStringList() << u"bitrate"_s);
  }

  // faac uses the bitrate the AAC options show by default, a saved bitrate replaces it below.
  if (name == "faac"_L1 && g_object_class_find_property(G_OBJECT_GET_CLASS(object), "bitrate")) {
    g_object_set(object, "bitrate", static_cast<gint>(TranscoderSettings::FaacSettings::kDefaultBitrate), nullptr);
  }

  SetElementPropertiesFromSettings(name, name, object);

}

void Transcoder::SetElementPropertiesFromSettings(const QString &name, const QString &settings_name, GObject *object, const QStringList &property_names) {

  Settings s;
  s.beginGroup("Transcoder/"_L1 + settings_name + settings_postfix_);

  guint properties_count = 0;
  GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(object), &properties_count);

  for (uint i = 0; i < properties_count; ++i) {
    GParamSpec *property = properties[i];

    if (!property_names.isEmpty() && !property_names.contains(QString::fromUtf8(property->name))) {
      continue;
    }

    if (!s.contains(QString::fromUtf8(property->name))) {
      continue;
    }

    const QVariant value = s.value(QString::fromUtf8(property->name));
    if (value.isNull()) {
      continue;
    }

    Q_EMIT LogLine(QStringLiteral("Setting %1 property: %2 = %3").arg(name, QString::fromUtf8(property->name), value.toString()));

    switch (property->value_type) {
      case G_TYPE_FLOAT:{
        const double g_value = static_cast<gfloat>(value.toFloat());
        qLog(Debug) << "Setting" << property->name << "float" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_DOUBLE:{
        const double g_value = static_cast<gdouble>(value.toDouble());
        qLog(Debug) << "Setting" << property->name << "(double)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_BOOLEAN:{
        const bool g_value = value.toBool();
        qLog(Debug) << "Setting" << property->name << "(bool)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_INT:
      case G_TYPE_ENUM:{
        const gint g_value = static_cast<gint>(value.toInt());
        qLog(Debug) << "Setting" << property->name << "(enum)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_UINT:{
        const guint g_value = static_cast<guint>(value.toUInt());
        qLog(Debug) << "Setting" << property->name << "(uint)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      // g_object_set() reads the value as the type of the property, and long is only 32 bits on Windows, so 64-bit properties need 64-bit values.
      case G_TYPE_LONG:{
        const glong g_value = static_cast<glong>(value.toLongLong());
        qLog(Debug) << "Setting" << property->name << "(long)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_INT64:{
        const gint64 g_value = static_cast<gint64>(value.toLongLong());
        qLog(Debug) << "Setting" << property->name << "(int64)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_ULONG:{
        const gulong g_value = static_cast<gulong>(value.toULongLong());
        qLog(Debug) << "Setting" << property->name << "(ulong)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      case G_TYPE_UINT64:{
        const guint64 g_value = static_cast<guint64>(value.toULongLong());
        qLog(Debug) << "Setting" << property->name << "(uint64)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
      default:{
        const gint g_value = static_cast<gint>(value.toInt());
        qLog(Debug) << "Setting" << property->name << "(int)" << "to" << g_value;
        g_object_set(object, property->name, g_value, nullptr);
        break;
      }
    }
  }

  g_free(properties);

  s.endGroup();

}
