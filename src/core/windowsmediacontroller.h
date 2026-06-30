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

#ifndef WINDOWSMEDIACONTROLLER_H
#define WINDOWSMEDIACONTROLLER_H

#include "config.h"

#include <windows.h>

#include <QObject>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "covermanager/albumcoverloaderresult.h"

class Player;
class CurrentAlbumCoverLoader;
class Song;

class WindowsMediaController : public QObject {
  Q_OBJECT

 public:
  explicit WindowsMediaController(const HWND hwnd, const SharedPtr<Player> player, const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader, QObject *parent = nullptr);
  ~WindowsMediaController() override;

 public Q_SLOTS:
  void EngineStateChanged(const EngineBase::State state);
  void CurrentSongChanged(const Song &song);
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result = AlbumCoverLoaderResult());

 private:
  void UpdatePlaybackStatus(const EngineBase::State state);
  void UpdateMetadata(const Song &song, const QUrl &art_url);
  void SetupButtonHandlers();

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  void *smtc_;  // Pointer to SystemMediaTransportControls (opaque to avoid WinRT headers in public header)
  bool apartment_initialized_;  // Track if we initialized the WinRT apartment
};

#endif  // WINDOWSMEDIACONTROLLER_H
