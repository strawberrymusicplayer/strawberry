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
#include <QMutex>
#include <QString>
#include <QUrl>
#include <QImage>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"

class QTimer;

class Player;
class Song;
class AlbumCoverLoaderResult;

class WinSystemMediaTransportControls : public QObject {
  Q_OBJECT

  friend class WinSystemMediaTransportControlsTest;

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
  void UpdateTimeline();
  void SetThumbnail(const QImage &image);
  void SetThumbnailFromStream(const quint64 thumbnail_id, void *stream);
  void ClearThumbnail();

  SharedPtr<Player> player_;

  bool ro_initialized_;
  void *smtc_;           // ABI::Windows::Media::ISystemMediaTransportControls*
  void *smtc2_;          // ABI::Windows::Media::ISystemMediaTransportControls2*
  void *updater_;        // ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater*
  void *button_handler_; // IUnknown* (WRL handler, kept alive for unregistration)
  qint64 button_pressed_token_;

  EngineBase::State state_;
  QUrl current_song_url_;
  qint64 current_duration_nanosec_;
  QTimer *timeline_timer_;
  // Increased for each new thumbnail, so a thumbnail which finishes storing after a newer one was set or cleared is ignored.
  quint64 thumbnail_id_;

  // Shared with the thumbnail store completion handlers, which run on another thread.
  // The destructor closes it, a handler only queues a call to this object while it holds the mutex and it's open, so this object isn't destroyed while it's queued.
  struct ThumbnailGate {
    QMutex mutex;
    bool open = true;
  };
  SharedPtr<ThumbnailGate> thumbnail_gate_;
  void *thumbnail_store_operation_;  // __FIAsyncOperation_1_UINT32_t*, kept until the next thumbnail
};

#endif  // WINSYSTEMMEDIATRANSPORTCONTROLS_H
