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

#ifndef ANDROIDMEDIASESSION_H
#define ANDROIDMEDIASESSION_H

#include <QObject>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"

class Player;
class PlaylistManager;
class CurrentAlbumCoverLoader;
class Song;
class AlbumCoverLoaderResult;

// Android counterpart of Mpris2: publishes the current song and playback state to an Android media session and notification (MediaSessionBridge.java).
// While a song is playing or paused, this also runs a foreground service, so Android keeps playing in the background.
class AndroidMediaSession : public QObject {
  Q_OBJECT

 public:
  explicit AndroidMediaSession(const SharedPtr<Player> player,
                               const SharedPtr<PlaylistManager> playlist_manager,
                               const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                               QObject *parent = nullptr);

 private Q_SLOTS:
  void EngineStateChanged(const EngineBase::State state);
  void Seeked();
  void CurrentSongChanged(const Song &song);
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result);

 private:
  void UpdatePlaybackState(const EngineBase::State state);

  const SharedPtr<Player> player_;
};

#endif  // ANDROIDMEDIASESSION_H
