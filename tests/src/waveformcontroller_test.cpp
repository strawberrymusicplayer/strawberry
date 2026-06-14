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
#include <QCoreApplication>
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
#include "core/settings.h"
#include "core/standardpaths.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "waveform/waveformcontroller.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"
#include "constants/waveformsettings.h"

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

TEST(WaveformControllerTest, ConstructorReadsKEnabledFromSettings) {

  ResetWaveformCache();
  QStandardPaths::setTestModeEnabled(true);

  // Write kEnabled=true before constructing the controller.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // enabled_ must be true because the constructor called ReloadSettings().
  // Verify indirectly: CurrentSongChanged on a non-local URL with enabled_=true
  // emits an empty payload (CannotLoad path), whereas disabled would emit nothing.
  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));

  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

  // Cleanup: reset kEnabled so other tests start from a clean slate.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}

TEST(WaveformControllerTest, ReloadSettingsUpdatesEnabledFlag) {

  ResetWaveformCache();
  QStandardPaths::setTestModeEnabled(true);

  // Start disabled.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // Disable path: no emission on song change.
  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));
  EXPECT_EQ(spy.count(), 0);

  // Now flip kEnabled in settings and call ReloadSettings().
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }
  controller.ReloadSettings();

  // ReloadSettings with a stored current song emits immediately (CR-01 fix):
  // the non-local URL is CannotLoad, so an empty payload is emitted.
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

  // After reload, enabled_=true; a subsequent song change also emits empty.
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track2.mp3"_s)));
  ASSERT_EQ(spy.count(), 2);
  EXPECT_TRUE(spy.at(1).at(0).value<QByteArray>().isEmpty());

  // Cleanup.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}

TEST(WaveformControllerTest, ReloadSettingsDisabledMidAsyncDoesNotEmitData) {

  GstStartup::Initialize();
  ResetWaveformCache();
  QStandardPaths::setTestModeEnabled(true);

  // Set kEnabled=true before constructing so ReloadSettings() in ctor gives enabled_=true.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  SharedPtr<MockPlaylistItem> current_item = make_shared<MockPlaylistItem>();
  EXPECT_CALL(*current_item, OriginalUrl()).WillRepeatedly(Return(url));

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  player->SetState(EngineBase::State::Playing);
  player->SetCurrentItem(current_item);

  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // Drive CurrentSongChanged so the controller starts the async load and wires
  // its completion slot — enabled_=true so GenerateWaveform is called.
  controller.CurrentSongChanged(MakeSong(url));

  // Disable via ReloadSettings (simulates settings dialog save with kEnabled=false).
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }
  controller.ReloadSettings();

  // Attach data-change spy AFTER disabling so we only observe post-disable emissions.
  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  // Pump the event loop long enough for the GStreamer pipeline to finish the
  // short WAV file and for any queued AsyncLoadComplete delivery to reach the
  // controller. 2 s is ample; the pipeline typically completes in < 50 ms.
  spy.wait(2000);

  // Regardless of whether the pipeline was still in-flight: AsyncLoadComplete
  // must have returned early (enabled_=false), so no data payload is emitted.
  for (int i = 0; i < spy.count(); ++i) {
    EXPECT_TRUE(spy.at(i).at(0).value<QByteArray>().isEmpty());
  }

  // Cleanup.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}

TEST(WaveformControllerTest, ReloadSettingsEnableMidTrackEmits) {

  ResetWaveformCache();
  QStandardPaths::setTestModeEnabled(true);

  // Start disabled.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // Set a current song while disabled; the controller stores it but does not load.
  controller.CurrentSongChanged(MakeSong(QUrl(u"http://example.com/track.mp3"_s)));

  // Flip kEnabled=true in QSettings and reload (simulates Preferences dialog Save).
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  controller.ReloadSettings();

  // The non-local URL cannot be loaded, so the controller emits an empty payload —
  // same as the SetEnabled mid-track path.
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

  // Cleanup.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}

TEST(WaveformControllerTest, ReloadSettingsDisableEmitsEmptyData) {

  ResetWaveformCache();
  QStandardPaths::setTestModeEnabled(true);

  // Start enabled.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  SharedPtr<StubPlayer> player = make_shared<StubPlayer>();
  SharedPtr<WaveformLoader> loader = make_shared<WaveformLoader>();
  WaveformController controller(player, loader);

  // Flip kEnabled=false in QSettings and reload (simulates Preferences dialog Save).
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

  QSignalSpy spy(&controller, &WaveformController::CurrentWaveformDataChanged);

  controller.ReloadSettings();

  // Disabling via ReloadSettings must revert the seekbar to a plain slider.
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<QByteArray>().isEmpty());

  // Cleanup.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}
