/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef WINDOWSMEDIACONTROLLER_H
#define WINDOWSMEDIACONTROLLER_H

#include "config.h"

#include <windows.h>

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "covermanager/albumcoverloaderresult.h"

class Player;
class PlaylistManager;
class CurrentAlbumCoverLoader;
class Song;

class WindowsMediaController : public QObject {
  Q_OBJECT

 public:
  explicit WindowsMediaController(HWND hwnd,
                                   const SharedPtr<Player> player,
                                   const SharedPtr<PlaylistManager> playlist_manager,
                                   const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                   QObject *parent = nullptr);
  ~WindowsMediaController() override;

 private Q_SLOTS:
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result = AlbumCoverLoaderResult());
  void EngineStateChanged(EngineBase::State newState);
  void CurrentSongChanged(const Song &song);

 private:
  void UpdatePlaybackStatus(EngineBase::State state);
  void UpdateMetadata(const Song &song, const QUrl &art_url);
  void SetupButtonHandlers();

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  void *smtc_;  // Pointer to SystemMediaTransportControls (opaque to avoid WinRT headers in public header)
  QString current_song_art_url_;
};

#endif  // WINDOWSMEDIACONTROLLER_H
