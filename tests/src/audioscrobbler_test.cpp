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

#include "gtest_include.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobbler/audioscrobbler.h"
#include "scrobbler/scrobblerservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// Records the songs now playing, like the services do to know which song to send love for.
class TestScrobblerService : public ScrobblerService {
 public:
  explicit TestScrobblerService(const SharedPtr<ScrobblerSettingsService> settings) : ScrobblerService(u"Test"_s, nullptr, settings, nullptr) {}

  void ReloadSettings() override {}
  bool authentication_required() const override { return false; }
  bool use_authorization_header() const override { return false; }
  QByteArray authorization_header() const override { return QByteArray(); }
  bool enabled() const override { return true; }
  void UpdateNowPlaying(const Song &song) override { song_playing = song; }
  void ClearPlaying() override { song_playing = Song(); }
  void Scrobble(const Song &song) override { Q_UNUSED(song) }
  void Start(const bool initial) override { Q_UNUSED(initial) }
  void Stop() override {}
  void WriteCache() override {}

  Song song_playing;
};

Song MakeSong(const QString &artist, const QString &title) {
  Song song(Song::Source::Collection);
  song.Init(title, artist, u"Album"_s, 123);
  song.set_url(QUrl(u"file:///"_s + title));
  return song;
}

class AudioScrobblerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    service_ = make_shared<TestScrobblerService>(scrobbler_.settings());
    scrobbler_.AddService(service_);
  }

  AudioScrobbler scrobbler_;
  SharedPtr<TestScrobblerService> service_;
};

TEST_F(AudioScrobblerTest, UpdatesNowPlayingForSongWithGoodMetadata) {

  scrobbler_.UpdateNowPlaying(MakeSong(u"Artist"_s, u"First"_s));

  EXPECT_EQ(u"First"_s, service_->song_playing.title());

}

// Regression test: a song without good metadata left the previous song as now playing, so love was sent for the previous song.
TEST_F(AudioScrobblerTest, SongWithoutGoodMetadataClearsPreviousSong) {

  scrobbler_.UpdateNowPlaying(MakeSong(u"Artist"_s, u"First"_s));
  ASSERT_TRUE(service_->song_playing.is_valid());

  scrobbler_.UpdateNowPlaying(MakeSong(QString(), u"Second"_s));

  EXPECT_FALSE(service_->song_playing.is_valid());

}

}  // namespace
