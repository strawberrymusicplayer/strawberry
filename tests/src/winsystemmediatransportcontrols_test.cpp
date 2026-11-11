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
#include <roapi.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.streams.h>
#if __has_include(<wrl.h>)
#  include <wrl.h>
#else
#  include <wrl/client.h>
#  include <wrl/implements.h>
#endif

#include "gtest_include.h"

#include <QList>
#include <QByteArray>
#include <QBuffer>
#include <QImage>
#include <QColor>
#include <QTest>

#include "core/song.h"
#include "core/winsystemmediatransportcontrols.h"

using namespace ABI::Windows::Media;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using Microsoft::WRL::ComPtr;

namespace {

// Display updater which records the thumbnails, so the tests don't need a window or the real system media transport controls.
class FakeDisplayUpdater : public Microsoft::WRL::RuntimeClass<ISystemMediaTransportControlsDisplayUpdater> {
  InspectableClass(L"Strawberry.Tests.FakeDisplayUpdater", BaseTrust)

 public:
  HRESULT STDMETHODCALLTYPE get_Type(MediaPlaybackType *value) override { *value = MediaPlaybackType_Music; return S_OK; }
  HRESULT STDMETHODCALLTYPE put_Type(MediaPlaybackType) override { return S_OK; }
  HRESULT STDMETHODCALLTYPE get_AppMediaId(HSTRING *value) override { *value = nullptr; return S_OK; }
  HRESULT STDMETHODCALLTYPE put_AppMediaId(HSTRING) override { return S_OK; }
  HRESULT STDMETHODCALLTYPE get_Thumbnail(IRandomAccessStreamReference **value) override {
    *value = thumbnails.isEmpty() ? nullptr : thumbnails.last().Get();
    if (*value) (*value)->AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE put_Thumbnail(IRandomAccessStreamReference *value) override {
    thumbnails << ComPtr<IRandomAccessStreamReference>(value);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_MusicProperties(IMusicDisplayProperties **value) override { *value = nullptr; return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE get_VideoProperties(IVideoDisplayProperties **value) override { *value = nullptr; return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE get_ImageProperties(IImageDisplayProperties **value) override { *value = nullptr; return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE CopyFromFileAsync(MediaPlaybackType, ABI::Windows::Storage::IStorageFile*, __FIAsyncOperation_1_boolean **operation) override { *operation = nullptr; return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE ClearAll() override { return S_OK; }
  HRESULT STDMETHODCALLTYPE Update() override { ++update_count; return S_OK; }

  // Every thumbnail which was set, nullptr when it was cleared.
  QList<ComPtr<IRandomAccessStreamReference>> thumbnails;
  int update_count = 0;

  qsizetype ThumbnailsSet() const {
    qsizetype count = 0;
    for (const ComPtr<IRandomAccessStreamReference> &thumbnail : thumbnails) {
      if (thumbnail) ++count;
    }
    return count;
  }
};

QImage CreateImage(const int size, const QColor &color) {

  QImage image(size, size, QImage::Format_RGB32);
  image.fill(color);
  return image;

}

// The thumbnail is encoded the same way by WinSystemMediaTransportControls, so the size tells which image a thumbnail is.
qint64 JpegSize(const QImage &image) {

  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "JPEG", 90);
  return data.size();

}

// Reads the size of the stream behind a thumbnail, waits for the asynchronous open while processing events.
qint64 ThumbnailSize(IRandomAccessStreamReference *thumbnail) {

  ComPtr<__FIAsyncOperation_1_Windows__CStorage__CStreams__CIRandomAccessStreamWithContentType> operation;
  if (FAILED(thumbnail->OpenReadAsync(&operation)) || !operation) return -1;

  ComPtr<IAsyncInfo> info;
  if (FAILED(operation.As(&info))) return -1;
  AsyncStatus status = AsyncStatus::Started;
  if (!QTest::qWaitFor([&info, &status]() { info->get_Status(&status); return status != AsyncStatus::Started; }, 5000)) return -1;
  if (status != AsyncStatus::Completed) return -1;

  ComPtr<IRandomAccessStreamWithContentType> stream_with_content_type;
  if (FAILED(operation->GetResults(&stream_with_content_type)) || !stream_with_content_type) return -1;
  ComPtr<IRandomAccessStream> stream;
  if (FAILED(stream_with_content_type.As(&stream))) return -1;
  UINT64 size = 0;
  if (FAILED(stream->get_Size(&size))) return -1;

  return static_cast<qint64>(size);

}

}  // namespace

// Declared at file scope so that the "friend class WinSystemMediaTransportControlsTest;" in WinSystemMediaTransportControls resolves to this class.
class WinSystemMediaTransportControlsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    ASSERT_TRUE(hr == S_OK || hr == S_FALSE) << std::hex << static_cast<DWORD>(hr);
    ro_initialized_ = true;
    updater_ = Microsoft::WRL::Make<FakeDisplayUpdater>();
    ASSERT_TRUE(updater_);
    smtc_ = CreateControls();
  }

  void TearDown() override {
    delete smtc_;
    smtc_ = nullptr;
    // Let store completion handlers which are still running finish before the next test.
    QTest::qWait(100);
    updater_.Reset();
    if (ro_initialized_) RoUninitialize();
  }

  // The controls are not initialized with a window, the fake display updater is used in place of the real one.
  WinSystemMediaTransportControls *CreateControls() {
    WinSystemMediaTransportControls *smtc = new WinSystemMediaTransportControls(nullptr);
    ISystemMediaTransportControlsDisplayUpdater *updater = updater_.Get();
    updater->AddRef();
    smtc->updater_ = updater;
    return smtc;
  }

  static void SetThumbnail(WinSystemMediaTransportControls *smtc, const QImage &image) { smtc->SetThumbnail(image); }
  static void ClearThumbnail(WinSystemMediaTransportControls *smtc) { smtc->ClearThumbnail(); }

  bool WaitForThumbnailsSet(const qsizetype count) {
    return QTest::qWaitFor([this, count]() { return updater_->ThumbnailsSet() >= count; }, 5000);
  }

  bool ro_initialized_ = false;
  ComPtr<FakeDisplayUpdater> updater_;
  WinSystemMediaTransportControls *smtc_ = nullptr;
};

namespace {

TEST_F(WinSystemMediaTransportControlsTest, ThumbnailIsSetWhenStored) {

  const QImage image = CreateImage(64, Qt::red);
  SetThumbnail(smtc_, image);

  // The store isn't waited for, the thumbnail is set from a queued call.
  EXPECT_EQ(0, updater_->ThumbnailsSet());

  ASSERT_TRUE(WaitForThumbnailsSet(1));
  ASSERT_EQ(1, updater_->thumbnails.size());
  EXPECT_GE(updater_->update_count, 1);
  // The whole image was stored before the thumbnail was set.
  EXPECT_EQ(JpegSize(image), ThumbnailSize(updater_->thumbnails.last().Get()));

}

TEST_F(WinSystemMediaTransportControlsTest, NewerThumbnailReplacesThumbnailBeingStored) {

  const QImage old_image = CreateImage(32, Qt::red);
  const QImage new_image = CreateImage(256, Qt::blue);
  ASSERT_NE(JpegSize(old_image), JpegSize(new_image));

  SetThumbnail(smtc_, old_image);
  SetThumbnail(smtc_, new_image);

  ASSERT_TRUE(WaitForThumbnailsSet(1));
  QTest::qWait(200);

  ASSERT_EQ(1, updater_->ThumbnailsSet());
  EXPECT_EQ(JpegSize(new_image), ThumbnailSize(updater_->thumbnails.last().Get()));

}

TEST_F(WinSystemMediaTransportControlsTest, ClearedThumbnailIsNotReplacedByThumbnailBeingStored) {

  SetThumbnail(smtc_, CreateImage(64, Qt::red));
  ClearThumbnail(smtc_);

  QTest::qWait(300);

  EXPECT_EQ(0, updater_->ThumbnailsSet());
  ASSERT_FALSE(updater_->thumbnails.isEmpty());
  EXPECT_FALSE(updater_->thumbnails.last());

}

TEST_F(WinSystemMediaTransportControlsTest, ThumbnailOfPreviousSongIsNotShown) {

  const QImage old_image = CreateImage(32, Qt::red);
  const QImage new_image = CreateImage(256, Qt::blue);
  ASSERT_NE(JpegSize(old_image), JpegSize(new_image));

  SetThumbnail(smtc_, old_image);
  smtc_->CurrentSongChanged(Song());

  QTest::qWait(300);
  EXPECT_EQ(0, updater_->ThumbnailsSet());

  // The cover of the new song is still shown.
  SetThumbnail(smtc_, new_image);
  ASSERT_TRUE(WaitForThumbnailsSet(1));
  QTest::qWait(100);
  ASSERT_EQ(1, updater_->ThumbnailsSet());
  EXPECT_EQ(JpegSize(new_image), ThumbnailSize(updater_->thumbnails.last().Get()));

}

TEST_F(WinSystemMediaTransportControlsTest, DestroyedWhileThumbnailIsBeingStored) {

  for (int i = 0; i < 20; ++i) {
    WinSystemMediaTransportControls *smtc = CreateControls();
    SetThumbnail(smtc, CreateImage(128, Qt::green));
    delete smtc;
  }

  // The completion handlers must not call the destroyed objects.
  QTest::qWait(300);
  EXPECT_EQ(0, updater_->ThumbnailsSet());

}

}  // namespace
