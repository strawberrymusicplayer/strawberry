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

#include "config.h"

#include <windows.h>

#include <QObject>
#include <QString>
#include <QUrl>

#pragma push_macro("interface")
#undef interface
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <SystemMediaTransportControlsInterop.h>
#pragma pop_macro("interface")

#include "core/logging.h"
#include "windowsmediacontroller.h"

#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlistmanager.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"

using namespace winrt;
using namespace Windows::Media;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

WindowsMediaController::WindowsMediaController(HWND hwnd,
                                               const SharedPtr<Player> player,
                                               const SharedPtr<PlaylistManager> playlist_manager,
                                               const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                               QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      current_albumcover_loader_(current_albumcover_loader),
      smtc_(nullptr) {

  try {
    // Initialize WinRT
    winrt::init_apartment();

    // Get the SystemMediaTransportControls instance using the interop interface
    auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    winrt::com_ptr<IInspectable> smtc_inspectable;
    winrt::check_hresult(interop->GetForWindow(hwnd, winrt::guid_of<SystemMediaTransportControls>(), smtc_inspectable.put_void()));
    
    auto smtc = smtc_inspectable.as<SystemMediaTransportControls>();
    smtc_ = new SystemMediaTransportControls(smtc);
    
    auto controls = static_cast<SystemMediaTransportControls*>(smtc_);
    
    // Enable the controls
    controls->IsEnabled(true);
    controls->IsPlayEnabled(true);
    controls->IsPauseEnabled(true);
    controls->IsStopEnabled(true);
    controls->IsNextEnabled(true);
    controls->IsPreviousEnabled(true);

    // Setup button handlers
    SetupButtonHandlers();

    // Connect signals from Player
    QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &WindowsMediaController::EngineStateChanged);
    QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &WindowsMediaController::CurrentSongChanged);
    QObject::connect(&*current_albumcover_loader_, &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &WindowsMediaController::AlbumCoverLoaded);

    qLog(Info) << "Windows Media Transport Controls initialized successfully";
  }
  catch (const hresult_error &e) {
    qLog(Warning) << "Failed to initialize Windows Media Transport Controls:" << QString::fromWCharArray(e.message().c_str());
  }
  catch (...) {
    qLog(Warning) << "Failed to initialize Windows Media Transport Controls: unknown error";
  }
}

WindowsMediaController::~WindowsMediaController() {
  if (smtc_) {
    auto controls = static_cast<SystemMediaTransportControls*>(smtc_);
    controls->IsEnabled(false);
    delete controls;
    smtc_ = nullptr;
  }
  winrt::uninit_apartment();
}

void WindowsMediaController::SetupButtonHandlers() {
  if (!smtc_) return;

  auto controls = static_cast<SystemMediaTransportControls*>(smtc_);

  // Handle button pressed events
  controls->ButtonPressed([this](const SystemMediaTransportControls &, const SystemMediaTransportControlsButtonPressedEventArgs &args) {
    switch (args.Button()) {
      case SystemMediaTransportControlsButton::Play:
        player_->Play();
        break;
      case SystemMediaTransportControlsButton::Pause:
        player_->Pause();
        break;
      case SystemMediaTransportControlsButton::Stop:
        player_->Stop();
        break;
      case SystemMediaTransportControlsButton::Next:
        player_->Next();
        break;
      case SystemMediaTransportControlsButton::Previous:
        player_->Previous();
        break;
      default:
        break;
    }
  });
}

void WindowsMediaController::EngineStateChanged(EngineBase::State newState) {
  UpdatePlaybackStatus(newState);
}

void WindowsMediaController::UpdatePlaybackStatus(EngineBase::State state) {
  if (!smtc_) return;

  auto controls = static_cast<SystemMediaTransportControls*>(smtc_);

  try {
    switch (state) {
      case EngineBase::State::Playing:
        controls->PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
      case EngineBase::State::Paused:
        controls->PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
      case EngineBase::State::Empty:
      case EngineBase::State::Idle:
        controls->PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    }
  }
  catch (const hresult_error &e) {
    qLog(Warning) << "Failed to update playback status:" << QString::fromWCharArray(e.message().c_str());
  }
}

void WindowsMediaController::CurrentSongChanged(const Song &song) {
  if (!song.is_valid()) {
    return;
  }
  
  // Update metadata immediately with what we have
  UpdateMetadata(song, QUrl());
  
  // Album cover will be updated via AlbumCoverLoaded signal
}

void WindowsMediaController::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {
  if (!song.is_valid()) {
    return;
  }

  // Update metadata with album cover
  UpdateMetadata(song, result.temp_cover_url.isEmpty() ? result.cover_url : result.temp_cover_url);
}

void WindowsMediaController::UpdateMetadata(const Song &song, const QUrl &art_url) {
  if (!smtc_) return;

  auto controls = static_cast<SystemMediaTransportControls*>(smtc_);

  try {
    // Get the updater
    SystemMediaTransportControlsDisplayUpdater updater = controls->DisplayUpdater();
    updater.Type(MediaPlaybackType::Music);

    // Get the music properties
    auto musicProperties = updater.MusicProperties();

    // Set basic metadata
    if (!song.title().isEmpty()) {
      musicProperties.Title(winrt::hstring(song.title().toStdWString()));
    }
    if (!song.artist().isEmpty()) {
      musicProperties.Artist(winrt::hstring(song.artist().toStdWString()));
    }
    if (!song.album().isEmpty()) {
      musicProperties.AlbumTitle(winrt::hstring(song.album().toStdWString()));
    }

    // Set album art if available
    if (art_url.isValid() && art_url.isLocalFile()) {
      QString artPath = art_url.toLocalFile();
      if (!artPath.isEmpty()) {
        try {
          auto thumbnailStream = RandomAccessStreamReference::CreateFromFile(
            StorageFile::GetFileFromPathAsync(winrt::hstring(artPath.toStdWString())).get()
          );
          updater.Thumbnail(thumbnailStream);
          current_song_art_url_ = artPath;
        }
        catch (const hresult_error &e) {
          qLog(Debug) << "Failed to set album art:" << QString::fromWCharArray(e.message().c_str());
        }
      }
    }

    // Update the display
    updater.Update();
  }
  catch (const hresult_error &e) {
    qLog(Warning) << "Failed to update metadata:" << QString::fromWCharArray(e.message().c_str());
  }
}
