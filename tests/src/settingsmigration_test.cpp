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

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "core/settings.h"
#include "core/song.h"
#include "core/settingsmigration.h"
#include "constants/appearancesettings.h"
#include "constants/backendsettings.h"
#include "constants/behavioursettings.h"
#include "constants/contextsettings.h"
#include "constants/notificationssettings.h"
#include "constants/jellyfinsettings.h"
#include "constants/plexsettings.h"
#include "constants/playlistsettings.h"
#include "constants/scrobblersettings.h"
#include "constants/subsonicsettings.h"
#include "constants/tidalsettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {

struct RenamedKey {
  const char *group;
  const char *old_key;
  const char *new_key;
};

const RenamedKey kRenamedKeys[] = {
  {"Appearance", "use-custom-color-set", "use_custom_color_set"},
  {"Appearance", "color-window", "color_window"},
  {"Appearance", "color-window-text", "color_window_text"},
  {"Appearance", "color-base", "color_base"},
  {"Appearance", "color-alternate-base", "color_alternate_base"},
  {"Appearance", "color-tooltip-base", "color_tooltip_base"},
  {"Appearance", "color-tooltip-text", "color_tooltip_text"},
  {"Appearance", "color-text", "color_text"},
  {"Appearance", "color-button", "color_button"},
  {"Appearance", "color-button-text", "color_button_text"},
  {"Appearance", "color-bright-text", "color_bright_text"},
  {"Appearance", "color-placeholder-text", "color_placeholder_text"},
  {"Backend", "FadeoutEnabled", "fadeout_enabled"},
  {"Backend", "CrossfadeEnabled", "crossfade_enabled"},
  {"Backend", "AutoCrossfadeEnabled", "auto_crossfade_enabled"},
  {"Backend", "NoCrossfadeSameAlbum", "no_crossfade_same_album"},
  {"Backend", "FadeoutPauseEnabled", "fadeout_pause_enabled"},
  {"Backend", "FadeoutDuration", "fadeout_duration"},
  {"Backend", "FadeoutPauseDuration", "fadeout_pause_duration"},
  {"Context", "TitleFmt", "title_fmt"},
  {"Context", "SummaryFmt", "summary_fmt"},
  {"Context", "AlbumEnable", "album_enable"},
  {"Context", "TechnicalDataEnable", "technical_data_enable"},
  {"Context", "SongLyricsEnable", "song_lyrics_enable"},
  {"Context", "SyncedLyricsEnable", "synced_lyrics_enable"},
  {"Context", "SearchCoverEnable", "search_cover_enable"},
  {"Context", "SearchLyricsEnable", "search_lyrics_enable"},
  {"OSD", "Behaviour", "behaviour"},
  {"OSD", "Timeout", "timeout"},
  {"OSD", "ShowOnVolumeChange", "show_on_volume_change"},
  {"OSD", "ShowOnPlayModeChange", "show_on_play_mode_change"},
  {"OSD", "ShowOnPausePlayback", "show_on_pause_playback"},
  {"OSD", "ShowOnResumePlayback", "show_on_resume_playback"},
  {"OSD", "ShowOnStreamTitleChange", "show_on_stream_title_change"},
  {"OSD", "ShowArt", "show_art"},
  {"OSD", "CustomTextEnabled", "custom_text_enabled"},
  {"OSD", "CustomText1", "custom_text1"},
  {"OSD", "CustomText2", "custom_text2"},
  {"DiscordRPC", "StatusDisplayType", "status_display_type"},
  {"AlbumCoverExport", "fileName", "file_name"},
  {"AlbumCoverExport", "forceSize", "force_size"},
  {"Behaviour", "keeprunning", "keep_running"},
  {"Behaviour", "resumeplayback", "resume_playback"},
  {"Behaviour", "startupbehaviour", "startup_behaviour"},
  {"Jellyfin", "verifycertificate", "verify_certificate"},
  {"Jellyfin", "downloadalbumcovers", "download_albumcovers"},
  {"Jellyfin", "serversidescrobbling", "serverside_scrobbling"},
  {"Plex", "verifycertificate", "verify_certificate"},
  {"Plex", "downloadalbumcovers", "download_albumcovers"},
  {"Plex", "clientid", "client_id"},
  {"Qobuz", "downloadalbumcovers", "download_albumcovers"},
  {"Qobuz", "searchdelay", "search_delay"},
  {"Qobuz", "artistssearchlimit", "artists_search_limit"},
  {"Qobuz", "albumssearchlimit", "albums_search_limit"},
  {"Qobuz", "songssearchlimit", "songs_search_limit"},
  {"Tidal", "downloadalbumcovers", "download_albumcovers"},
  {"Tidal", "searchdelay", "search_delay"},
  {"Tidal", "artistssearchlimit", "artists_search_limit"},
  {"Tidal", "albumssearchlimit", "albums_search_limit"},
  {"Tidal", "songssearchlimit", "songs_search_limit"},
  {"Tidal", "fetchalbums", "fetch_albums"},
  {"Tidal", "streamurl", "stream_url"},
  {"Backend", "alsaplugin", "alsa_plugin"},
  {"Backend", "bufferduration", "buffer_duration"},
  {"Backend", "bufferhighwatermark", "buffer_high_watermark"},
  {"Backend", "bufferlowwatermark", "buffer_low_watermark"},
  {"Backend", "devicewarmupduration", "device_warmup_duration"},
  {"Backend", "rgcompression", "rg_compression"},
  {"Backend", "rgenabled", "rg_enabled"},
  {"Backend", "rgfallbackgain", "rg_fallback_gain"},
  {"Backend", "rgmode", "rg_mode"},
  {"Backend", "rgpreamp", "rg_preamp"},
  {"Behaviour", "showtrayicon", "show_tray_icon"},
  {"Playlist", "editmetadatainline", "edit_metadata_inline"},
  {"Subsonic", "usealbumidforalbumcovers", "use_album_id_for_albumcovers"},
  {"Jellyfin", "deviceid", "device_id"},
  {"Tidal", "coversize", "cover_size"},
  {"Backend", "Output", "output"},
  {"Backend", "Device", "device"},
  {"Spotify", "downloadalbumcovers", "download_albumcovers"},
  {"Spotify", "searchdelay", "search_delay"},
  {"Spotify", "artistssearchlimit", "artists_search_limit"},
  {"Spotify", "albumssearchlimit", "albums_search_limit"},
  {"Spotify", "songssearchlimit", "songs_search_limit"},
  {"Spotify", "fetchalbums", "fetch_albums"},
  {"Subsonic", "verifycertificate", "verify_certificate"},
  {"Subsonic", "downloadalbumcovers", "download_albumcovers"},
  {"Subsonic", "serversidescrobbling", "serverside_scrobbling"},
  {"Subsonic", "authmethod", "auth_method"},
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

  {
    Settings s;
    int i = 0;
    for (const RenamedKey &renamed_key : kRenamedKeys) {
      s.setValue(Key(renamed_key.group, renamed_key.old_key), ++i);
    }
  }

  SettingsMigration::Migrate();

  Settings s;
  // The stored spelling is checked, since keys are not case sensitive on Windows, so contains() finds the new key for a key only changing case.
  const QStringList all_keys = s.allKeys();
  int i = 0;
  for (const RenamedKey &renamed_key : kRenamedKeys) {
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

// The keys read by the code are the keys the values are moved to.
TEST(SettingsKeysTest, ConstantsAreTheMigratedKeys) {

  const char *const keys[] = {AppearanceSettings::kUseCustomColorSet, AppearanceSettings::kColorWindow, AppearanceSettings::kColorPlaceholderText, BackendSettings::kFadeoutEnabled, BackendSettings::kNoCrossfadeSameAlbum, ContextSettings::kSettingsTitleFmt, ContextSettings::kSearchLyrics, OSDSettings::kType, OSDSettings::kTimeout, OSDSettings::kCustomText1, DiscordRPCSettings::kStatusDisplayType, BehaviourSettings::kKeepRunning, BehaviourSettings::kStartupBehaviour, PlexSettings::kClientId, PlexSettings::kVerifyCertificate, SubsonicSettings::kAuthMethod, SubsonicSettings::kServerSideScrobbling, TidalSettings::kStreamUrl, TidalSettings::kSearchDelay, TidalSettings::kArtistsSearchLimit, TidalSettings::kFetchAlbums, TidalSettings::kDownloadAlbumCovers, BackendSettings::kALSAPlugin, BackendSettings::kBufferHighWatermark, BackendSettings::kDeviceWarmupDuration, BackendSettings::kRgFallbackGain, BehaviourSettings::kShowTrayIcon, PlaylistSettings::kEditMetadataInline, SubsonicSettings::kUseAlbumIdForAlbumCovers, JellyfinSettings::kDeviceId, TidalSettings::kCoverSize, BackendSettings::kOutput, BackendSettings::kDevice};
  for (const char *key : keys) {
    bool found = false;
    for (const RenamedKey &renamed_key : kRenamedKeys) {
      if (QString::fromLatin1(renamed_key.new_key) == QString::fromLatin1(key)) found = true;
    }
    EXPECT_TRUE(found) << key;
    EXPECT_TRUE(QString::fromLatin1(key).contains(QRegularExpression(u"^[a-z0-9]+(_[a-z0-9]+)*$"_s))) << key;
  }

}

}  // namespace
