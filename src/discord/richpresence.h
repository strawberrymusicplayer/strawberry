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

#ifndef RICHPRESENCE_H
#define RICHPRESENCE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/player.h"
#include "engine/enginebase.h"

class Song;
class Player;
class PlaylistManager;

namespace discord {

class RichPresence : public QObject {
  Q_OBJECT

 public:
  explicit RichPresence(const SharedPtr<Player> player,
                        const SharedPtr<PlaylistManager> playlist_manager,
                        QObject *parent = nullptr);
  ~RichPresence();

  void ReloadSettings();
  void Stop();

 private Q_SLOTS:
  void EngineStateChanged(const EngineBase::State state);
  void CurrentSongChanged(const Song &song);
  void Seeked(const qint64 seek_microseconds);

 private:
  void SendPresenceUpdate();
  void SetTimestamp(const qint64 seconds = 0);

  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;

  class Activity {
   public:
    explicit Activity() : start_timestamp(0), length_secs(0), seek_secs(0) {}
    QString title;
    QString artist;
    QString album;
    qint64 start_timestamp;
    qint64 length_secs;
    qint64 seek_secs;
  };
  Activity activity_;
  bool initialized_;
  int status_display_type_;
};

}  // namespace discord

#endif  // RICHPRESENCE_H
