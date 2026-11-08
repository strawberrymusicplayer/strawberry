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

#include <memory>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/song.h"
#include "constants/scrobblersettings.h"
#include "scrobbler/scrobblersettingsservice.h"
#include "scrobbler/subsonicscrobbler.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// Records the scrobbles instead of sending them to a Subsonic server.
class TestSubsonicScrobbler : public SubsonicScrobbler {
 public:
  explicit TestSubsonicScrobbler(const SharedPtr<ScrobblerSettingsService> settings) : SubsonicScrobbler(settings, nullptr, nullptr) {}

  QStringList sent_song_ids;

 protected:
  void SendScrobble(const QString &song_id, const QDateTime &time) override {
    Q_UNUSED(time)
    sent_song_ids << song_id;
  }
};

class SubsonicScrobblerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    Settings s;
    s.beginGroup(ScrobblerSettings::kSettingsGroup);
    s.setValue(ScrobblerSettings::kOffline, false);
    // The submit delay is in seconds.
    s.setValue(ScrobblerSettings::kSubmit, 1);
    s.endGroup();

    settings_ = make_shared<ScrobblerSettingsService>();
    scrobbler_ = std::make_unique<TestSubsonicScrobbler>(settings_);

  }

  static Song MakeSong(const QString &song_id) {
    Song song(Song::Source::Subsonic);
    song.Init(u"Title "_s + song_id, u"Artist"_s, u"Album"_s, 123);
    song.set_song_id(song_id);
    song.set_url(QUrl(u"subsonic:"_s + song_id));
    return song;
  }

  void Play(const QString &song_id) {
    const Song song = MakeSong(song_id);
    scrobbler_->UpdateNowPlaying(song);
    scrobbler_->Scrobble(song);
  }

  // Processes events until the number of scrobbles is sent, or the timeout.
  void WaitForSentScrobbles(const qsizetype count, const int timeout_ms = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (scrobbler_->sent_song_ids.count() < count && timer.elapsed() < timeout_ms) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
  }

  SharedPtr<ScrobblerSettingsService> settings_;
  std::unique_ptr<TestSubsonicScrobbler> scrobbler_;
};

// Regression test: a song scrobbled while the scrobble of the previous song was waiting for the submit delay was not scrobbled.
TEST_F(SubsonicScrobblerTest, ScrobblesEverySongDuringSubmitDelay) {

  Play(u"1"_s);
  Play(u"2"_s);
  Play(u"3"_s);
  EXPECT_TRUE(scrobbler_->sent_song_ids.isEmpty());

  WaitForSentScrobbles(3);
  EXPECT_EQ(QStringList() << u"1"_s << u"2"_s << u"3"_s, scrobbler_->sent_song_ids);

}

// The same play is only scrobbled once.
TEST_F(SubsonicScrobblerTest, ScrobblesSamePlayOnce) {

  const Song song = MakeSong(u"1"_s);
  scrobbler_->UpdateNowPlaying(song);
  scrobbler_->Scrobble(song);
  scrobbler_->Scrobble(song);

  WaitForSentScrobbles(2, 2000);
  EXPECT_EQ(QStringList() << u"1"_s, scrobbler_->sent_song_ids);

}

// Regression test: the scrobbles pending when scrobbling was stopped are sent when it's started again, also when another song was scrobbled before.
TEST_F(SubsonicScrobblerTest, SendsScrobblesPendingWhenStopped) {

  Play(u"1"_s);
  scrobbler_->Stop();
  Play(u"2"_s);
  scrobbler_->Stop();

  scrobbler_->Start();
  WaitForSentScrobbles(2);
  EXPECT_EQ(QStringList() << u"1"_s << u"2"_s, scrobbler_->sent_song_ids);

}

// Regression test: when the pending scrobble was not sent because offline mode was switched on, scrobbling the same play again after offline mode was switched off didn't schedule it.
TEST_F(SubsonicScrobblerTest, SamePlayAfterOfflineSendsPendingScrobble) {

  const Song song = MakeSong(u"1"_s);
  scrobbler_->UpdateNowPlaying(song);
  scrobbler_->Scrobble(song);

  // The timer fires while offline, the scrobble stays pending.
  settings_->ToggleOffline();
  ASSERT_TRUE(settings_->offline());
  WaitForSentScrobbles(1, 1500);
  ASSERT_TRUE(scrobbler_->sent_song_ids.isEmpty());
  settings_->ToggleOffline();
  ASSERT_FALSE(settings_->offline());

  scrobbler_->Scrobble(song);
  WaitForSentScrobbles(1);
  EXPECT_EQ(QStringList() << u"1"_s, scrobbler_->sent_song_ids);

}

// Regression test: scrobbling the same play again after its scrobble was sent sent it again, since only the pending scrobbles were checked.
TEST_F(SubsonicScrobblerTest, ScrobblesSamePlayOnceAfterSent) {

  const Song song = MakeSong(u"1"_s);
  scrobbler_->UpdateNowPlaying(song);
  scrobbler_->Scrobble(song);
  WaitForSentScrobbles(1);
  ASSERT_EQ(QStringList() << u"1"_s, scrobbler_->sent_song_ids);

  scrobbler_->Scrobble(song);
  WaitForSentScrobbles(2, 2000);
  EXPECT_EQ(QStringList() << u"1"_s, scrobbler_->sent_song_ids);

  // Playing the song again is a new play, which is scrobbled.
  Play(u"1"_s);
  WaitForSentScrobbles(2);
  EXPECT_EQ(QStringList() << u"1"_s << u"1"_s, scrobbler_->sent_song_ids);

}

}  // namespace
