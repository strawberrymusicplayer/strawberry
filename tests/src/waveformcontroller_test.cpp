/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include "gtest_include.h"
#include "gmock_include.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QVariant>

#include "includes/shared_ptr.h"
#include "engine/gststartup.h"
#include "engine/enginebase.h"
#include "core/song.h"
#include "core/playerinterface.h"
#include "core/standardpaths.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "waveform/waveformcontroller.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"

#include "mock_playlistitem.h"
#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

using std::make_shared;

using ::testing::Return;

namespace {

// Minimal PlayerInterface stub exposing only the state/current-item accessors
// WaveformController consults in its async-completion guards. All other pure
// virtuals are no-ops so the controller can be exercised without a real Player.
class StubPlayer : public PlayerInterface {
 public:
  StubPlayer() : state_(EngineBase::State::Playing) {}

  void SetState(const EngineBase::State state) { state_ = state; }
  void SetCurrentItem(PlaylistItemPtr item) { current_item_ = item; }

  SharedPtr<EngineBase> engine() const override { return nullptr; }
  EngineBase::State GetState() const override { return state_; }
  uint GetVolume() const override { return 0; }
  PlaylistItemPtr GetCurrentItem() const override { return current_item_; }
  PlaylistItemPtr GetItemAt(const int) const override { return nullptr; }

  void ReloadSettings() override {}
  void LoadVolume() override {}
  void SaveVolume() override {}
  void SavePlaybackStatus() override {}
  void PlaylistsLoaded() override {}
  void PlayAt(const int, const bool, const quint64, EngineBase::TrackChangeFlags, const Playlist::AutoScroll, const bool, const bool) override {}
  void PlayPause(const quint64, const Playlist::AutoScroll) override {}
  void PlayPauseHelper() override {}
  void RestartOrPrevious() override {}
  void Next() override {}
  void Previous() override {}
  void PlayPlaylist(const QString &) override {}
  void SetVolumeFromEngine(const uint) override {}
  void SetVolumeFromSlider(const int) override {}
  void SetVolume(const uint) override {}
  void VolumeUp() override {}
  void VolumeDown() override {}
  void SeekTo(const quint64) override {}
  void SeekForward() override {}
  void SeekBackward() override {}
  void CurrentMetadataChanged(const Song &) override {}
  void Mute() override {}
  void Pause() override {}
  void Stop(const bool) override {}
  void Play(const quint64) override {}
  void PlayWithPause(const quint64) override {}
  void PlayHelper() override {}
  void ShowOSD() override {}

 private:
  EngineBase::State state_;
  PlaylistItemPtr current_item_;
};

Song MakeSong(const QUrl &url) {
  Song song;
  song.set_url(url);
  return song;
}

// Resets the waveform cache to an isolated empty directory so the real loader's
// first Load on a local file takes the async generation path deterministically.
void ResetWaveformCache() {
  QStandardPaths::setTestModeEnabled(true);
  const QString waveform_cache_dir = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s;
  QDir(waveform_cache_dir).removeRecursively();
}

}  // namespace

TEST(WaveformControllerTest, DisabledDoesNotLoadOnSongChange) {

  ResetWaveformCache();

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // Disabled by default: a song change must not trigger any load or emission,
  // so no decode runs in the background while the waveform is hidden.
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));

  EXPECT_EQ(spy.count(), 0);

}

TEST(WaveformControllerTest, EnableMidTrackLoadsCurrentSong) {

  ResetWaveformCache();

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // A non-local song is the current track but the waveform is still disabled.
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // Enabling mid-track must generate for the song that is already playing; the
  // non-local URL is CannotLoad, so the controller emits an empty payload.
  controller.SetEnabled(true);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

}

TEST(WaveformControllerTest, DisableEmitsEmptyData) {

  ResetWaveformCache();

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  controller.SetEnabled(true);

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // Disabling reverts the seekbar to a plain slider.
  controller.SetEnabled(false);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

}

TEST(WaveformControllerTest, EnabledCannotLoadEmitsEmptyData) {

  ResetWaveformCache();

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);
  controller.SetEnabled(true);

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // A non-local URL can never be loaded, so the loader returns CannotLoad and
  // the controller must emit an empty payload (revert to a plain slider).
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

}

TEST(WaveformControllerTest, PlaybackStoppedEmitsEmptyData) {

  ResetWaveformCache();

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);
  controller.SetEnabled(true);

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  controller.PlaybackStopped();

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

}

TEST(WaveformControllerTest, StaleUrlAsyncLoadCompleteDoesNotEmit) {

  GstStartup::Initialize();
  ResetWaveformCache();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl playing_url = QUrl::fromLocalFile(res.fileName());

  // The player reports a DIFFERENT current track than the one that was loading,
  // simulating a song change while the pipeline was in flight.
  SharedPtr<MockPlaylistItem> current_item = make_shared<MockPlaylistItem>();
  const QUrl other_url = QUrl::fromLocalFile(u"/some/other/track.flac"_s);
  EXPECT_CALL(*current_item, OriginalUrl()).WillRepeatedly(Return(other_url));

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  player->SetState(EngineBase::State::Playing);
  player->SetCurrentItem(current_item);

  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);
  controller.SetEnabled(true);

  // First Load on the local file takes the async path: this connects the
  // pipeline Finished signal to the controller's AsyncLoadComplete guard.
  const WaveformLoader::LoadResult load_result = loader->Load(playing_url, false);
  ASSERT_EQ(load_result.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(load_result.pipeline);

  // Drive CurrentSongChanged so the controller wires its async-completion slot.
  controller.CurrentSongChanged(MakeSong(playing_url));

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // Wait for the in-flight pipeline to finish; AsyncLoadComplete then runs its
  // guard, finds the current track URL no longer matches, and returns early.
  QSignalSpy finished_spy(&*load_result.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(finished_spy.wait(10000));

  // No data-changed emission: the stale-URL guard suppressed it.
  EXPECT_EQ(spy.count(), 0);

}
