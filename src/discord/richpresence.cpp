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
constexpr qint64 kDiscordPresenceUpdateRateLimitMs = 2000;
}  // namespace

using namespace discord_rpc;

namespace discord {

RichPresence::RichPresence(const SharedPtr<Player> player,
                           const SharedPtr<PlaylistManager> playlist_manager,
                           QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      activity_({ {}, {}, {}, 0, 0, 0 }),
      send_presence_timestamp_(0),
      enabled_(false) {

  Discord_Initialize(kDiscordApplicationId, nullptr, 1, nullptr);

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &RichPresence::EngineStateChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &RichPresence::CurrentSongChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &RichPresence::Seeked);

  ReloadSettings();

}

RichPresence::~RichPresence() {
  Discord_Shutdown();
}

void RichPresence::ReloadSettings() {

  Settings s;
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  const bool enabled = s.value(DiscordRPCSettings::kEnabled, false).toBool();
  s.endGroup();

  if (enabled_ && !enabled) {
    Discord_ClearPresence();
  }

  enabled_ = enabled;

}

void RichPresence::EngineStateChanged(const EngineBase::State state) {

  if (state == EngineBase::State::Playing) {
    SetTimestamp(player_->engine()->position_nanosec() / kNsecPerSec);
    SendPresenceUpdate();
  }
  else {
    Discord_ClearPresence();
  }

}

void RichPresence::CurrentSongChanged(const Song &song) {

  SetTimestamp(0LL);
  activity_.length_secs = song.length_nanosec() / kNsecPerSec;
  activity_.title = song.title();
  activity_.artist = song.artist();
  activity_.album = song.album();

  SendPresenceUpdate();

}

void RichPresence::SendPresenceUpdate() {

  if (!enabled_) {
    return;
  }

  const qint64 current_timestamp = QDateTime::currentMSecsSinceEpoch();
  if (current_timestamp - send_presence_timestamp_ < kDiscordPresenceUpdateRateLimitMs) {
    qLog(Info) << "Not sending rich presence due to rate limit of" << kDiscordPresenceUpdateRateLimitMs << "ms";
    return;
  }

  send_presence_timestamp_ = current_timestamp;

  ::DiscordRichPresence presence_data{};
  memset(&presence_data, 0, sizeof(presence_data));
  presence_data.type = 2; // Listening
  presence_data.largeImageKey = kStrawberryIconResourceName;
  presence_data.smallImageKey = kStrawberryIconResourceName;
  presence_data.smallImageText = kStrawberryIconDescription;
  presence_data.instance = 0;

  QByteArray artist;
  QByteArray album;
  QByteArray title;
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
  SetTimestamp(seek_microseconds / 1000LL);
  SendPresenceUpdate();
}

}  // namespace discord
