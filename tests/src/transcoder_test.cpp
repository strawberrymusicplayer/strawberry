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
#include <QDir>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include "includes/shared_ptr.h"
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
    SharedPtr<Transcoder::JobState> state = make_shared<Transcoder::JobState>(++transcoder.next_job_id_, job, &transcoder);
    state->output_existed_ = output_existed;
    state->pipeline_ = gst_pipeline_new(nullptr);
    transcoder.current_jobs_ << state;
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

  static int RunningJobs(const Transcoder &transcoder) {
    return static_cast<int>(transcoder.current_jobs_.count());
  }
};

namespace {

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

// Regression test: only existing files were checked when choosing the output filename, so files with the same name, like tracks from different albums, were transcoded to the same output file.
TEST(TranscoderTest, GetFileNotUsedByAnotherJob) {

#if !defined(Q_OS_UNIX) || defined(Q_OS_MACOS)
  GTEST_SKIP() << "The cache location is only set with XDG_CACHE_HOME on Unix, except macOS.";
#endif

  GstStartup::Initialize();

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QByteArray cache_home = qgetenv("XDG_CACHE_HOME");
  qputenv("XDG_CACHE_HOME", temp_dir.path().toLocal8Bit());

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

  if (cache_home.isNull()) {
    qunsetenv("XDG_CACHE_HOME");
  }
  else {
    qputenv("XDG_CACHE_HOME", cache_home);
  }

  EXPECT_TRUE(first_output.startsWith(temp_dir.path()));
  EXPECT_NE(first_output, second_output);
  EXPECT_NE(first_output, third_output);
  EXPECT_NE(second_output, third_output);
  EXPECT_NE(0, fifth_output.compare(fourth_output, Qt::CaseInsensitive));

}

}  // namespace
