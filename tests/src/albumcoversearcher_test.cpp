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

#include <memory>

#include "gtest_include.h"

#include <QList>
#include <QString>
#include <QIcon>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QScopeGuard>
#include <QWidget>
#include <QUrl>
#include <QImage>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QEvent>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "widgets/searchfield.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverfetcher.h"
#include "covermanager/albumcoversearcher.h"
#include "covermanager/albumcoverimageresult.h"
#include "covermanager/coverprovider.h"
#include "covermanager/coverproviders.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {

// A cover provider which records the searches, they are finished by the test.
class FakeCoverProvider : public CoverProvider {
 public:
  explicit FakeCoverProvider(const SharedPtr<NetworkAccessManager> network) : CoverProvider(u"Fake"_s, true, false, 1.0, true, true, network, nullptr) {}

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override {
    Q_UNUSED(artist);
    Q_UNUSED(album);
    Q_UNUSED(title);
    started_ids_ << id;
    return true;
  }

  void CancelSearch(const int id) override { cancelled_ids_ << id; }

  void FinishSearch(const int id, const CoverProviderSearchResults &results = CoverProviderSearchResults()) { Q_EMIT SearchFinished(id, results); }

  QList<int> started_ids_;
  QList<int> cancelled_ids_;
};

class AlbumCoverSearcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_ = make_shared<NetworkAccessManager>();
    cover_providers_ = make_shared<CoverProviders>();
    // The cover providers own the provider.
    provider_ = new FakeCoverProvider(network_);
    cover_providers_->AddProvider(provider_);
    fetcher_ = std::make_unique<AlbumCoverFetcher>(cover_providers_, network_);
    albumcover_loader_ = make_shared<AlbumCoverLoader>(nullptr);
  }

  SharedPtr<NetworkAccessManager> network_;
  SharedPtr<CoverProviders> cover_providers_;
  FakeCoverProvider *provider_ = nullptr;
  std::unique_ptr<AlbumCoverFetcher> fetcher_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
};

// Searches in the dialog like the user does.
void SearchInDialog(AlbumCoverSearcher *searcher, QPushButton **button_search) {
  SearchField *artist = searcher->findChild<SearchField*>(u"artist"_s);
  SearchField *album = searcher->findChild<SearchField*>(u"album"_s);
  *button_search = searcher->findChild<QPushButton*>(u"search"_s);
  ASSERT_NE(nullptr, artist);
  ASSERT_NE(nullptr, album);
  ASSERT_NE(nullptr, *button_search);
  artist->setText(u"Other artist"_s);
  album->setText(u"Other album"_s);
  (*button_search)->click();
}

// Regression test: Abort in the cover search dialog cancelled all the requests of the fetcher, also the automatic cover search for the playing song, which then never finished, so the cover kept showing the spinner.
TEST_F(AlbumCoverSearcherTest, AbortOnlyCancelsTheDialogSearch) {

  QSignalSpy fetched_spy(&*fetcher_, &AlbumCoverFetcher::AlbumCoverFetched);
  QSignalSpy search_finished_spy(&*fetcher_, &AlbumCoverFetcher::SearchFinished);

  // The automatic cover search for the playing song.
  const quint64 playing_request_id = fetcher_->FetchAlbumCover(u"Artist"_s, u"Album"_s, u"Title"_s, true);
  ASSERT_EQ(1, provider_->started_ids_.count());
  const int playing_provider_id = provider_->started_ids_.first();

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  // Search, then abort the search.
  QPushButton *button_search = nullptr;
  SearchInDialog(&searcher, &button_search);
  if (HasFatalFailure()) return;
  ASSERT_EQ(2, provider_->started_ids_.count());
  const int dialog_provider_id = provider_->started_ids_.last();
  button_search->click();

  EXPECT_EQ(QList<int>() << dialog_provider_id, provider_->cancelled_ids_);

  // The aborted search is not reported.
  provider_->FinishSearch(dialog_provider_id);
  EXPECT_EQ(0, search_finished_spy.count());

  // The automatic cover search still finishes.
  provider_->FinishSearch(playing_provider_id);
  ASSERT_EQ(1, fetched_spy.count());
  EXPECT_EQ(playing_request_id, fetched_spy.first().first().toULongLong());

}

// When the fetcher already has the most requests running, the search in the dialog waits, an aborted search is not started later.
TEST_F(AlbumCoverSearcherTest, AbortedQueuedSearchIsNotStarted) {

  for (int i = 0; i < 5; ++i) {
    fetcher_->FetchAlbumCover(u"Artist"_s, u"Album %1"_s.arg(i), u"Title"_s, true);
  }
  ASSERT_EQ(5, provider_->started_ids_.count());

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  QPushButton *button_search = nullptr;
  SearchInDialog(&searcher, &button_search);
  if (HasFatalFailure()) return;
  ASSERT_EQ(5, provider_->started_ids_.count());
  button_search->click();
  EXPECT_TRUE(provider_->cancelled_ids_.isEmpty());

  // A running request finishes, the queued requests are started by a timer.
  QSignalSpy fetched_spy(&*fetcher_, &AlbumCoverFetcher::AlbumCoverFetched);
  provider_->FinishSearch(provider_->started_ids_.first());
  ASSERT_EQ(1, fetched_spy.count());
  QTest::qWait(1500);

  EXPECT_EQ(5, provider_->started_ids_.count());

}

// Regression test: closing the dialog while a search was running left it running, so when the dialog was opened again, the new search aborted it instead of starting.
TEST_F(AlbumCoverSearcherTest, ReopeningDuringSearchStartsNewSearch) {

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  // Opened and closed before the search finished.
  QTimer::singleShot(0, &searcher, &AlbumCoverSearcher::reject);
  searcher.Exec(u"Artist"_s, u"Album"_s);
  ASSERT_EQ(1, provider_->started_ids_.count());
  const int first_provider_id = provider_->started_ids_.first();
  EXPECT_EQ(QList<int>() << first_provider_id, provider_->cancelled_ids_);

  // Opened again.
  qsizetype started_when_open = 0;
  QTimer::singleShot(0, &searcher, [this, &searcher, &started_when_open]() {
    started_when_open = provider_->started_ids_.count();
    searcher.reject();
  });
  searcher.Exec(u"Other artist"_s, u"Other album"_s);
  EXPECT_EQ(2, started_when_open);

}

// Regression test: a new search, or closing the dialog, did not cancel loading the images of the previous results, so the loader still loaded all of them.
TEST_F(AlbumCoverSearcherTest, ThumbnailsAreCancelled) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  CoverProviderSearchResults results;
  for (int i = 0; i < 3; ++i) {
    const QString filename = temp_dir.filePath(u"cover%1.png"_s.arg(i));
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    ASSERT_TRUE(image.save(filename, "PNG"));
    CoverProviderSearchResult result;
    result.artist = u"Artist"_s;
    result.album = u"Album"_s;
    result.image_url = QUrl::fromLocalFile(filename);
    results << result;
  }

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);
  QAbstractItemView *covers = searcher.findChild<QAbstractItemView*>(u"covers"_s);
  ASSERT_NE(nullptr, covers);

  // Shows the results of the search, only the queued call to the dialog is delivered, so the loader has not loaded the images yet.
  const auto show_results = [this, &searcher, covers, &results]() {
    ASSERT_FALSE(provider_->started_ids_.isEmpty());
    provider_->FinishSearch(provider_->started_ids_.last(), results);
    QCoreApplication::sendPostedEvents(&searcher, QEvent::MetaCall);
    ASSERT_EQ(results.count(), covers->model()->rowCount());
  };

  QSignalSpy loaded_spy(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded);

  // A new search, the dialog is kept open while the loader processes the tasks, closing the dialog cancels them too.
  qsizetype loaded_when_open = -1;
  QTimer::singleShot(0, &searcher, [&searcher, &show_results, &loaded_spy, &loaded_when_open]() {
    const auto close_dialog = qScopeGuard([&searcher]() { searcher.reject(); });
    show_results();
    QPushButton *button_search = searcher.findChild<QPushButton*>(u"search"_s);
    ASSERT_NE(nullptr, button_search);
    button_search->click();
    QTest::qWait(200);
    loaded_when_open = loaded_spy.count();
  });
  searcher.Exec(u"Artist"_s, u"Album"_s);
  EXPECT_EQ(0, loaded_when_open);

  // Closing the dialog.
  QTimer::singleShot(0, &searcher, [&searcher, &show_results]() {
    const auto close_dialog = qScopeGuard([&searcher]() { searcher.reject(); });
    show_results();
  });
  searcher.Exec(u"Artist"_s, u"Album"_s);
  QTest::qWait(200);
  EXPECT_EQ(0, loaded_spy.count());

  // Opened again without searching, the cancelled images are not loading anymore.
  QWidget *busy = searcher.findChild<QWidget*>(u"busy"_s);
  ASSERT_NE(nullptr, busy);
  bool busy_shown = true;
  int rows_shown = -1;
  QTimer::singleShot(0, &searcher, [&searcher, busy, &busy_shown, covers, &rows_shown]() {
    busy_shown = busy->isVisible();
    rows_shown = covers->model()->rowCount();
    searcher.reject();
  });
  searcher.Exec(QString(), QString());
  EXPECT_FALSE(busy_shown);
  // The results without an image can't be chosen, so they are not shown.
  EXPECT_EQ(0, rows_shown);

  // The images of the results which are shown are loaded.
  QTimer::singleShot(0, &searcher, [&searcher, &show_results, &loaded_spy, &results]() {
    const auto close_dialog = qScopeGuard([&searcher]() { searcher.reject(); });
    show_results();
    QTRY_COMPARE_WITH_TIMEOUT(loaded_spy.count(), results.count(), 5000);
  });
  searcher.Exec(u"Artist"_s, u"Album"_s);
  EXPECT_EQ(results.count(), loaded_spy.count());

}

// Regression test: removing the result still loading which was chosen when the dialog was closed made another result current, which was returned instead.
TEST_F(AlbumCoverSearcherTest, ChosenResultStillLoadingIsNotReplaced) {

  // The server accepts the request, but never replies, so the remote image is still loading.
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  QList<QTcpSocket*> sockets;
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &sockets]() {
    while (server.hasPendingConnections()) sockets << server.nextPendingConnection();
  });

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString loaded_filename = temp_dir.filePath(u"loaded.png"_s);
  QImage loaded_image(8, 8, QImage::Format_RGB32);
  loaded_image.fill(Qt::red);
  ASSERT_TRUE(loaded_image.save(loaded_filename, "PNG"));
  CoverProviderSearchResults results;
  for (const QUrl &image_url : {QUrl::fromLocalFile(loaded_filename), QUrl(u"http://127.0.0.1:%1/loading.jpg"_s.arg(server.serverPort()))}) {
    CoverProviderSearchResult result;
    result.artist = u"Artist"_s;
    result.album = u"Album"_s;
    result.image_url = image_url;
    results << result;
  }

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);
  QAbstractItemView *covers = searcher.findChild<QAbstractItemView*>(u"covers"_s);
  ASSERT_NE(nullptr, covers);

  // Returns the row of the result which is loaded, or still loading.
  const auto result_row = [covers](const bool loaded) {
    for (int row = 0; row < covers->model()->rowCount(); ++row) {
      const QModelIndex idx = covers->model()->index(row, 0);
      if (idx.data(AlbumCoverSearcher::Role_ImageFetchFinished).toBool() == loaded) return idx;
    }
    return QModelIndex();
  };

  // The result still loading is chosen.
  QTimer::singleShot(0, &searcher, [this, &searcher, covers, &results, &sockets, &result_row]() {
    const auto close_dialog = qScopeGuard([&searcher]() { searcher.accept(); });
    ASSERT_FALSE(provider_->started_ids_.isEmpty());
    provider_->FinishSearch(provider_->started_ids_.last(), results);
    ASSERT_TRUE(QTest::qWaitFor([&result_row, &sockets]() { return result_row(true).isValid() && !sockets.isEmpty(); }, 5000));
    const QModelIndex loading_index = result_row(false);
    ASSERT_TRUE(loading_index.isValid());
    covers->setCurrentIndex(loading_index);
  });
  const AlbumCoverImageResult loading_result = searcher.Exec(u"Artist"_s, u"Album"_s);
  EXPECT_TRUE(loading_result.image.isNull());
  EXPECT_TRUE(loading_result.image_data.isEmpty());

  // The loaded result is kept, and can be chosen when the dialog is opened again.
  QTimer::singleShot(0, &searcher, [&searcher, covers, &result_row]() {
    const auto close_dialog = qScopeGuard([&searcher]() { searcher.accept(); });
    ASSERT_EQ(1, covers->model()->rowCount());
    const QModelIndex loaded_index = result_row(true);
    ASSERT_TRUE(loaded_index.isValid());
    covers->setCurrentIndex(loaded_index);
  });
  const AlbumCoverImageResult loaded_result = searcher.Exec(QString(), QString());
  EXPECT_EQ(loaded_image.size(), loaded_result.image.size());

}

}  // namespace
