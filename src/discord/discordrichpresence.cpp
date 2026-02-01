/*
 * Strawberry Music Player
 * Copyright 2025-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QDateTime>

#include "constants/timeconstants.h"
#include "constants/notificationssettings.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlistmanager.h"
#include "discordrichpresence.h"
#include "discordrpc.h"
#include "discordpresence.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kDiscordApplicationId[] = "1352351827206733974";
constexpr char kStrawberryIconResourceName[] = "embedded_cover";
constexpr char kStrawberryIconDescription[] = "Strawberry Music Player";
}  // namespace

DiscordRichPresence::DiscordRichPresence(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      discord_rpc_(nullptr),
      initialized_(false),
      status_display_type_(0) {

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &DiscordRichPresence::EngineStateChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &DiscordRichPresence::CurrentSongChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &DiscordRichPresence::Seeked);

  ReloadSettings();

}

DiscordRichPresence::~DiscordRichPresence() {

  if (discord_rpc_) {
    discord_rpc_->Shutdown();
    delete discord_rpc_;
    discord_rpc_ = nullptr;
  }

}

void DiscordRichPresence::ReloadSettings() {

  Settings s;
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  const bool enabled = s.value(DiscordRPCSettings::kEnabled, false).toBool();
  status_display_type_ = s.value(DiscordRPCSettings::kStatusDisplayType, static_cast<int>(DiscordRPCSettings::StatusDisplayType::App)).toInt();
  s.endGroup();

  if (enabled && !discord_rpc_) {
    discord_rpc_ = new DiscordRPC(QString::fromLatin1(kDiscordApplicationId), this);
    discord_rpc_->Initialize();
    initialized_ = true;
  }
  else if (!enabled && discord_rpc_) {
    discord_rpc_->ClearPresence();
    discord_rpc_->Shutdown();
    delete discord_rpc_;
    discord_rpc_ = nullptr;
    initialized_ = false;
  }

}

void DiscordRichPresence::EngineStateChanged(const EngineBase::State state) {

  if (!discord_rpc_) return;

  if (state == EngineBase::State::Playing) {
    SetTimestamp(player_->engine()->position_nanosec() / kNsecPerSec);
    SendPresenceUpdate();
  }
  else {
    discord_rpc_->ClearPresence();
  }

}

void DiscordRichPresence::CurrentSongChanged(const Song &song) {

  if (!discord_rpc_) return;

  SetTimestamp(0LL);
  activity_.length_secs = song.length_nanosec() / kNsecPerSec;
  activity_.title = song.PrettyTitle();
  activity_.artist = song.artist();
  activity_.album = song.album();

  SendPresenceUpdate();

}

void DiscordRichPresence::SendPresenceUpdate() {

  if (!discord_rpc_) return;

  DiscordPresence presence;

  // Listening to
  presence.type = 2;
  presence.status_display_type = status_display_type_;

  presence.large_image_key = QString::fromLatin1(kStrawberryIconResourceName);
  presence.small_image_key = QString::fromLatin1(kStrawberryIconResourceName);
  presence.small_image_text = QString::fromLatin1(kStrawberryIconDescription);
  presence.instance = false;

  if (!activity_.artist.isEmpty()) {
    QString artist = activity_.artist;
    if (artist.length() < 2) {  // Discord activity 2 char min. fix
      artist.append(" "_L1);
    }
    presence.state = artist;
  }

  if (!activity_.album.isEmpty()) {
    presence.large_image_text = tr("on ") + activity_.album;
  }

  presence.details = activity_.title;

  const qint64 start_timestamp = activity_.start_timestamp - activity_.seek_secs;
  presence.start_timestamp = start_timestamp;
  presence.end_timestamp = start_timestamp + activity_.length_secs;

  discord_rpc_->UpdatePresence(presence);

}

void DiscordRichPresence::SetTimestamp(const qint64 seconds) {

  activity_.start_timestamp = QDateTime::currentSecsSinceEpoch();
  activity_.seek_secs = seconds;

}

void DiscordRichPresence::Seeked(const qint64 seek_microseconds) {

  if (!discord_rpc_) return;

  SetTimestamp(seek_microseconds / 1000000LL);
  SendPresenceUpdate();

}
