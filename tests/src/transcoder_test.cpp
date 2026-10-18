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
#include <QSignalSpy>
#include <QCoreApplication>

#include "includes/shared_ptr.h"
#include "engine/gststartup.h"
#include "transcoder/transcoder.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

class TranscoderTest {
 public:
  // Adds a running job without starting a pipeline, it gets an empty pipeline, so it can be finished like a real job.
  static quint64 AddRunningJob(Transcoder &transcoder, const QString &input) {
    Transcoder::Job job;
    job.input = input;
    job.output = input + u".out"_s;
    SharedPtr<Transcoder::JobState> state = make_shared<Transcoder::JobState>(++transcoder.next_job_id_, job, &transcoder);
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

}  // namespace
