/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <gst/gst.h>

#include "gtest_include.h"

#include <QString>
#include <QFile>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QByteArray>
#include <QDir>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include "includes/shared_ptr.h"
#include "core/standardpaths.h"
#include "engine/gststartup.h"
#include "transcoder/transcoder.h"
#include "transcoder/transcodedialog.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

class TranscoderTest {
 public:
  // Adds a running job without starting a pipeline, it gets an empty pipeline, so it can be finished like a real job.
  static quint64 AddRunningJob(Transcoder &transcoder, const QString &input, const QString &output = QString(), const bool output_existed = false) {
    Transcoder::Job job;
    job.input = input;
    job.output = output.isEmpty() ? input + u".out"_s : output;
    job.output_key = Transcoder::OutputKey(job.output);
    SharedPtr<Transcoder::JobState> state = make_shared<Transcoder::JobState>(++transcoder.next_job_id_, job, &transcoder);
    // Like when the job is started, also when the test creates the output after adding the job.
    state->output_existed_ = output_existed || QFileInfo::exists(job.output);
    state->pipeline_ = gst_pipeline_new(nullptr);
    transcoder.current_jobs_ << state;
    transcoder.ReserveOutput(job.output_key);
    return state->id_;
  }

  static void PostFinished(Transcoder &transcoder, const quint64 job_id, const bool success) {
    for (const SharedPtr<Transcoder::JobState> &state : std::as_const(transcoder.current_jobs_)) {
      if (state->id_ == job_id) {
        state->PostFinished(success);
        return;
      }
    }
  }

  static void PostFinishedEvent(Transcoder &transcoder, const quint64 job_id, const bool success) {
    QCoreApplication::postEvent(&transcoder, new Transcoder::JobFinishedEvent(job_id, success));
  }

  static void Cancel(Transcoder &transcoder) { transcoder.Cancel(); }

  static bool OutputUsedByJob(const Transcoder &transcoder, const QString &output) { return transcoder.OutputUsedByJob(output); }

  // Adds a job with a pipeline with the audio converter, which the pads added by the decoder are linked to.
  static SharedPtr<Transcoder::JobState> AddJobWithConverter(Transcoder &transcoder) {
    Transcoder::Job job;
    job.input = u"input.mkv"_s;
    job.output = u"output.ogg"_s;
    SharedPtr<Transcoder::JobState> state = make_shared<Transcoder::JobState>(++transcoder.next_job_id_, job, &transcoder);
    state->pipeline_ = gst_pipeline_new(nullptr);
    state->convert_element_ = gst_element_factory_make("audioconvert", nullptr);
    gst_bin_add(GST_BIN(state->pipeline_), state->convert_element_);
    return state;
  }

  static void NewPad(const SharedPtr<Transcoder::JobState> &state, GstPad *pad) { Transcoder::NewPadCallback(nullptr, pad, &*state); }

  static GstPad *ConverterPeer(const SharedPtr<Transcoder::JobState> &state) {
    GstPad *sinkpad = gst_element_get_static_pad(state->convert_element_, "sink");
    GstPad *peer = gst_pad_get_peer(sinkpad);
    gst_object_unref(sinkpad);
    if (peer) gst_object_unref(peer);
    return peer;
  }

  static int RunningJobs(const Transcoder &transcoder) {
    return static_cast<int>(transcoder.current_jobs_.count());
  }
};

namespace {

// Creates a source pad with the caps, like the pads decodebin adds.
GstPad *NewSourcePad(const char *name, const char *caps_string) {
  GstCaps *caps = gst_caps_from_string(caps_string);
  GstPadTemplate *pad_template = gst_pad_template_new(name, GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_caps_unref(caps);
  GstPad *pad = gst_pad_new_from_template(pad_template, name);
  gst_object_unref(pad_template);
  return pad;
}

void CountCritical(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
  Q_UNUSED(log_domain);
  Q_UNUSED(log_level);
  Q_UNUSED(message);
  ++*static_cast<int*>(user_data);
}

// Regression test: the transcoder tried to link every pad the decoder added, a video pad after the audio pad caused a GStreamer critical warning when unlinking the audio pad.
TEST(TranscoderTest, OnlyAudioPadIsLinked) {

  GstStartup::Initialize();
  if (!gst_element_factory_find("audioconvert")) {
    GTEST_SKIP() << "The GStreamer audioconvert element is missing.";
  }

  Transcoder transcoder(nullptr);
  const auto state = TranscoderTest::AddJobWithConverter(transcoder);

  GstPad *audio_pad = NewSourcePad("audio", "audio/x-raw");
  GstPad *video_pad = NewSourcePad("video", "video/x-raw");

  TranscoderTest::NewPad(state, audio_pad);
  EXPECT_EQ(audio_pad, TranscoderTest::ConverterPeer(state));

  // A video pad added after the audio pad.
  int criticals = 0;
  const guint handler_id = g_log_set_handler("GStreamer", static_cast<GLogLevelFlags>(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING), CountCritical, &criticals);
  TranscoderTest::NewPad(state, video_pad);
  g_log_remove_handler("GStreamer", handler_id);
  EXPECT_EQ(audio_pad, TranscoderTest::ConverterPeer(state));
  EXPECT_EQ(0, criticals);

  GstPad *sinkpad = gst_element_get_static_pad(state->convert_element_, "sink");
  gst_pad_unlink(audio_pad, sinkpad);
  gst_object_unref(sinkpad);
  gst_object_unref(audio_pad);
  gst_object_unref(video_pad);

}

// Regression test: a pipeline posting two error messages posted two finished events, the second one could finish another job which was allocated at the address of the finished job, as failed.
TEST(TranscoderTest, JobIsOnlyFinishedOnce) {

  GstStartup::Initialize();

  Transcoder transcoder;
  QSignalSpy spy(&transcoder, &Transcoder::JobComplete);

  const quint64 job_id = TranscoderTest::AddRunningJob(transcoder, u"first"_s);
  TranscoderTest::PostFinished(transcoder, job_id, false);
  TranscoderTest::PostFinished(transcoder, job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);

  ASSERT_EQ(1, spy.count());
  EXPECT_EQ(u"first"_s, spy.at(0).at(0).toString());
  EXPECT_FALSE(spy.at(0).at(2).toBool());
  EXPECT_EQ(0, TranscoderTest::RunningJobs(transcoder));

}

// Regression test: a finished event which was handled after its job was removed finished the next job if the next job was allocated at the same address.
TEST(TranscoderTest, LateFinishedEventDoesNotFinishNextJob) {

  GstStartup::Initialize();

  Transcoder transcoder;
  QSignalSpy spy(&transcoder, &Transcoder::JobComplete);

  const quint64 first_job_id = TranscoderTest::AddRunningJob(transcoder, u"first"_s);
  TranscoderTest::PostFinished(transcoder, first_job_id, true);
  QCoreApplication::sendPostedEvents(&transcoder);
  ASSERT_EQ(1, spy.count());

  // A late event for the finished job is ignored, also when the next job is running.
  const quint64 second_job_id = TranscoderTest::AddRunningJob(transcoder, u"second"_s);
  EXPECT_NE(first_job_id, second_job_id);
  TranscoderTest::PostFinishedEvent(transcoder, first_job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);
  EXPECT_EQ(1, spy.count());
  EXPECT_EQ(1, TranscoderTest::RunningJobs(transcoder));

  TranscoderTest::PostFinished(transcoder, second_job_id, true);
  QCoreApplication::sendPostedEvents(&transcoder);
  ASSERT_EQ(2, spy.count());
  EXPECT_EQ(u"second"_s, spy.at(1).at(0).toString());
  EXPECT_TRUE(spy.at(1).at(2).toBool());

}

// Regression test: only existing files were checked when choosing the output filenames, so files with the same name, like tracks from different albums, were transcoded to the same output file.
TEST(TranscoderTest, OutputFilePathNotUsedByAnotherJob) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filepath = temp_dir.filePath(u"01.mp3"_s);

  TranscodeDialog::UsedFilePaths used_filepaths;
  EXPECT_EQ(filepath, TranscodeDialog::UniqueFilePath(filepath, used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"01-1.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"01-2.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));

  // The paths are compared case-insensitively, since the destination can be on a case-insensitive filesystem.
  EXPECT_EQ(temp_dir.filePath(u"01-3.MP3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"01.MP3"_s), used_filepaths));

}

// The numbers continue after the last number used for the path, and existing files are still not used.
TEST(TranscoderTest, OutputFilePathNumbersContinue) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filepath = temp_dir.filePath(u"01.mp3"_s);

  TranscodeDialog::UsedFilePaths used_filepaths;
  EXPECT_EQ(filepath, TranscodeDialog::UniqueFilePath(filepath, used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"01-1.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));
  EXPECT_EQ(2, used_filepaths.next_numbers.value(TranscodeDialog::FilePathKey(filepath)));

  QFile file(temp_dir.filePath(u"01-2.mp3"_s));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.close();
  EXPECT_EQ(temp_dir.filePath(u"01-3.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));
  EXPECT_EQ(4, used_filepaths.next_numbers.value(TranscodeDialog::FilePathKey(filepath)));

  // The numbers before the next number are not tried again.
  used_filepaths.keys.remove(TranscodeDialog::FilePathKey(temp_dir.filePath(u"01-1.mp3"_s)));
  EXPECT_EQ(temp_dir.filePath(u"01-4.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));

}

// The same output file can be written differently, like when the input files are given with redundant "." components, or through a symbolic link.
TEST(TranscoderTest, OutputFilePathNotUsedByAnotherJobWrittenDifferently) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"music"_s));

  TranscodeDialog::UsedFilePaths used_filepaths;
  EXPECT_EQ(temp_dir.filePath(u"music/01.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/01.mp3"_s), used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"music/./01-1.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/./01.mp3"_s), used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"music/../music/01-2.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/../music/01.mp3"_s), used_filepaths));

#ifndef Q_OS_WIN32
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"music"_s), temp_dir.filePath(u"link"_s)));
  EXPECT_EQ(temp_dir.filePath(u"link/01-3.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"link/01.mp3"_s), used_filepaths));

  // ".." after a symbolic link is the parent of the target of the link, not the directory containing the link.
  ASSERT_TRUE(QDir(temp_dir.path()).mkpath(u"other/album"_s));
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"other/album"_s), temp_dir.filePath(u"music/album"_s)));
  EXPECT_EQ(temp_dir.filePath(u"other/02.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"other/02.mp3"_s), used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"music/album/../02-1.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/album/../02.mp3"_s), used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"music/03.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/03.mp3"_s), used_filepaths));
  EXPECT_EQ(temp_dir.filePath(u"music/album/../03.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"music/album/../03.mp3"_s), used_filepaths));
#endif

}

TEST(TranscoderTest, OutputFilePathNotExistingFile) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filepath = temp_dir.filePath(u"01.mp3"_s);
  QFile file(filepath);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.close();

  TranscodeDialog::UsedFilePaths used_filepaths;
  EXPECT_EQ(temp_dir.filePath(u"01-1.mp3"_s), TranscodeDialog::UniqueFilePath(filepath, used_filepaths));

}

// Creates a file, like the output file written by a job.
bool CreateFile(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) return false;
  file.write("partial");
  file.close();
  return true;
}

// Regression test: the partially written output file of a job which failed was kept.
TEST(TranscoderTest, FailedJobRemovesOutput) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output = temp_dir.filePath(u"01.mp3"_s);

  Transcoder transcoder;
  const quint64 job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output);
  ASSERT_TRUE(CreateFile(output));
  TranscoderTest::PostFinished(transcoder, job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);

  EXPECT_FALSE(QFile::exists(output));

}

TEST(TranscoderTest, SuccessfulJobKeepsOutput) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output = temp_dir.filePath(u"01.mp3"_s);

  Transcoder transcoder;
  const quint64 job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output);
  ASSERT_TRUE(CreateFile(output));
  TranscoderTest::PostFinished(transcoder, job_id, true);
  QCoreApplication::sendPostedEvents(&transcoder);

  EXPECT_TRUE(QFile::exists(output));

}

// Regression test: the partially written output files of the running jobs were kept when transcoding was canceled.
TEST(TranscoderTest, CanceledJobRemovesOutput) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output = temp_dir.filePath(u"01.mp3"_s);

  Transcoder transcoder;
  TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output);
  ASSERT_TRUE(CreateFile(output));
  TranscoderTest::Cancel(transcoder);

  EXPECT_FALSE(QFile::exists(output));
  EXPECT_EQ(0, TranscoderTest::RunningJobs(transcoder));

}

// An output file which existed before the job started is not removed, since the job might have failed before writing it.
TEST(TranscoderTest, FailedJobKeepsOutputWhichExisted) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output = temp_dir.filePath(u"01.mp3"_s);
  ASSERT_TRUE(CreateFile(output));

  Transcoder transcoder;
  const quint64 job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output, true);
  TranscoderTest::PostFinished(transcoder, job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);

  EXPECT_TRUE(QFile::exists(output));

}

// Regression test: a job which finished successfully, but its finished event was not handled yet when transcoding was canceled, was handled as canceled, so its output file was removed.
TEST(TranscoderTest, CancelKeepsOutputOfJobWhichFinished) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString finished_output = temp_dir.filePath(u"01.mp3"_s);
  const QString failed_output = temp_dir.filePath(u"02.mp3"_s);

  Transcoder transcoder;
  const quint64 finished_job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, finished_output);
  const quint64 failed_job_id = TranscoderTest::AddRunningJob(transcoder, u"02.flac"_s, failed_output);
  ASSERT_TRUE(CreateFile(finished_output));
  ASSERT_TRUE(CreateFile(failed_output));
  TranscoderTest::PostFinished(transcoder, finished_job_id, true);
  TranscoderTest::PostFinished(transcoder, failed_job_id, false);
  TranscoderTest::Cancel(transcoder);
  QCoreApplication::sendPostedEvents(&transcoder);

  EXPECT_TRUE(QFile::exists(finished_output));
  EXPECT_FALSE(QFile::exists(failed_output));

}

// A symbolic link at the output path existed before the job started, like a dangling symbolic link, so it's not removed.
TEST(TranscoderTest, FailedJobKeepsSymbolicLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link at the output path.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString target = temp_dir.filePath(u"target.mp3"_s);
  const QString output = temp_dir.filePath(u"01.mp3"_s);
  ASSERT_TRUE(QFile::link(target, output));

  Transcoder transcoder;
  const quint64 job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output);
  ASSERT_TRUE(CreateFile(target));
  TranscoderTest::PostFinished(transcoder, job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);

  EXPECT_TRUE(QFileInfo(output).isSymLink());

}

// Sets the cache location used for the output files of the jobs while it exists.
class CacheLocation {
 public:
  explicit CacheLocation(const QString &path) : cache_home_(qgetenv("XDG_CACHE_HOME")) {
    // The cache location is only set with XDG_CACHE_HOME on Unix, except macOS, the test mode is used on the other platforms.
    qputenv("XDG_CACHE_HOME", path.toLocal8Bit());
    QStandardPaths::setTestModeEnabled(true);
  }
  ~CacheLocation() {
    QStandardPaths::setTestModeEnabled(false);
    if (cache_home_.isNull()) {
      qunsetenv("XDG_CACHE_HOME");
    }
    else {
      qputenv("XDG_CACHE_HOME", cache_home_);
    }
  }

 private:
  const QByteArray cache_home_;
  Q_DISABLE_COPY(CacheLocation)
};

// Regression test: only existing files were checked when choosing the output filename, so files with the same name, like tracks from different albums, were transcoded to the same output file.
TEST(TranscoderTest, GetFileNotUsedByAnotherJob) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  Transcoder transcoder;
  const QString first_output = transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
  transcoder.AddJob(u"/music/album1/01.flac"_s, preset, first_output);
  const QString second_output = transcoder.GetFile(u"/music/album2/01.flac"_s, preset);
  TranscoderTest::AddRunningJob(transcoder, u"/music/album2/01.flac"_s, second_output);
  // The first job is queued, and the second job is running.
  const QString third_output = transcoder.GetFile(u"/music/album3/01.flac"_s, preset);
  // The output can be on a case-insensitive filesystem.
  const QString fourth_output = transcoder.GetFile(u"/music/album4/Track.flac"_s, preset);
  transcoder.AddJob(u"/music/album4/Track.flac"_s, preset, fourth_output);
  const QString fifth_output = transcoder.GetFile(u"/music/album5/track.flac"_s, preset);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  EXPECT_TRUE(first_output.startsWith(temp_dir.path()));
#endif
  EXPECT_NE(first_output, second_output);
  EXPECT_NE(first_output, third_output);
  EXPECT_NE(second_output, third_output);
  EXPECT_NE(0, fifth_output.compare(fourth_output, Qt::CaseInsensitive));

}

// Regression test: all the numbers were tried again for each file with the same name, so choosing the output filenames for many files with the same name was slow.
TEST(TranscoderTest, GetFileNumbersContinue) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  Transcoder transcoder;
  const QString first_output = transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
  transcoder.AddJob(u"/music/album1/01.flac"_s, preset, first_output);
  const QString second_output = transcoder.GetFile(u"/music/album2/01.flac"_s, preset);
  const quint64 second_job_id = TranscoderTest::AddRunningJob(transcoder, u"/music/album2/01.flac"_s, second_output);
  TranscoderTest::PostFinished(transcoder, second_job_id, false);
  QCoreApplication::sendPostedEvents(&transcoder);
  // The number of the job which failed is not tried again while there are other jobs.
  const QString third_output = transcoder.GetFile(u"/music/album3/01.flac"_s, preset);

  // The numbers start again when there are no more jobs.
  TranscoderTest::Cancel(transcoder);
  transcoder.AddJob(u"/music/album1/01.flac"_s, preset, first_output);
  const QString fourth_output = transcoder.GetFile(u"/music/album4/01.flac"_s, preset);

  EXPECT_TRUE(first_output.endsWith(u"/01.mp3"_s));
  EXPECT_TRUE(second_output.endsWith(u"/01-0.mp3"_s));
  EXPECT_TRUE(third_output.endsWith(u"/01-1.mp3"_s));
  EXPECT_TRUE(fourth_output.endsWith(u"/01-0.mp3"_s));

}

// The output of a job is not used by a job anymore when the job is finished or canceled.
TEST(TranscoderTest, OutputNotUsedAfterJobFinished) {

  GstStartup::Initialize();

  Transcoder transcoder;
  const quint64 first_job_id = TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, u"/output/01.mp3"_s);
  TranscoderTest::AddRunningJob(transcoder, u"02.flac"_s, u"/output/02.mp3"_s);
  // Another job with the same output, written differently.
  const quint64 third_job_id = TranscoderTest::AddRunningJob(transcoder, u"03.flac"_s, u"/output/01.MP3"_s);
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/01.mp3"_s));

  // The output is still used by the other job.
  TranscoderTest::PostFinished(transcoder, first_job_id, true);
  QCoreApplication::sendPostedEvents(&transcoder);
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/01.mp3"_s));

  TranscoderTest::PostFinished(transcoder, third_job_id, true);
  QCoreApplication::sendPostedEvents(&transcoder);
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/01.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/02.mp3"_s));

  // The queued and running jobs are canceled.
  transcoder.AddJob(u"04.flac"_s, TranscoderPreset(), u"/output/04.mp3"_s);
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/04.mp3"_s));
  TranscoderTest::Cancel(transcoder);
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/02.mp3"_s));
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/04.mp3"_s));

}

// The output of a job which failed to start is not used by a job anymore.
TEST(TranscoderTest, OutputNotUsedAfterJobFailedToStart) {

  GstStartup::Initialize();

  TranscoderPreset preset;
  preset.codec_mimetype_ = u"audio/x-strawberry-unknown"_s;

  Transcoder transcoder;
  QSignalSpy spy(&transcoder, &Transcoder::JobComplete);
  transcoder.AddJob(u"01.flac"_s, preset, u"/output/01.mp3"_s);
  transcoder.Start();

  ASSERT_EQ(1, spy.count());
  EXPECT_FALSE(spy.at(0).at(2).toBool());
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, u"/output/01.mp3"_s));

}

// Regression test: outputs were only compared case-insensitively, so the same output written differently, like through a symbolic link, as a relative path, or with a decomposed character, could be used by two jobs.
TEST(TranscoderTest, OutputUsedByJobWrittenDifferently) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"music"_s));

  Transcoder transcoder;
  transcoder.AddJob(u"01.flac"_s, TranscoderPreset(), temp_dir.filePath(u"music/01.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/../music/./01.mp3"_s)));
  // The directory is created when the job is started.
  transcoder.AddJob(u"03.flac"_s, TranscoderPreset(), temp_dir.filePath(u"new/03.mp3"_s));

  const QString current_path = QDir::currentPath();
  ASSERT_TRUE(QDir::setCurrent(temp_dir.path()));
  const bool relative_used = TranscoderTest::OutputUsedByJob(transcoder, u"music/01.mp3"_s);
  const bool relative_new_used = TranscoderTest::OutputUsedByJob(transcoder, u"new/03.mp3"_s);
  QDir::setCurrent(current_path);
  EXPECT_TRUE(relative_used);
  EXPECT_TRUE(relative_new_used);

  // The same character, composed and decomposed.
  transcoder.AddJob(u"02.flac"_s, TranscoderPreset(), temp_dir.filePath(u"music/café.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/café.mp3"_s)));

#ifndef Q_OS_WIN
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"music"_s), temp_dir.filePath(u"link"_s)));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"link/01.mp3"_s)));
#endif

}

// Regression test: the output files were only reserved for the jobs of the same transcoder, so organizing to several devices at once could use the same output file for different files, then a job which failed could remove the output file of a job which finished.
TEST(TranscoderTest, GetFileNotUsedByAnotherTranscoder) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  Transcoder first_transcoder;
  const QString first_output = first_transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
  first_transcoder.AddJob(u"/music/album1/01.flac"_s, preset, first_output);
  Transcoder second_transcoder;
  const QString second_output = second_transcoder.GetFile(u"/music/album2/01.flac"_s, preset);

  EXPECT_NE(first_output, second_output);
  EXPECT_NE(QFileInfo(first_output).path(), QFileInfo(second_output).path());
  EXPECT_TRUE(QFileInfo(first_output).dir().exists());

}

// The directory for the output files of a transcoder is removed with the transcoder, with the files which were not removed.
TEST(TranscoderTest, CacheDirectoryRemovedWithTranscoder) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  QString output;
  {
    Transcoder transcoder;
    output = transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
    ASSERT_TRUE(CreateFile(output));
  }

  EXPECT_FALSE(QFileInfo::exists(output));
  EXPECT_FALSE(QFileInfo(output).dir().exists());

}

// Regression test: the output filenames were only compared case-insensitively in the transcode dialog, so the same filename with a composed and a decomposed character was used by two jobs.
TEST(TranscoderTest, OutputFilePathNotUsedByAnotherJobDecomposed) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  TranscodeDialog::UsedFilePaths used_filepaths;
  const QString composed = TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"café.mp3"_s), used_filepaths);
  const QString decomposed = TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"café.mp3"_s), used_filepaths);

  EXPECT_EQ(temp_dir.filePath(u"café.mp3"_s), composed);
  EXPECT_EQ(temp_dir.filePath(u"café-1.mp3"_s), decomposed);

}

// Regression test: the ".." components were removed before the symbolic links were resolved, so ".." after a symbolic link to a directory was handled as the parent of the link, not of the target.
TEST(TranscoderTest, OutputUsedByJobParentOfSymbolicLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkpath(u"other/album"_s));
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"music"_s));
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"other/album"_s), temp_dir.filePath(u"music/album"_s)));

  Transcoder transcoder;
  transcoder.AddJob(u"02.flac"_s, TranscoderPreset(), temp_dir.filePath(u"other/02.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/album/../02.mp3"_s)));
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/02.mp3"_s)));

}

// Regression test: the partial output files of the running jobs were kept when the transcoder was destroyed, like when closing the application while transcoding.
TEST(TranscoderTest, DestroyingTranscoderRemovesOutput) {

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output = temp_dir.filePath(u"01.mp3"_s);

  {
    Transcoder transcoder;
    TranscoderTest::AddRunningJob(transcoder, u"01.flac"_s, output);
    ASSERT_TRUE(CreateFile(output));
  }

  EXPECT_FALSE(QFile::exists(output));

}

// Regression test: the path to a directory which didn't exist yet was only cleaned, so a symbolic link in the existing part was not resolved, and the key changed when the directory was created.
TEST(TranscoderTest, OutputUsedByJobMissingDirectoryInSymbolicLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"real"_s));
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"real"_s), temp_dir.filePath(u"alias"_s)));

  Transcoder transcoder;
  transcoder.AddJob(u"01.flac"_s, TranscoderPreset(), temp_dir.filePath(u"alias/new/01.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"real/new/01.mp3"_s)));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"real/new/missing/../01.mp3"_s)));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"real/new/./01.mp3"_s)));

  // The key is the same when the directory is created.
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"real/new"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"alias/new/01.mp3"_s)));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"real/new/01.mp3"_s)));

}

// Regression test: the components after a directory which didn't exist yet were not resolved, so ".." followed by a symbolic link gave another directory when the directory was created.
TEST(TranscoderTest, OutputUsedByJobSymbolicLinkAfterMissingDirectory) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkpath(u"other/album"_s));
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"music"_s));
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"other/album"_s), temp_dir.filePath(u"music/link"_s)));

  Transcoder transcoder;
  transcoder.AddJob(u"01.flac"_s, TranscoderPreset(), temp_dir.filePath(u"music/missing/../link/../01.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"other/01.mp3"_s)));
  EXPECT_FALSE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/01.mp3"_s)));

  // The key is the same when the missing directory is created.
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"music/missing"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"music/missing/../link/../01.mp3"_s)));

}

// Regression test: the shared cache directory was used when the cache directory of the transcoder couldn't be created, then the jobs of other transcoders could use the same output files.
TEST(TranscoderTest, GetFileFailsWithoutCacheDirectory) {

#if !defined(Q_OS_UNIX) || defined(Q_OS_MACOS)
  GTEST_SKIP() << "The cache location is only set with XDG_CACHE_HOME on Unix, except macOS.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  // A file where the cache directory should be, so the directory can't be created.
  const QString cache_path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation);
  ASSERT_TRUE(cache_path.startsWith(temp_dir.path()));
  ASSERT_TRUE(QDir().mkpath(QFileInfo(cache_path).path()));
  ASSERT_TRUE(CreateFile(cache_path));

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  Transcoder transcoder;
  EXPECT_TRUE(transcoder.GetFile(u"/music/album1/01.flac"_s, preset).isEmpty());

  // The directory is created when it can be created again.
  ASSERT_TRUE(QFile::remove(cache_path));
  const QString output = transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
  EXPECT_TRUE(output.startsWith(cache_path + u"/transcoder/"_s));
  EXPECT_TRUE(QFileInfo(output).dir().exists());

}

// Regression test: a dangling symbolic link was not seen as an existing file, so a file and a link to it could be used as the outputs of two jobs.
TEST(TranscoderTest, OutputFilePathNotDanglingSymbolicLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link.";
#endif

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"02.mp3"_s), temp_dir.filePath(u"01.mp3"_s)));

  TranscodeDialog::UsedFilePaths used_filepaths;
  EXPECT_EQ(temp_dir.filePath(u"01-1.mp3"_s), TranscodeDialog::UniqueFilePath(temp_dir.filePath(u"01.mp3"_s), used_filepaths));

}

TEST(TranscoderTest, GetFileNotDanglingSymbolicLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CacheLocation cache_location(temp_dir.path());

  TranscoderPreset preset;
  preset.extension_ = u"mp3"_s;

  Transcoder transcoder;
  const QString first_output = transcoder.GetFile(u"/music/album1/01.flac"_s, preset);
  ASSERT_FALSE(first_output.isEmpty());
  const QString cache_directory = QFileInfo(first_output).path();
  ASSERT_TRUE(QFile::link(cache_directory + u"/target.mp3"_s, cache_directory + u"/02.mp3"_s));

  EXPECT_EQ(cache_directory + u"/02-0.mp3"_s, transcoder.GetFile(u"/music/album1/02.flac"_s, preset));

}

// Paths starting with "//" are local paths on the platforms other than Windows, so they're resolved like the other paths.
TEST(TranscoderTest, OutputUsedByJobDoubleSlash) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "Paths starting with \"//\" are network paths on Windows.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"real"_s));
  ASSERT_TRUE(QFile::link(temp_dir.filePath(u"real"_s), temp_dir.filePath(u"alias"_s)));

  Transcoder transcoder;
  transcoder.AddJob(u"01.flac"_s, TranscoderPreset(), u"/"_s + temp_dir.filePath(u"alias/new/01.mp3"_s));
  EXPECT_TRUE(TranscoderTest::OutputUsedByJob(transcoder, temp_dir.filePath(u"real/new/01.mp3"_s)));

}

}  // namespace
