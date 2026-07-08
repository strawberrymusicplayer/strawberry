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

#include <QObject>
#include <QMetaObject>
#include <QString>
#include <QImage>
#include <QBuffer>
#include <QUrl>
#include <QTimer>

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
      smtc2_(nullptr),
      updater_(nullptr),
      button_handler_(nullptr),
      button_pressed_token_(0),
      state_(EngineBase::State::Empty),
      current_duration_nanosec_(0),
      timeline_timer_(new QTimer(this)) {

  timeline_timer_->setInterval(1000);
  QObject::connect(timeline_timer_, &QTimer::timeout, this, &WinSystemMediaTransportControls::UpdateTimeline);

}

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

  if (smtc2_) {
    static_cast<ABI::Windows::Media::ISystemMediaTransportControls2*>(smtc2_)->Release();
    smtc2_ = nullptr;
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

  ABI::Windows::Media::ISystemMediaTransportControls2 *smtc2 = nullptr;
  if (SUCCEEDED(smtc->QueryInterface(__uuidof(ABI::Windows::Media::ISystemMediaTransportControls2), reinterpret_cast<void**>(&smtc2)))) {
    smtc2_ = smtc2;
  }
  else {
    qLog(Warning) << "WinSystemMediaTransportControls: ISystemMediaTransportControls2 unavailable, timeline disabled";
  }

  qLog(Info) << "WinSystemMediaTransportControls: Initialized";

  return true;

}

void WinSystemMediaTransportControls::EngineStateChanged(const EngineBase::State state) {

  state_ = state;
  UpdatePlaybackStatus(state);

  if (state == EngineBase::State::Playing) {
    timeline_timer_->start();
  }
  else {
    timeline_timer_->stop();
    UpdateTimeline();
  }

}

void WinSystemMediaTransportControls::CurrentSongChanged(const Song &song) {

  current_song_url_ = song.url();
  current_duration_nanosec_ = song.length_nanosec();
  UpdateMetadata(song);
  UpdateTimeline();

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

void WinSystemMediaTransportControls::UpdateTimeline() {

  if (!smtc2_) return;

  ABI::Windows::Media::ISystemMediaTransportControls2 *smtc2 = static_cast<ABI::Windows::Media::ISystemMediaTransportControls2*>(smtc2_);

  HSTRING h_class = nullptr;
  static const wchar_t kClass[] = L"Windows.Media.SystemMediaTransportControlsTimelineProperties";
  WindowsCreateString(kClass, static_cast<UINT32>(wcslen(kClass)), &h_class);

  IInspectable *insp = nullptr;
  const HRESULT hr = RoActivateInstance(h_class, &insp);
  WindowsDeleteString(h_class);
  if (FAILED(hr) || !insp) return;

  ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties *props = nullptr;
  if (FAILED(insp->QueryInterface(__uuidof(ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties), reinterpret_cast<void**>(&props))) || !props) {
    insp->Release();
    return;
  }
  insp->Release();

  const qint64 pos_ns = player_->engine()->position_nanosec();
  const qint64 dur_ns = current_duration_nanosec_ > 0 ? current_duration_nanosec_ : player_->engine()->length_nanosec();

  ABI::Windows::Foundation::TimeSpan zero_ts = {};
  ABI::Windows::Foundation::TimeSpan pos_ts = { pos_ns / 100 };
  ABI::Windows::Foundation::TimeSpan dur_ts = { dur_ns / 100 };

  props->put_StartTime(zero_ts);
  props->put_EndTime(dur_ts);
  props->put_MinSeekTime(zero_ts);
  props->put_MaxSeekTime(dur_ts);
  props->put_Position(pos_ts);

  smtc2->UpdateTimelineProperties(props);
  props->Release();

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

  // Create InMemoryRandomAccessStream — an agile (free-threaded) WinRT type.
  // Unlike CreateRandomAccessStreamOverStream, it has no STA affinity, so the SMTC background MTA thread can call OpenReadAsync without marshaling back to the GUI STA.
  IRandomAccessStream *ra_stream = nullptr;
  {
    HSTRING h = nullptr;
    static const wchar_t kImsClass[] = L"Windows.Storage.Streams.InMemoryRandomAccessStream";
    WindowsCreateString(kImsClass, static_cast<UINT32>(wcslen(kImsClass)), &h);
    IInspectable *insp = nullptr;
    const HRESULT hr = RoActivateInstance(h, &insp);
    WindowsDeleteString(h);
    if (FAILED(hr) || !insp) {
      ClearThumbnail();
      return;
    }
    insp->QueryInterface(__uuidof(IRandomAccessStream), reinterpret_cast<void**>(&ra_stream));
    insp->Release();
  }
  if (!ra_stream) {
    ClearThumbnail();
    return;
  }

  // Write JPEG bytes into the stream via DataWriter
  {
    IOutputStream *out = nullptr;
    ra_stream->GetOutputStreamAt(0, &out);
    if (!out) {
      ra_stream->Release();
      ClearThumbnail();
      return;
    }

    IDataWriterFactory *dwf = nullptr;
    {
      HSTRING h = nullptr;
      static const wchar_t kDwClass[] = L"Windows.Storage.Streams.DataWriter";
      WindowsCreateString(kDwClass, static_cast<UINT32>(wcslen(kDwClass)), &h);
      RoGetActivationFactory(h, __uuidof(IDataWriterFactory), reinterpret_cast<void**>(&dwf));
      WindowsDeleteString(h);
    }
    if (!dwf) {
      out->Release();
      ra_stream->Release();
      ClearThumbnail();
      return;
    }

    IDataWriter *dw = nullptr;
    dwf->CreateDataWriter(out, &dw);
    dwf->Release();
    out->Release();
    if (!dw) {
      ra_stream->Release();
      ClearThumbnail();
      return;
    }

    // WriteBytes buffers data synchronously into DataWriter's internal buffer
    dw->WriteBytes(static_cast<UINT32>(jpeg_data.size()), reinterpret_cast<BYTE*>(const_cast<char*>(jpeg_data.constData())));

    // StoreAsync flushes to the InMemoryRandomAccessStream.
    // The completion callback runs on an MTA thread pool thread — no STA marshal, so WaitForSingleObject from the GUI STA thread cannot deadlock here.
    using StoreOp = __FIAsyncOperation_1_UINT32_t;
    using StoreHandler = __FIAsyncOperationCompletedHandler_1_UINT32_t;
    StoreOp *store_op = nullptr;
    dw->StoreAsync(&store_op);
    if (store_op) {
      HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      if (ev) {
        auto cb = Microsoft::WRL::Callback<StoreHandler>([ev](StoreOp *, AsyncStatus) -> HRESULT {
          SetEvent(ev);
          return S_OK;
        });
        store_op->put_Completed(cb.Get());
        WaitForSingleObject(ev, 5000);
        CloseHandle(ev);
      }
      store_op->Release();
    }

    IOutputStream *detached = nullptr;
    dw->DetachStream(&detached);
    if (detached) detached->Release();
    dw->Release();
  }

  // Create a RandomAccessStreamReference from the agile stream
  IRandomAccessStreamReference *stream_ref = nullptr;
  {
    HSTRING h = nullptr;
    static const wchar_t kSrClass[] = L"Windows.Storage.Streams.RandomAccessStreamReference";
    WindowsCreateString(kSrClass, static_cast<UINT32>(wcslen(kSrClass)), &h);
    IRandomAccessStreamReferenceStatics *statics = nullptr;
    RoGetActivationFactory(h, __uuidof(IRandomAccessStreamReferenceStatics), reinterpret_cast<void**>(&statics));
    WindowsDeleteString(h);
    if (statics) {
      statics->CreateFromStream(ra_stream, &stream_ref);
      statics->Release();
    }
  }
  ra_stream->Release();
  if (!stream_ref) {
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
