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

#include "config.h"

#include <windows.h>

#include <QObject>
#include <QString>
#include <QUrl>

// Undefine 'interface' macro from windows.h before including WinRT headers
#pragma push_macro("interface")
#undef interface

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma pop_macro("interface")

// Include the interop header for ISystemMediaTransportControlsInterop
#include <systemmediatransportcontrolsinterop.h>

#include "core/logging.h"
#include "windowsmediacontroller.h"

#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

// Helper struct to hold the WinRT object
struct WindowsMediaControllerPrivate {
  SystemMediaTransportControls smtc = nullptr;
};

WindowsMediaController::WindowsMediaController(const HWND hwnd, const SharedPtr<Player> player, const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader, QObject *parent)
    : QObject(parent),
      player_(player),
      current_albumcover_loader_(current_albumcover_loader),
      smtc_(nullptr),
      apartment_initialized_(false) {

  try {
    // Initialize WinRT apartment if not already initialized
    // Qt or other components may have already initialized it
    try {
      winrt::init_apartment(winrt::apartment_type::single_threaded);
      apartment_initialized_ = true;
    }
    catch (const hresult_error &e) {
      // Apartment already initialized - this is fine, continue
      if (e.code() != RPC_E_CHANGED_MODE) {
        throw;
      }
    }

    // Create private implementation
    auto *smtc_priv = new WindowsMediaControllerPrivate();
    smtc_ = smtc_priv;

    // Get the SystemMediaTransportControls instance for this window
    // Use the interop interface
    auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();

    if (!interop) {
      qLog(Warning) << "Failed to get ISystemMediaTransportControlsInterop";
      delete smtc_priv;
      smtc_ = nullptr;
      return;
    }

    // Get SMTC for the window
    winrt::com_ptr<winrt::Windows::Foundation::IInspectable> inspectable;
    HRESULT hr = interop->GetForWindow(hwnd, winrt::guid_of<SystemMediaTransportControls>(), inspectable.put_void());
    
    if (FAILED(hr) || !inspectable) {
      qLog(Warning) << "Failed to get SystemMediaTransportControls for window, HRESULT:" << Qt::hex << static_cast<unsigned int>(hr);
      delete smtc_priv;
      smtc_ = nullptr;
      return;
    }

    // Convert to SystemMediaTransportControls
    smtc_priv->smtc = inspectable.as<SystemMediaTransportControls>();
    
    if (!smtc_priv->smtc) {
      qLog(Warning) << "Failed to cast to SystemMediaTransportControls";
      delete smtc_priv;
      smtc_ = nullptr;
      return;
    }
    
    // Enable the controls
    smtc_priv->smtc.IsEnabled(true);
    smtc_priv->smtc.IsPlayEnabled(true);
    smtc_priv->smtc.IsPauseEnabled(true);
    smtc_priv->smtc.IsStopEnabled(true);
    smtc_priv->smtc.IsNextEnabled(true);
    smtc_priv->smtc.IsPreviousEnabled(true);

    // Setup button handlers
    SetupButtonHandlers();

    qLog(Info) << "Windows Media Transport Controls initialized successfully";
  }
  catch (const hresult_error &e) {
    qLog(Warning) << "Failed to initialize Windows Media Transport Controls:" << QString::fromWCharArray(e.message().c_str());
    if (smtc_) {
      delete static_cast<WindowsMediaControllerPrivate*>(smtc_);
      smtc_ = nullptr;
    }
  }
  catch (...) {
    qLog(Warning) << "Failed to initialize Windows Media Transport Controls: unknown error";
    if (smtc_) {
      delete static_cast<WindowsMediaControllerPrivate*>(smtc_);
      smtc_ = nullptr;
    }
  }

}

WindowsMediaController::~WindowsMediaController() {

  if (smtc_) {
    auto *smtc_private = reinterpret_cast<WindowsMediaControllerPrivate*>(smtc_);
    if (smtc_private->smtc) {
      smtc_private->smtc.IsEnabled(false);
    }
    delete smtc_private;
    smtc_ = nullptr;
  }

  if (apartment_initialized_) {
    winrt::uninit_apartment();
  }

}

void WindowsMediaController::SetupButtonHandlers() {

  if (!smtc_) return;

  auto *smtc_private = reinterpret_cast<WindowsMediaControllerPrivate*>(smtc_);
  if (!smtc_private->smtc) return;

  // Handle button pressed events
  smtc_private->smtc.ButtonPressed([this](const SystemMediaTransportControls &, const SystemMediaTransportControlsButtonPressedEventArgs &args) {
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

void WindowsMediaController::EngineStateChanged(const EngineBase::State state) {
  UpdatePlaybackStatus(state);
}

void WindowsMediaController::UpdatePlaybackStatus(const EngineBase::State state) {

  if (!smtc_) return;

  auto *smtc_private = reinterpret_cast<WindowsMediaControllerPrivate*>(smtc_);
  if (!smtc_private->smtc) return;

  try {
    switch (state) {
      case EngineBase::State::Playing:
        smtc_private->smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
      case EngineBase::State::Paused:
        smtc_private->smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
      case EngineBase::State::Empty:
      case EngineBase::State::Idle:
        smtc_private->smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
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
  UpdateMetadata(song, result.temp_cover_url.isEmpty() ? result.album_cover.cover_url : result.temp_cover_url);

}

void WindowsMediaController::UpdateMetadata(const Song &song, const QUrl &art_url) {

  if (!smtc_) return;

  auto *smtc_private = reinterpret_cast<WindowsMediaControllerPrivate*>(smtc_);
  if (!smtc_private->smtc) return;

  try {
    // Get the updater
    SystemMediaTransportControlsDisplayUpdater display_updater = smtc_private->smtc.DisplayUpdater();
    display_updater.Type(MediaPlaybackType::Music);

    // Get the music properties
    auto music_properties = display_updater.MusicProperties();

    // Set basic metadata
    music_properties.Title(winrt::hstring(song.PrettyTitle().toStdWString()));
    if (!song.artist().isEmpty()) {
      music_properties.Artist(winrt::hstring(song.artist().toStdWString()));
    }
    if (!song.album().isEmpty()) {
      music_properties.AlbumTitle(winrt::hstring(song.album().toStdWString()));
    }

    // Set album art if available
    if (art_url.isValid() && art_url.isLocalFile()) {
      try {
        const QString file_uri = art_url.toString();
        auto thumbnailStream = RandomAccessStreamReference::CreateFromUri(winrt::Windows::Foundation::Uri(winrt::hstring(file_uri.toStdWString())));
        display_updater.Thumbnail(thumbnailStream);
      }
      catch (const hresult_error &e) {
        qLog(Debug) << "Failed to set album art:" << QString::fromWCharArray(e.message().c_str());
      }
    }
    display_updater.Update();
  }
  catch (const hresult_error &e) {
    qLog(Warning) << "Failed to update metadata:" << QString::fromWCharArray(e.message().c_str());
  }

}
