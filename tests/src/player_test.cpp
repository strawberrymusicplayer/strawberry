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

#include <memory>

#include "gtest_include.h"

#include <gst/gst.h>

#include <QCoreApplication>
#include <QList>
#include <QMetaObject>
#include <QScopeGuard>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/player.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/urlhandler.h"
#include "core/urlhandlers.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "constants/backendsettings.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentalbumcoverloader.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistcontainer.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "tagreader/tagreaderclient.h"
#include "mock_settingsprovider.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// Records the URLs the player loads, which is the first thing it does when preloading the next track.
class TestUrlHandler : public UrlHandler {
 public:
  QString scheme() const override { return u"test"_s; }
  LoadResult StartLoading(const QUrl &url) override {
    started_urls << url;
    return LoadResult(url, load_asynchronously ? LoadResult::Type::WillLoadAsynchronously : LoadResult::Type::NoMoreTracks);
  }

  QList<QUrl> started_urls;
  bool load_asynchronously = false;
};

class PlayerTest : public ::testing::Test {
 protected:
  PlayerTest()
      : tagreader_client_(new TagReaderClient()),
        tagreader_client_thread_(new QThread()),
        sequence_(nullptr, new DummySettingsProvider) {
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  ~PlayerTest() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  void SetUp() override {

    gst_init(nullptr, nullptr);
    ASSERT_TRUE(temp_dir_.isValid());

    task_manager_ = make_shared<TaskManager>();
    url_handlers_ = make_shared<UrlHandlers>();
    url_handlers_->Register(&url_handler_);
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    playlist_backend_ = make_shared<PlaylistBackend>(database_, tagreader_client_, collection_backend_);
    current_albumcover_loader_ = make_shared<CurrentAlbumCoverLoader>(make_shared<AlbumCoverLoader>(tagreader_client_));

    playlist_manager_ = make_shared<PlaylistManager>(task_manager_, tagreader_client_, url_handlers_, playlist_backend_, collection_backend_, current_albumcover_loader_);
    playlist_manager_->Init(&sequence_, &playlist_container_);
    player_ = make_shared<Player>(task_manager_, url_handlers_, playlist_manager_);

    Playlist *playlist = playlist_manager_->active();
    playlist->InsertItems(PlaylistItemPtrList() << MakeItem(u"first"_s) << MakeItem(u"second"_s));
    playlist->set_current_row(0);

  }

  void TearDown() override {
    player_.reset();
    playlist_manager_.reset();
    QCoreApplication::processEvents();
    database_->Close();
  }

  static PlaylistItemPtr MakeItem(const QString &title) {
    Song song(Song::Source::Stream);
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl(u"test:"_s + title));
    return PlaylistItem::NewFromSong(song);
  }

  void TrackAboutToEnd() {
    ASSERT_TRUE(QMetaObject::invokeMethod(&*player_, "TrackAboutToEnd", Qt::DirectConnection));
  }

  void TrackEnded() {
    ASSERT_TRUE(QMetaObject::invokeMethod(&*player_, "TrackEnded", Qt::DirectConnection));
  }

  // Enables auto crossfade, also between tracks of the same album, and starts playing the first track, which is loading until the end of the test.
  void PlayFirstTrackWithAutoCrossfade() {
    Settings s;
    s.beginGroup(BackendSettings::kSettingsGroup);
    s.setValue(BackendSettings::kAutoCrossfadeEnabled, true);
    s.setValue(BackendSettings::kNoCrossfadeSameAlbum, false);
    s.endGroup();
    player_->engine()->ReloadSettings();
    ASSERT_TRUE(player_->engine()->is_autocrossfade_enabled());

    url_handler_.load_asynchronously = true;
    player_->PlayAt(0, false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Never, true);
    ASSERT_TRUE(player_->GetCurrentItem());
    url_handler_.started_urls.clear();
  }

  static void RemoveCrossfadeSettings() {
    Settings s;
    s.beginGroup(BackendSettings::kSettingsGroup);
    s.remove(BackendSettings::kAutoCrossfadeEnabled);
    s.remove(BackendSettings::kNoCrossfadeSameAlbum);
    s.endGroup();
  }

  QTemporaryDir temp_dir_;
  TestUrlHandler url_handler_;
  SharedPtr<TagReaderClient> tagreader_client_;
  QThread *tagreader_client_thread_;
  PlaylistSequence sequence_;
  PlaylistContainer playlist_container_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<UrlHandlers> url_handlers_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<PlaylistBackend> playlist_backend_;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  SharedPtr<PlaylistManager> playlist_manager_;
  SharedPtr<Player> player_;
};

// The next track is preloaded when the current track is about to end.
TEST_F(PlayerTest, PreloadsNextTrack) {

  TrackAboutToEnd();

  EXPECT_EQ(QList<QUrl>() << QUrl(u"test:second"_s), url_handler_.started_urls);

}

// Regression test: with "Stop after this track", the next track was preloaded, or crossfaded into, which stopped the track seconds before it ended.
TEST_F(PlayerTest, DoesNotPreloadNextTrackWhenStoppingAfterCurrent) {

  player_->StopAfterCurrent();

  TrackAboutToEnd();

  EXPECT_TRUE(url_handler_.started_urls.isEmpty());

}

// With auto crossfade, the next track starts when the current track is about to end.
TEST_F(PlayerTest, AutoCrossfadeStartsNextTrack) {

  const auto cleanup = qScopeGuard([]() { RemoveCrossfadeSettings(); });
  PlayFirstTrackWithAutoCrossfade();

  TrackAboutToEnd();

  EXPECT_EQ(1, playlist_manager_->active()->current_row());
  EXPECT_EQ(QList<QUrl>() << QUrl(u"test:second"_s), url_handler_.started_urls);

}

// Regression test: with auto crossfade and "Stop after this track", playback stopped when the track was about to end, seconds before it ended.
TEST_F(PlayerTest, AutoCrossfadeKeepsPlayingUntilTrackEndedWhenStoppingAfterCurrent) {

  const auto cleanup = qScopeGuard([]() { RemoveCrossfadeSettings(); });
  PlayFirstTrackWithAutoCrossfade();
  player_->StopAfterCurrent();
  Playlist *playlist = playlist_manager_->active();

  TrackAboutToEnd();

  EXPECT_TRUE(player_->GetCurrentItem());
  EXPECT_EQ(0, playlist->current_row());
  EXPECT_TRUE(playlist->stop_after_current());

  TrackEnded();

  EXPECT_FALSE(player_->GetCurrentItem());
  EXPECT_EQ(-1, playlist->current_row());
  EXPECT_FALSE(playlist->stop_after_current());
  EXPECT_TRUE(url_handler_.started_urls.isEmpty());

}

}  // namespace
