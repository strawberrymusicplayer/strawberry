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

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "settings.h"
#include "settingsmigration.h"
#include "song.h"
#include "constants/scrobblersettings.h"
#include "transcoder/transcoder.h"

using namespace Qt::Literals::StringLiterals;

using SettingsMigration::RenamedKey;

namespace {

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

// Keys which are not used anymore.
const char *const kRemovedKeys[] = {
  "Backend/Engine",
  "Backend/engine",
  "Qobuz/username",
  "Qobuz/password",
  "Tidal/oauth",
  "Tidal/api_token",
  "Tidal/username",
  "Tidal/password",
  "Transcoder/faac/profile",
};

void MigrateRenamedKeys(Settings &s) {

  for (const RenamedKey &renamed_key : kRenamedKeys) {
    const QString old_key = QString::fromLatin1(renamed_key.group) + u'/' + QString::fromLatin1(renamed_key.old_key);
    if (!s.contains(old_key)) continue;
    const QString new_key = QString::fromLatin1(renamed_key.group) + u'/' + QString::fromLatin1(renamed_key.new_key);
    const bool case_only = old_key.compare(new_key, Qt::CaseInsensitive) == 0;
    if (case_only) {
      // Keys are not case sensitive on Windows, so contains() also finds a key which is already migrated, the key is only migrated when it's stored with the old spelling.
      s.beginGroup(QString::fromLatin1(renamed_key.group));
      const bool has_old_spelling = s.childKeys().contains(QString::fromLatin1(renamed_key.old_key), Qt::CaseSensitive);
      s.endGroup();
      if (!has_old_spelling) continue;
    }
    const QVariant value = s.value(old_key);
    // The old key is removed first, since keys are not case sensitive on Windows, so a key only changing case would be removed after it was written.
    s.remove(old_key);
#ifdef Q_OS_WIN
    // With the INI format, QSettings keeps the old spelling for the pending change when the new key is written before the removal is synced.
    // The registry removes the key immediately, and keys are case sensitive on other systems, so the removal is only synced separately when needed.
    if (case_only && s.format() == QSettings::IniFormat) s.sync();
#endif
    if (!s.contains(new_key)) s.setValue(new_key, value);
  }

}

void MigrateRemovedKeys(Settings &s) {

  for (const char *key : kRemovedKeys) {
    s.remove(QString::fromLatin1(key));
  }

}

void MigrateTranscoder(Settings &s) {


  // The WMA options were saved for the GStreamer 0.10 encoder "ffenc_wmav2", which is "avenc_wmav2" now.
  // Settings which are already saved for the new encoder are kept, they are newer.
  const QString legacy_prefix = u"ffenc_wmav2"_s;
  const QString new_prefix = u"avenc_wmav2"_s;

  s.beginGroup("Transcoder"_L1);
  const QStringList groups = s.childGroups();
  for (const QString &group : groups) {
    if (!group.startsWith(legacy_prefix)) continue;
    const QString new_group = new_prefix + group.mid(legacy_prefix.length());
    s.beginGroup(group);
    const QStringList keys = s.childKeys();
    QVariantMap values;
    for (const QString &key : keys) {
      values.insert(key, s.value(key));
    }
    s.endGroup();
    for (QVariantMap::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
      const QString new_key = new_group + u'/' + it.key();
      if (!s.contains(new_key)) {
        s.setValue(new_key, it.value());
      }
    }
    s.remove(group);
  }

  // The transcode dialog saves the output format as the caps of the codec, the WMA caps have the version now, so the old caps would not match any format.
  if (s.value("last_output_format"_L1).toString() == "audio/x-wma"_L1) {
    s.setValue("last_output_format"_L1, Transcoder::PresetForFileType(Song::FileType::ASF).codec_mimetype_);
  }

  s.endGroup();


}

// Plex and Jellyfin are added once to the sources saved before they existed, so they are not added again when they have been deselected.
void MigrateScrobblerSources(Settings &s) {

  s.beginGroup(ScrobblerSettings::kSettingsGroup);

  const bool plex_source_migration_done = s.value(ScrobblerSettings::kPlexSourceMigrationDone, false).toBool();
  const bool jellyfin_source_migration_done = s.value(ScrobblerSettings::kJellyfinSourceMigrationDone, false).toBool();

  QStringList sources = s.value(ScrobblerSettings::kSources).toStringList();
  if (!sources.isEmpty()) {
    const qsizetype sources_count = sources.count();
    if (!plex_source_migration_done && !sources.contains(Song::TextForSource(Song::Source::Plex))) {
      sources << Song::TextForSource(Song::Source::Plex);
    }
    if (!jellyfin_source_migration_done && !sources.contains(Song::TextForSource(Song::Source::Jellyfin))) {
      sources << Song::TextForSource(Song::Source::Jellyfin);
    }
    if (sources.count() != sources_count) {
      s.setValue(ScrobblerSettings::kSources, sources);
    }
  }

  if (!plex_source_migration_done) s.setValue(ScrobblerSettings::kPlexSourceMigrationDone, true);
  if (!jellyfin_source_migration_done) s.setValue(ScrobblerSettings::kJellyfinSourceMigrationDone, true);

  s.endGroup();

}

}  // namespace

namespace SettingsMigration {

void Migrate() {

  Settings s;
  MigrateRenamedKeys(s);
  MigrateRemovedKeys(s);
  MigrateTranscoder(s);
  MigrateScrobblerSources(s);

}

QList<RenamedKey> RenamedKeys() {

  QList<RenamedKey> renamed_keys;
  for (const RenamedKey &renamed_key : kRenamedKeys) {
    renamed_keys << renamed_key;
  }
  return renamed_keys;

}

}  // namespace SettingsMigration
