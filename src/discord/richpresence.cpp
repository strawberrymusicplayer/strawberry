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
                           SharedPtr<CoverProviders> cover_providers,
                           SharedPtr<NetworkAccessManager> network,
                           QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      cover_fetcher_(new AlbumCoverFetcher(cover_providers, network)),
      initialized_(false) {

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &RichPresence::EngineStateChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &RichPresence::CurrentSongChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &RichPresence::Seeked);
  QObject::connect(&*cover_fetcher_, &AlbumCoverFetcher::SearchFinished, this, &RichPresence::SearchFinished);

  ReloadSettings();

}

RichPresence::~RichPresence() {

  if (initialized_) {
    Discord_Shutdown();
  }

}

void RichPresence::SearchFinished(const quint64 request_id,
                                  const CoverProviderSearchResults &results,
                                  const CoverSearchStatistics &statistics) {

  if (!initialized_ || results.size() < 1)
    return;

  Q_UNUSED(request_id);
  Q_UNUSED(statistics);

  // find the best result
  qsizetype which = 0;
  float whichscore = 0.0;

  for (qsizetype i = 0; i < results.size(); i++) {
    // Discord Rich Presence only supports up to 128 chars in an image
    // resource. Ignore any URLs with more than 128 UTF-8 bytes.
    if (results[i].image_url.toString().toUtf8().size() > 128)
      continue;

    float s = results[i].score();
    if (s > whichscore) {
      which = i;
      whichscore = s;
    }
  }

  activity_.large_image = results[which].image_url.toString();

  SendPresenceUpdate();

}

void RichPresence::ReloadSettings() {

  Settings s;
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  const bool enabled = s.value(DiscordRPCSettings::kEnabled, false).toBool();
  s.endGroup();

  if (enabled && !initialized_) {
    Discord_Initialize(kDiscordApplicationId, nullptr, 1);
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

  QUrl art_url = song.art_automatic();
  if (art_url.isValid() && !art_url.isLocalFile()) {
    activity_.large_image = art_url.toString();
    SendPresenceUpdate();
  } else {
    activity_.large_image.clear();
    // send unfilled presence update NOW, so we don't race ;)
    SendPresenceUpdate();
    cover_fetcher_->SearchForCovers(song.artist(), song.album(), song.title());
  }

}

void RichPresence::SendPresenceUpdate() {

  if (!initialized_) return;

  ::DiscordRichPresence presence_data{};
  memset(&presence_data, 0, sizeof(presence_data));
  presence_data.type = 2; // Listening
  presence_data.smallImageKey = kStrawberryIconResourceName;
  presence_data.smallImageText = kStrawberryIconDescription;
  presence_data.instance = 0;

  QByteArray large_image;
  if (!activity_.large_image.isEmpty()) {
    large_image = activity_.large_image.toUtf8();
    presence_data.largeImageKey = large_image.constData();
  } else {
    presence_data.largeImageKey = kStrawberryIconResourceName;
  }

  QByteArray artist;
  if (!activity_.artist.isEmpty()) {
    artist = activity_.artist.toUtf8();
    artist.prepend(tr("by ").toUtf8());
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
