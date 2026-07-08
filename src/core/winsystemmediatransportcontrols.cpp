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

#include "config.h"

#include <windows.h>
#include <objbase.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <eventtoken.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.streams.h>
#if __has_include(<wrl.h>)
#  include <wrl.h>
#else
#  include <wrl/client.h>
#  include <wrl/event.h>
#endif

#include <systemmediatransportcontrolsinterop.h>

// robuffer.h may not be present in all SDK installs; declare the shcore.dll function manually.
typedef enum BSOS_OPTIONS { BSOS_DEFAULT = 0, BSOS_PREFERDESTINATIONSTREAM = 1 } BSOS_OPTIONS;
extern "C" HRESULT WINAPI CreateRandomAccessStreamOverStream(IStream *stream, BSOS_OPTIONS options, REFIID riid, void **ppv);
#pragma comment(lib, "shcore.lib")

#include <QObject>
#include <QMetaObject>
#include <QString>
#include <QImage>
#include <QBuffer>
#include <QUrl>

#include "core/logging.h"
#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "covermanager/albumcoverloaderresult.h"
#include "winsystemmediatransportcontrols.h"

using namespace ABI::Windows::Media;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;

namespace {

// Helper: create an HSTRING from a QString, caller must WindowsDeleteString() it.
HRESULT CreateHString(const QString &str, HSTRING *out) {
  const std::wstring w = str.toStdWString();
  return WindowsCreateString(w.c_str(), static_cast<UINT32>(w.size()), out);
}

using SmtcBtnHandlerType = ITypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*>;

}  // namespace

WinSystemMediaTransportControls::WinSystemMediaTransportControls(SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      ro_initialized_(false),
      smtc_(nullptr),
      updater_(nullptr),
      button_handler_(nullptr),
      button_pressed_token_(0),
      state_(EngineBase::State::Empty) {}

WinSystemMediaTransportControls::~WinSystemMediaTransportControls() {

  ISystemMediaTransportControls *smtc = static_cast<ISystemMediaTransportControls*>(smtc_);

  if (smtc && button_pressed_token_) {
    EventRegistrationToken token;
    token.value = button_pressed_token_;
    smtc->remove_ButtonPressed(token);
  }

  if (button_handler_) {
    static_cast<IUnknown*>(button_handler_)->Release();
    button_handler_ = nullptr;
  }

  if (updater_) {
    static_cast<ISystemMediaTransportControlsDisplayUpdater*>(updater_)->Release();
    updater_ = nullptr;
  }

  if (smtc) {
    smtc->put_IsEnabled(false);
    smtc->Release();
    smtc_ = nullptr;
  }

  if (ro_initialized_) {
    RoUninitialize();
  }

}

bool WinSystemMediaTransportControls::Initialize(const HWND hwnd) {

  const HRESULT hr_init = RoInitialize(RO_INIT_SINGLETHREADED);
  if (hr_init != S_OK && hr_init != S_FALSE) {
    qLog(Error) << "WinSystemMediaTransportControls: RoInitialize failed" << Qt::hex << static_cast<DWORD>(hr_init);
    return false;
  }
  ro_initialized_ = true;

  // Obtain the Win32 interop factory for SystemMediaTransportControls
  HSTRING h_class = nullptr;
  static const wchar_t kSmtcClass[] = L"Windows.Media.SystemMediaTransportControls";
  WindowsCreateString(kSmtcClass, static_cast<UINT32>(wcslen(kSmtcClass)), &h_class);

  ISystemMediaTransportControlsInterop *interop = nullptr;
  HRESULT hr = RoGetActivationFactory(h_class, __uuidof(ISystemMediaTransportControlsInterop), reinterpret_cast<void**>(&interop));
  WindowsDeleteString(h_class);

  if (FAILED(hr) || !interop) {
    qLog(Error) << "WinSystemMediaTransportControls: Failed to get SMTC interop factory" << Qt::hex << static_cast<DWORD>(hr);
    return false;
  }

  // Get SMTC bound to our window handle
  ISystemMediaTransportControls *smtc = nullptr;
  hr = interop->GetForWindow(hwnd, __uuidof(ISystemMediaTransportControls), reinterpret_cast<void**>(&smtc));
  interop->Release();

  if (FAILED(hr) || !smtc) {
    qLog(Error) << "WinSystemMediaTransportControls: GetForWindow failed" << Qt::hex << static_cast<DWORD>(hr);
    return false;
  }

  smtc->put_IsEnabled(true);
  smtc->put_IsPlayEnabled(true);
  smtc->put_IsPauseEnabled(true);
  smtc->put_IsStopEnabled(true);
  smtc->put_IsNextEnabled(true);
  smtc->put_IsPreviousEnabled(true);

  // Register button-pressed handler using WRL Callback
  Microsoft::WRL::ComPtr<SmtcBtnHandlerType> handler = Microsoft::WRL::Callback<SmtcBtnHandlerType>([this](ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs *args) -> HRESULT {
    SystemMediaTransportControlsButton btn;
    if (SUCCEEDED(args->get_Button(&btn))) {
      QMetaObject::invokeMethod(this, "HandleButtonPressed", Qt::QueuedConnection, Q_ARG(const int, static_cast<const int>(btn)));
    }
    return S_OK;
  });

  EventRegistrationToken token{};
  hr = smtc->add_ButtonPressed(handler.Get(), &token);
  if (FAILED(hr)) {
    qLog(Error) << "WinSystemMediaTransportControls: Failed to register button handler" << Qt::hex << static_cast<DWORD>(hr);
    smtc->put_IsEnabled(false);
    smtc->Release();
    return false;
  }

  ISystemMediaTransportControlsDisplayUpdater *updater = nullptr;
  hr = smtc->get_DisplayUpdater(&updater);
  if (FAILED(hr) || !updater) {
    qLog(Error) << "WinSystemMediaTransportControls: Failed to get display updater" << Qt::hex << static_cast<DWORD>(hr);
    smtc->remove_ButtonPressed(token);
    smtc->put_IsEnabled(false);
    smtc->Release();
    return false;
  }

  handler->AddRef();
  button_handler_ = handler.Get();
  button_pressed_token_ = token.value;
  smtc_ = smtc;
  updater_ = updater;

  qLog(Info) << "WinSystemMediaTransportControls: Initialized";

  return true;

}

void WinSystemMediaTransportControls::EngineStateChanged(const EngineBase::State state) {

  state_ = state;
  UpdatePlaybackStatus(state);

}

void WinSystemMediaTransportControls::CurrentSongChanged(const Song &song) {

  current_song_url_ = song.url();
  UpdateMetadata(song);

}

void WinSystemMediaTransportControls::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  if (song.url() != current_song_url_) return;

  if (result.success && !result.album_cover.image.isNull()) {
    SetThumbnail(result.album_cover.image);
  }
  else {
    ClearThumbnail();
  }

}

void WinSystemMediaTransportControls::UpdatePlaybackStatus(const EngineBase::State state) {

  if (!smtc_) return;

  ISystemMediaTransportControls *smtc = static_cast<ISystemMediaTransportControls*>(smtc_);

  ABI::Windows::Media::MediaPlaybackStatus playback_status;
  switch (state) {
    case EngineBase::State::Playing:
      playback_status = ABI::Windows::Media::MediaPlaybackStatus_Playing;
      break;
    case EngineBase::State::Paused:
      playback_status = ABI::Windows::Media::MediaPlaybackStatus_Paused;
      break;
    default:
      playback_status = ABI::Windows::Media::MediaPlaybackStatus_Stopped;
      break;
  }

  smtc->put_PlaybackStatus(playback_status);

}

void WinSystemMediaTransportControls::UpdateMetadata(const Song &song) {

  if (!updater_) return;

  ISystemMediaTransportControlsDisplayUpdater *updater = static_cast<ISystemMediaTransportControlsDisplayUpdater*>(updater_);

  if (!song.is_valid()) {
    updater->ClearAll();
    updater->Update();
    return;
  }

  updater->put_Type(ABI::Windows::Media::MediaPlaybackType_Music);

  ABI::Windows::Media::IMusicDisplayProperties *music_props = nullptr;
  if (SUCCEEDED(updater->get_MusicProperties(&music_props)) && music_props) {

    HSTRING h = nullptr;
    if (SUCCEEDED(CreateHString(song.title(), &h))) {
      music_props->put_Title(h);
      WindowsDeleteString(h);
      h = nullptr;
    }

    const QString artist = song.effective_albumartist().isEmpty() ? song.artist() : song.effective_albumartist();
    if (SUCCEEDED(CreateHString(artist, &h))) {
      music_props->put_Artist(h);
      WindowsDeleteString(h);
      h = nullptr;
    }

    // AlbumTitle is exposed on IMusicDisplayProperties2 (not v1)
    ABI::Windows::Media::IMusicDisplayProperties2 *music_props2 = nullptr;
    if (SUCCEEDED(music_props->QueryInterface(__uuidof(ABI::Windows::Media::IMusicDisplayProperties2), reinterpret_cast<void**>(&music_props2)))) {
      if (SUCCEEDED(CreateHString(song.album(), &h))) {
        music_props2->put_AlbumTitle(h);
        WindowsDeleteString(h);
      }
      music_props2->Release();
    }

    music_props->Release();
  }

  updater->Update();

}

void WinSystemMediaTransportControls::SetThumbnail(const QImage &image) {

  if (!updater_) return;

  ISystemMediaTransportControlsDisplayUpdater *updater = static_cast<ISystemMediaTransportControlsDisplayUpdater*>(updater_);

  // Encode image to JPEG bytes in memory
  QByteArray jpeg_data;
  {
    QBuffer buf(&jpeg_data);
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "JPEG", 90)) {
      qLog(Warning) << "WinSystemMediaTransportControls: Failed to encode thumbnail";
      ClearThumbnail();
      return;
    }
  }

  // Create a COM IStream over the JPEG bytes
  HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(jpeg_data.size()));
  if (!hg) {
    ClearThumbnail();
    return;
  }
  void *ptr = GlobalLock(hg);
  if (!ptr) {
    GlobalFree(hg);
    ClearThumbnail(); return;
  }
  memcpy(ptr, jpeg_data.constData(), static_cast<size_t>(jpeg_data.size()));
  GlobalUnlock(hg);

  IStream *stream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(hg, TRUE, &stream)) || !stream) {
    GlobalFree(hg);
    ClearThumbnail();
    return;
  }

  // Wrap as a WinRT IRandomAccessStream
  IRandomAccessStream *ra_stream = nullptr;
  HRESULT hr = CreateRandomAccessStreamOverStream(stream, BSOS_DEFAULT, __uuidof(IRandomAccessStream), reinterpret_cast<void**>(&ra_stream));
  stream->Release();
  if (FAILED(hr) || !ra_stream) {
    ClearThumbnail();
    return;
  }

  // Create a RandomAccessStreamReference from the stream
  HSTRING h_stream_class = nullptr;
  static const wchar_t kStreamRefClass[] = L"Windows.Storage.Streams.RandomAccessStreamReference";
  WindowsCreateString(kStreamRefClass, static_cast<UINT32>(wcslen(kStreamRefClass)), &h_stream_class);

  IRandomAccessStreamReferenceStatics *stream_statics = nullptr;
  hr = RoGetActivationFactory(h_stream_class, __uuidof(IRandomAccessStreamReferenceStatics), reinterpret_cast<void**>(&stream_statics));
  WindowsDeleteString(h_stream_class);
  if (FAILED(hr) || !stream_statics) {
    ra_stream->Release();
    ClearThumbnail();
    return;
  }

  IRandomAccessStreamReference *stream_ref = nullptr;
  hr = stream_statics->CreateFromStream(ra_stream, &stream_ref);
  ra_stream->Release();
  stream_statics->Release();
  if (FAILED(hr) || !stream_ref) {
    ClearThumbnail();
    return;
  }

  updater->put_Thumbnail(stream_ref);
  stream_ref->Release();
  updater->Update();

}

void WinSystemMediaTransportControls::ClearThumbnail() {

  if (!updater_) return;
  ISystemMediaTransportControlsDisplayUpdater *updater = static_cast<ISystemMediaTransportControlsDisplayUpdater*>(updater_);
  updater->put_Thumbnail(nullptr);
  updater->Update();

}

void WinSystemMediaTransportControls::HandleButtonPressed(const int button) {

  switch (static_cast<ABI::Windows::Media::SystemMediaTransportControlsButton>(button)) {
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Play:
      player_->Play();
      break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Pause:
      player_->Pause();
      break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Stop:
      player_->Stop();
      break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Next:
      player_->Next();
      break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Previous:
      player_->Previous();
      break;
    default:
      break;
  }

}
