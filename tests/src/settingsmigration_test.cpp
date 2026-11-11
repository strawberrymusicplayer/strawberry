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

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "core/settings.h"
#include "core/song.h"
#include "core/settingsmigration.h"
#include "constants/albumcoverexportsettings.h"
#include "constants/appearancesettings.h"
#include "constants/backendsettings.h"
#include "constants/behavioursettings.h"
#include "constants/contextsettings.h"
#include "constants/notificationssettings.h"
#include "constants/jellyfinsettings.h"
#include "constants/plexsettings.h"
#include "constants/qobuzsettings.h"
#include "constants/playlistsettings.h"
#include "constants/scrobblersettings.h"
#include "constants/spotifysettings.h"
#include "constants/subsonicsettings.h"
#include "constants/tidalsettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {

using SettingsMigration::RenamedKey;

struct SettingsKey {
  const char *group;
  const char *key;
};

// The keys read by the code which renamed keys are moved to, the migration must move the old keys to exactly these keys.
const SettingsKey kKeysReadByCode[] = {
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kUseCustomColorSet},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorWindow},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorWindowText},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorBase},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorAlternateBase},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorToolTipBase},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorToolTipText},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorText},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorButton},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorButtonText},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorBrightText},
  {AppearanceSettings::kSettingsGroup, AppearanceSettings::kColorPlaceholderText},
  {BackendSettings::kSettingsGroup, BackendSettings::kFadeoutEnabled},
  {BackendSettings::kSettingsGroup, BackendSettings::kCrossfadeEnabled},
  {BackendSettings::kSettingsGroup, BackendSettings::kAutoCrossfadeEnabled},
  {BackendSettings::kSettingsGroup, BackendSettings::kNoCrossfadeSameAlbum},
  {BackendSettings::kSettingsGroup, BackendSettings::kFadeoutPauseEnabled},
  {BackendSettings::kSettingsGroup, BackendSettings::kFadeoutDuration},
  {BackendSettings::kSettingsGroup, BackendSettings::kFadeoutPauseDuration},
  {ContextSettings::kSettingsGroup, ContextSettings::kSettingsTitleFmt},
  {ContextSettings::kSettingsGroup, ContextSettings::kSettingsSummaryFmt},
  {ContextSettings::kSettingsGroup, ContextSettings::kAlbum},
  {ContextSettings::kSettingsGroup, ContextSettings::kTechnicalData},
  {ContextSettings::kSettingsGroup, ContextSettings::kSongLyrics},
  {ContextSettings::kSettingsGroup, ContextSettings::kSyncedLyrics},
  {ContextSettings::kSettingsGroup, ContextSettings::kSearchCover},
  {ContextSettings::kSettingsGroup, ContextSettings::kSearchLyrics},
  {OSDSettings::kSettingsGroup, OSDSettings::kType},
  {OSDSettings::kSettingsGroup, OSDSettings::kTimeout},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowOnVolumeChange},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowOnPlayModeChange},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowOnPausePlayback},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowOnResumePlayback},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowOnStreamTitleChange},
  {OSDSettings::kSettingsGroup, OSDSettings::kShowArt},
  {OSDSettings::kSettingsGroup, OSDSettings::kCustomTextEnabled},
  {OSDSettings::kSettingsGroup, OSDSettings::kCustomText1},
  {OSDSettings::kSettingsGroup, OSDSettings::kCustomText2},
  {DiscordRPCSettings::kSettingsGroup, DiscordRPCSettings::kStatusDisplayType},
  {AlbumCoverExportSettings::kSettingsGroup, AlbumCoverExportSettings::kFileName},
  {AlbumCoverExportSettings::kSettingsGroup, AlbumCoverExportSettings::kForceSize},
  {BehaviourSettings::kSettingsGroup, BehaviourSettings::kKeepRunning},
  {BehaviourSettings::kSettingsGroup, BehaviourSettings::kResumePlayback},
  {BehaviourSettings::kSettingsGroup, BehaviourSettings::kStartupBehaviour},
  {JellyfinSettings::kSettingsGroup, JellyfinSettings::kVerifyCertificate},
  {JellyfinSettings::kSettingsGroup, JellyfinSettings::kDownloadAlbumCovers},
  {JellyfinSettings::kSettingsGroup, JellyfinSettings::kServerSideScrobbling},
  {PlexSettings::kSettingsGroup, PlexSettings::kVerifyCertificate},
  {PlexSettings::kSettingsGroup, PlexSettings::kDownloadAlbumCovers},
  {PlexSettings::kSettingsGroup, PlexSettings::kClientId},
  {QobuzSettings::kSettingsGroup, QobuzSettings::kDownloadAlbumCovers},
  {QobuzSettings::kSettingsGroup, QobuzSettings::kSearchDelay},
  {QobuzSettings::kSettingsGroup, QobuzSettings::kArtistsSearchLimit},
  {QobuzSettings::kSettingsGroup, QobuzSettings::kAlbumsSearchLimit},
  {QobuzSettings::kSettingsGroup, QobuzSettings::kSongsSearchLimit},
  {TidalSettings::kSettingsGroup, TidalSettings::kDownloadAlbumCovers},
  {TidalSettings::kSettingsGroup, TidalSettings::kSearchDelay},
  {TidalSettings::kSettingsGroup, TidalSettings::kArtistsSearchLimit},
  {TidalSettings::kSettingsGroup, TidalSettings::kAlbumsSearchLimit},
  {TidalSettings::kSettingsGroup, TidalSettings::kSongsSearchLimit},
  {TidalSettings::kSettingsGroup, TidalSettings::kFetchAlbums},
  {TidalSettings::kSettingsGroup, TidalSettings::kStreamUrl},
  {BackendSettings::kSettingsGroup, BackendSettings::kALSAPlugin},
  {BackendSettings::kSettingsGroup, BackendSettings::kBufferDuration},
  {BackendSettings::kSettingsGroup, BackendSettings::kBufferHighWatermark},
  {BackendSettings::kSettingsGroup, BackendSettings::kBufferLowWatermark},
  {BackendSettings::kSettingsGroup, BackendSettings::kDeviceWarmupDuration},
  {BackendSettings::kSettingsGroup, BackendSettings::kRgCompression},
  {BackendSettings::kSettingsGroup, BackendSettings::kRgEnabled},
  {BackendSettings::kSettingsGroup, BackendSettings::kRgFallbackGain},
  {BackendSettings::kSettingsGroup, BackendSettings::kRgMode},
  {BackendSettings::kSettingsGroup, BackendSettings::kRgPreamp},
  {BehaviourSettings::kSettingsGroup, BehaviourSettings::kShowTrayIcon},
  {PlaylistSettings::kSettingsGroup, PlaylistSettings::kEditMetadataInline},
  {SubsonicSettings::kSettingsGroup, SubsonicSettings::kUseAlbumIdForAlbumCovers},
  {JellyfinSettings::kSettingsGroup, JellyfinSettings::kDeviceId},
  {TidalSettings::kSettingsGroup, TidalSettings::kCoverSize},
  {BackendSettings::kSettingsGroup, BackendSettings::kOutput},
  {BackendSettings::kSettingsGroup, BackendSettings::kDevice},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kDownloadAlbumCovers},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kSearchDelay},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kArtistsSearchLimit},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kAlbumsSearchLimit},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kSongsSearchLimit},
  {SpotifySettings::kSettingsGroup, SpotifySettings::kFetchAlbums},
  {SubsonicSettings::kSettingsGroup, SubsonicSettings::kVerifyCertificate},
  {SubsonicSettings::kSettingsGroup, SubsonicSettings::kDownloadAlbumCovers},
  {SubsonicSettings::kSettingsGroup, SubsonicSettings::kServerSideScrobbling},
  {SubsonicSettings::kSettingsGroup, SubsonicSettings::kAuthMethod},
};

QString Key(const char *group, const char *key) {
  return QString::fromLatin1(group) + u'/' + QString::fromLatin1(key);
}

class SettingsMigrationTest : public ::testing::Test {
 protected:
  void SetUp() override { Clear(); }
  void TearDown() override { Clear(); }

  static void Clear() {
    Settings s;
    s.clear();
    s.sync();
  }
};

TEST_F(SettingsMigrationTest, MovesEveryRenamedKey) {

  const QList<RenamedKey> renamed_keys = SettingsMigration::RenamedKeys();
  ASSERT_FALSE(renamed_keys.isEmpty());

  {
    Settings s;
    int i = 0;
    for (const RenamedKey &renamed_key : renamed_keys) {
      s.setValue(Key(renamed_key.group, renamed_key.old_key), ++i);
    }
  }

  SettingsMigration::Migrate();

  Settings s;
  // The stored spelling is checked, since keys are not case sensitive on Windows, so contains() finds the new key for a key only changing case.
  const QStringList all_keys = s.allKeys();
  int i = 0;
  for (const RenamedKey &renamed_key : renamed_keys) {
    ++i;
    EXPECT_FALSE(all_keys.contains(Key(renamed_key.group, renamed_key.old_key), Qt::CaseSensitive)) << renamed_key.old_key;
    EXPECT_EQ(i, s.value(Key(renamed_key.group, renamed_key.new_key)).toInt()) << renamed_key.new_key;
  }

}

// A value already saved with the new key is newer than the value of the old key.
TEST_F(SettingsMigrationTest, KeepsValueOfNewKey) {

  {
    Settings s;
    s.setValue(Key(BackendSettings::kSettingsGroup, "FadeoutDuration"), 1000);
    s.setValue(Key(BackendSettings::kSettingsGroup, BackendSettings::kFadeoutDuration), 2000);
  }

  SettingsMigration::Migrate();

  Settings s;
  EXPECT_FALSE(s.contains(Key(BackendSettings::kSettingsGroup, "FadeoutDuration")));
  EXPECT_EQ(2000, s.value(Key(BackendSettings::kSettingsGroup, BackendSettings::kFadeoutDuration)).toInt());

}

// A key which isn't in the list is left alone, also a key with the same name in another group.
TEST_F(SettingsMigrationTest, LeavesOtherKeysAlone) {

  {
    Settings s;
    s.setValue(Key("Playlist", "Timeout"), 1);
    s.setValue(Key(OSDPrettySettings::kSettingsGroup, "ShowArt"), 2);
  }

  SettingsMigration::Migrate();

  Settings s;
  EXPECT_EQ(1, s.value(Key("Playlist", "Timeout")).toInt());
  EXPECT_EQ(2, s.value(Key(OSDPrettySettings::kSettingsGroup, "ShowArt")).toInt());
  EXPECT_FALSE(s.allKeys().contains(Key("Playlist", "timeout"), Qt::CaseSensitive));

}

TEST_F(SettingsMigrationTest, RemovesObsoleteKeys) {

  const QStringList removed_keys = QStringList() << u"Backend/Engine"_s << u"Backend/engine"_s << u"Qobuz/username"_s << u"Qobuz/password"_s << u"Tidal/oauth"_s << u"Tidal/api_token"_s << u"Tidal/username"_s << u"Tidal/password"_s;
  {
    Settings s;
    for (const QString &key : removed_keys) s.setValue(key, u"value"_s);
    s.setValue(u"Subsonic/username"_s, u"kept"_s);
    s.setValue(u"Tidal/client_id"_s, u"kept"_s);
  }

  SettingsMigration::Migrate();

  Settings s;
  for (const QString &key : removed_keys) EXPECT_FALSE(s.contains(key)) << key.toStdString();
  EXPECT_EQ(u"kept"_s, s.value(u"Subsonic/username"_s).toString());
  EXPECT_EQ(u"kept"_s, s.value(u"Tidal/client_id"_s).toString());

}

// Other output formats are not changed when the old WMA caps are replaced.
TEST_F(SettingsMigrationTest, KeepsOtherLastOutputFormat) {

  Settings().setValue(u"Transcoder/last_output_format"_s, u"audio/x-flac"_s);

  SettingsMigration::Migrate();

  EXPECT_EQ(u"audio/x-flac"_s, Settings().value(u"Transcoder/last_output_format"_s).toString());

}

TEST_F(SettingsMigrationTest, AddsNewScrobblerSourcesOnce) {

  const QString plex = Song::TextForSource(Song::Source::Plex);
  const QString jellyfin = Song::TextForSource(Song::Source::Jellyfin);
  const QString sources_key = Key(ScrobblerSettings::kSettingsGroup, ScrobblerSettings::kSources);
  Settings().setValue(sources_key, QStringList() << u"collection"_s);

  SettingsMigration::Migrate();

  EXPECT_EQ(QStringList() << u"collection"_s << plex << jellyfin, Settings().value(sources_key).toStringList());
  EXPECT_TRUE(Settings().value(Key(ScrobblerSettings::kSettingsGroup, ScrobblerSettings::kPlexSourceMigrationDone)).toBool());
  EXPECT_TRUE(Settings().value(Key(ScrobblerSettings::kSettingsGroup, ScrobblerSettings::kJellyfinSourceMigrationDone)).toBool());

  // The sources deselected after they were added stay deselected.
  Settings().setValue(sources_key, QStringList() << u"collection"_s);

  SettingsMigration::Migrate();

  EXPECT_EQ(QStringList() << u"collection"_s, Settings().value(sources_key).toStringList());

}

// Without saved sources every source is used, so a list must not be created.
TEST_F(SettingsMigrationTest, DoesNotCreateScrobblerSources) {

  SettingsMigration::Migrate();

  EXPECT_FALSE(Settings().contains(Key(ScrobblerSettings::kSettingsGroup, ScrobblerSettings::kSources)));
  EXPECT_TRUE(Settings().value(Key(ScrobblerSettings::kSettingsGroup, ScrobblerSettings::kPlexSourceMigrationDone)).toBool());

}

// Regression test: only some of the keys read by the code were checked against the keys the values are moved to, and only the key, not the group.
// Every renamed key must be moved to a key read by the code, and every key read by the code must be in the list once, so a key added to only one of them, or with a typo, fails.
TEST(SettingsKeysTest, RenamedKeysAreTheKeysReadByCode) {

  const QList<RenamedKey> renamed_keys = SettingsMigration::RenamedKeys();

  QStringList migrated_keys;
  for (const RenamedKey &renamed_key : renamed_keys) {
    const QString key = Key(renamed_key.group, renamed_key.new_key);
    EXPECT_FALSE(migrated_keys.contains(key)) << "Moved to twice: " << key.toStdString();
    migrated_keys << key;
    EXPECT_TRUE(QString::fromLatin1(renamed_key.new_key).contains(QRegularExpression(u"^[a-z0-9]+(_[a-z0-9]+)*$"_s))) << renamed_key.new_key;
  }

  QStringList keys_read_by_code;
  for (const SettingsKey &settings_key : kKeysReadByCode) {
    keys_read_by_code << Key(settings_key.group, settings_key.key);
  }

  for (const QString &key : std::as_const(migrated_keys)) {
    EXPECT_TRUE(keys_read_by_code.contains(key)) << "Moved to a key not read by the code: " << key.toStdString();
  }
  for (const QString &key : std::as_const(keys_read_by_code)) {
    EXPECT_TRUE(migrated_keys.contains(key)) << "Not moved to: " << key.toStdString();
  }

}

}  // namespace
