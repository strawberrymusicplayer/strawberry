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

#ifndef WINSYSTEMMEDIATRANSPORTCONTROLS_H
#define WINSYSTEMMEDIATRANSPORTCONTROLS_H

#include "config.h"

#include <windows.h>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QImage>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"

class Player;
class Song;
class AlbumCoverLoaderResult;

class WinSystemMediaTransportControls : public QObject {
  Q_OBJECT

 public:
  explicit WinSystemMediaTransportControls(SharedPtr<Player> player, QObject *parent = nullptr);
  ~WinSystemMediaTransportControls() override;

  bool Initialize(const HWND hwnd);

 public Q_SLOTS:
  void EngineStateChanged(EngineBase::State state);
  void CurrentSongChanged(const Song &song);
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result);

 private Q_SLOTS:
  void HandleButtonPressed(const int button);

 private:
  void UpdatePlaybackStatus(EngineBase::State state);
  void UpdateMetadata(const Song &song);
  void SetThumbnail(const QImage &image);
  void ClearThumbnail();
  void SetThumbnailFromFile(const QString &path);

  SharedPtr<Player> player_;

  bool ro_initialized_;
  void *smtc_;           // ABI::Windows::Media::ISystemMediaTransportControls*
  void *updater_;        // ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater*
  void *button_handler_; // IUnknown* (WRL handler, kept alive for unregistration)
  qint64 button_pressed_token_;

  EngineBase::State state_;
  QUrl current_song_url_;
  QString temp_art_path_;
};

#endif  // WINSYSTEMMEDIATRANSPORTCONTROLS_H
