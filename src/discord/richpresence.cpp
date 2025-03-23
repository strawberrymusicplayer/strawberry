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

#include "richpresence.h"

#include "core/logging.h"
#include "core/player.h"
#include "core/settings.h"
#include "engine/enginebase.h"
#include "constants/notificationssettings.h"

#include <discord_rpc.h>

namespace {

constexpr char kDiscordApplicationId[] = "1352351827206733974";
constexpr char kStrawberryIconResourceName[] = "embedded_cover";
constexpr char kStrawberryIconDescription[] = "Strawberry Music Player";
constexpr qint64 kDiscordPresenceUpdateRateLimitMs = 2000;

}  // namespace

namespace discord {

RichPresence::RichPresence(const SharedPtr<Player> player,
                           const SharedPtr<PlaylistManager> playlist_manager,
                           QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      activity_({ {}, {}, {}, 0, 0, 0 }),
      send_presence_timestamp_(0),
      is_enabled_(false) {
  Discord_Initialize(kDiscordApplicationId, nullptr, true, nullptr);

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &RichPresence::EngineStateChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &RichPresence::CurrentSongChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &RichPresence::Seeked);
}

RichPresence::~RichPresence() {
  Discord_Shutdown();
}

void RichPresence::EngineStateChanged(EngineBase::State newState) {
  if (newState == EngineBase::State::Playing) {
    SetTimestamp(player_->engine()->position_nanosec() / 1e3);
    SendPresenceUpdate();
  }
  else
    Discord_ClearPresence();
}

void RichPresence::CurrentSongChanged(const Song &song) {
  SetTimestamp(0);
  activity_.length_secs = song.length_nanosec() / 1e9;
  activity_.title = song.title();
  activity_.artist = song.artist();
  activity_.album = song.album();

  SendPresenceUpdate();
}

void RichPresence::CheckEnabled() {
  Settings s;
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);

  is_enabled_ = s.value(DiscordRPCSettings::kEnabled).toBool();

  s.endGroup();

  if (!is_enabled_)
    Discord_ClearPresence();
}

void RichPresence::SendPresenceUpdate() {
  CheckEnabled();
  if (!is_enabled_)
    return;

  qint64 nowTimestamp = QDateTime::currentMSecsSinceEpoch();
  if (nowTimestamp - send_presence_timestamp_ < kDiscordPresenceUpdateRateLimitMs) {
    qLog(Debug) << "Not sending rich presence due to rate limit of " << kDiscordPresenceUpdateRateLimitMs << "ms";
    return;
  }

  send_presence_timestamp_ = nowTimestamp;

  ::DiscordRichPresence presence_data;
  memset(&presence_data, 0, sizeof(presence_data));
  QByteArray title;
  QByteArray artist;
  QByteArray album;

  presence_data.type = 2 /* Listening */;
  presence_data.largeImageKey = kStrawberryIconResourceName;
  presence_data.smallImageKey = kStrawberryIconResourceName;
  presence_data.smallImageText = kStrawberryIconDescription;
  presence_data.instance = false;

  if (!activity_.artist.isEmpty()) {
    artist = activity_.artist.toUtf8();
    artist.prepend(tr("by ").toUtf8());
    presence_data.state = artist.constData();
  }

  if (!activity_.album.isEmpty() && !(activity_.album == activity_.title)) {
    album = activity_.album.toUtf8();
    album.prepend(tr("on ").toUtf8());
    presence_data.largeImageText = album.constData();
  }

  title = activity_.title.toUtf8();
  presence_data.details = title.constData();

  const qint64 startTimestamp = activity_.start_timestamp - activity_.seek_secs;

  presence_data.startTimestamp = startTimestamp;
  presence_data.endTimestamp = startTimestamp + activity_.length_secs;

  Discord_UpdatePresence(&presence_data);
}

void RichPresence::SetTimestamp(const qint64 seekMicroseconds) {
  activity_.start_timestamp = time(nullptr);
  activity_.seek_secs = seekMicroseconds / 1e6;
}

void RichPresence::Seeked(const qint64 microseconds) {
  SetTimestamp(microseconds);
  SendPresenceUpdate();
}

}  // namespace discord
