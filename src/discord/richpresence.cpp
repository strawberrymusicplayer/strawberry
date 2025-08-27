/*
 * Strawberry Music Player
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

#include <discord_rpc.h>

#include <QByteArray>
#include <QString>
#include <QDateTime>

#include "constants/timeconstants.h"
#include "constants/notificationssettings.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlistmanager.h"
#include "richpresence.h"

namespace {
constexpr char kDiscordApplicationId[] = "1352351827206733974";
constexpr char kStrawberryIconResourceName[] = "embedded_cover";
constexpr char kStrawberryIconDescription[] = "Strawberry Music Player";
}  // namespace

namespace discord {

RichPresence::RichPresence(const SharedPtr<Player> player,
                           const SharedPtr<PlaylistManager> playlist_manager,
                           QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      initialized_(false),
      status_display_type_(0) {

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &RichPresence::EngineStateChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &RichPresence::CurrentSongChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &RichPresence::Seeked);

  ReloadSettings();

}

RichPresence::~RichPresence() {

  if (initialized_) {
    Discord_Shutdown();
  }

}

void RichPresence::ReloadSettings() {

  Settings s;
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  const bool enabled = s.value(DiscordRPCSettings::kEnabled, false).toBool();
  status_display_type_ = s.value(DiscordRPCSettings::kStatusDisplayType, static_cast<int>(DiscordRPCSettings::StatusDisplayType::App)).toInt();
  s.endGroup();

  if (enabled && !initialized_) {
    Discord_Initialize(kDiscordApplicationId, nullptr, 0);
    initialized_ = true;
  }
  else if (!enabled && initialized_) {
    Discord_ClearPresence();
    Discord_Shutdown();
    initialized_ = false;
  }

}

void RichPresence::EngineStateChanged(const EngineBase::State state) {

  if (!initialized_) return;

  if (state == EngineBase::State::Playing) {
    SetTimestamp(player_->engine()->position_nanosec() / kNsecPerSec);
    SendPresenceUpdate();
  }
  else {
    Discord_ClearPresence();
  }

}

void RichPresence::CurrentSongChanged(const Song &song) {

  if (!initialized_) return;

  SetTimestamp(0LL);
  activity_.length_secs = song.length_nanosec() / kNsecPerSec;
  activity_.title = song.title();
  activity_.artist = song.artist();
  activity_.album = song.album();

  SendPresenceUpdate();

}

void RichPresence::SendPresenceUpdate() {

  if (!initialized_) return;

  ::DiscordRichPresence presence_data{};
  memset(&presence_data, 0, sizeof(presence_data));

  // Listening to
  presence_data.type = 2;
  presence_data.status_display_type = status_display_type_;

  presence_data.largeImageKey = kStrawberryIconResourceName;
  presence_data.smallImageKey = kStrawberryIconResourceName;
  presence_data.smallImageText = kStrawberryIconDescription;
  presence_data.instance = 0;

  QByteArray artist;
  if (!activity_.artist.isEmpty()) {
    artist = activity_.artist.toUtf8();
    if (artist.size() < 2) { // Discord activity 2 char min. fix
      artist.append(" ");
    }
    presence_data.state = artist.constData();
  }

  QByteArray album;
  if (!activity_.album.isEmpty()) {
    album = activity_.album.toUtf8();
    album.prepend(tr("on ").toUtf8());
    presence_data.largeImageText = album.constData();
  }

  const QByteArray title = activity_.title.toUtf8();
  presence_data.details = title.constData();

  const qint64 start_timestamp = activity_.start_timestamp - activity_.seek_secs;
  presence_data.startTimestamp = start_timestamp;
  presence_data.endTimestamp = start_timestamp + activity_.length_secs;

  Discord_UpdatePresence(&presence_data);

}

void RichPresence::SetTimestamp(const qint64 seconds) {

  activity_.start_timestamp = QDateTime::currentSecsSinceEpoch();
  activity_.seek_secs = seconds;

}

void RichPresence::Seeked(const qint64 seek_microseconds) {

  if (!initialized_) return;

  SetTimestamp(seek_microseconds / 1000LL);
  SendPresenceUpdate();

}

}  // namespace discord
