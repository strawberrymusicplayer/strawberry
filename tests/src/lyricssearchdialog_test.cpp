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

#include <functional>

#include "gtest_include.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPlainTextEdit>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "constants/timeconstants.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricssearchrequest.h"
#include "lyrics/lyricssearchresult.h"
#include "songeditor/lyricssearchdialog.h"
#include "songeditor/lyricssearchtype.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {

// Returns the lyrics right away, without using the network.
class FakeLyricsProvider : public LyricsProvider {
 public:
  explicit FakeLyricsProvider(const QString &name, const LyricsSearchResult &result, QObject *parent = nullptr)
      : LyricsProvider(name, true, false, !result.synced_lyrics.isEmpty(), nullptr, parent), result_(result) {}

 protected:
  void StartSearch(const int id, const LyricsSearchRequest &request) override {
    Q_UNUSED(request)
    QTimer::singleShot(0, this, [this, id]() { Q_EMIT SearchFinished(id, LyricsSearchResults() << result_); });
  }

 private:
  const LyricsSearchResult result_;
};

class LyricsSearchDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    qRegisterMetaType<LyricsSearchRequest>("LyricsSearchRequest");
    qRegisterMetaType<LyricsSearchResult>("LyricsSearchResult");
    qRegisterMetaType<LyricsSearchResults>("LyricsSearchResults");
    lyrics_providers_ = make_shared<LyricsProviders>();
    LyricsSearchResult both_result = Result(u"Plain lyrics"_s);
    both_result.synced_lyrics = u"[00:01.00]Synced lyrics"_s;
    lyrics_providers_->AddProvider(new FakeLyricsProvider(u"Both"_s, both_result, &*lyrics_providers_));
    lyrics_providers_->AddProvider(new FakeLyricsProvider(u"Plain"_s, Result(u"Only plain lyrics"_s), &*lyrics_providers_));
    dialog_ = new LyricsSearchDialog(lyrics_providers_);
    tabs_ = dialog_->findChild<QTabWidget*>(u"lyrics_tabs"_s);
    results_ = dialog_->findChild<QTreeWidget*>(u"results"_s);
    lyrics_ = dialog_->findChild<QPlainTextEdit*>(u"lyrics"_s);
    synced_lyrics_ = dialog_->findChild<QPlainTextEdit*>(u"synced_lyrics"_s);
    tab_lyrics_ = dialog_->findChild<QWidget*>(u"tab_lyrics"_s);
    tab_synced_lyrics_ = dialog_->findChild<QWidget*>(u"tab_synced_lyrics"_s);
  }

  void TearDown() override {
    delete dialog_;
  }

  static LyricsSearchResult Result(const QString &lyrics) {
    LyricsSearchResult result(lyrics + QString(100, u' ') + u'.');
    result.artist = u"Artist"_s;
    result.album = u"Album"_s;
    result.title = u"Title"_s;
    return result;
  }

  // Runs the dialog, and calls the function when the results are shown, then closes the dialog.
  void Exec(const LyricsSearchType search_type, const int result_count, const std::function<void()> &function) {
    QTimer::singleShot(0, dialog_, [this, result_count, &function]() {
      QElapsedTimer timer;
      timer.start();
      while (results_->topLevelItemCount() < result_count && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      EXPECT_EQ(result_count, results_->topLevelItemCount());
      function();
      dialog_->reject();
    });
    Song song;
    song.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 100 * kNsecPerSec);
    dialog_->Exec(song, search_type);
  }

  void SelectResult(const QString &provider) {
    const QList<QTreeWidgetItem*> items = results_->findItems(provider, Qt::MatchExactly, 0);
    ASSERT_EQ(1, items.size());
    results_->setCurrentItem(items.first());
  }

  bool TabEnabled(QWidget *tab) const { return tabs_->isTabEnabled(tabs_->indexOf(tab)); }
  bool TabVisible(QWidget *tab) const { return tabs_->isTabVisible(tabs_->indexOf(tab)); }

  SharedPtr<LyricsProviders> lyrics_providers_;
  LyricsSearchDialog *dialog_ = nullptr;
  QTabWidget *tabs_ = nullptr;
  QTreeWidget *results_ = nullptr;
  QPlainTextEdit *lyrics_ = nullptr;
  QPlainTextEdit *synced_lyrics_ = nullptr;
  QWidget *tab_lyrics_ = nullptr;
  QWidget *tab_synced_lyrics_ = nullptr;
};

// When searching for both, the lyrics and synchronized lyrics are shown in separate tabs, instead of both in the same text.
TEST_F(LyricsSearchDialogTest, BothShowsLyricsAndSyncedLyricsInTabs) {

  ASSERT_TRUE(tabs_ && results_ && lyrics_ && synced_lyrics_ && tab_lyrics_ && tab_synced_lyrics_);

  Exec(LyricsSearchType::Both, 2, [this]() {
    EXPECT_TRUE(tabs_->tabBar()->isVisibleTo(dialog_));
    EXPECT_TRUE(TabVisible(tab_lyrics_));
    EXPECT_TRUE(TabVisible(tab_synced_lyrics_));

    SelectResult(u"Both"_s);
    EXPECT_EQ(Result(u"Plain lyrics"_s).lyrics, lyrics_->toPlainText());
    EXPECT_EQ(u"[00:01.00]Synced lyrics"_s, synced_lyrics_->toPlainText());
    EXPECT_TRUE(TabEnabled(tab_lyrics_));
    EXPECT_TRUE(TabEnabled(tab_synced_lyrics_));

    // A result without synchronized lyrics disables the tab, and switches to the lyrics.
    tabs_->setCurrentWidget(tab_synced_lyrics_);
    SelectResult(u"Plain"_s);
    EXPECT_EQ(Result(u"Only plain lyrics"_s).lyrics, lyrics_->toPlainText());
    EXPECT_TRUE(synced_lyrics_->toPlainText().isEmpty());
    EXPECT_TRUE(TabEnabled(tab_lyrics_));
    EXPECT_FALSE(TabEnabled(tab_synced_lyrics_));
    EXPECT_EQ(tab_lyrics_, tabs_->currentWidget());

    SelectResult(u"Both"_s);
    EXPECT_TRUE(TabEnabled(tab_synced_lyrics_));
  });

}

// When searching for synchronized lyrics, only they are shown, without tabs to switch.
TEST_F(LyricsSearchDialogTest, SyncedLyricsShowsOnlySyncedLyrics) {

  ASSERT_TRUE(tabs_ && results_ && lyrics_ && synced_lyrics_ && tab_lyrics_ && tab_synced_lyrics_);

  Exec(LyricsSearchType::SyncedLyrics, 1, [this]() {
    EXPECT_FALSE(tabs_->tabBar()->isVisibleTo(dialog_));
    EXPECT_FALSE(TabVisible(tab_lyrics_));
    EXPECT_TRUE(TabVisible(tab_synced_lyrics_));
    EXPECT_EQ(tab_synced_lyrics_, tabs_->currentWidget());
    SelectResult(u"Both"_s);
    EXPECT_EQ(u"[00:01.00]Synced lyrics"_s, synced_lyrics_->toPlainText());
  });

}

// When searching for lyrics, only they are shown, also after searching for synchronized lyrics before.
TEST_F(LyricsSearchDialogTest, LyricsShowsOnlyLyrics) {

  ASSERT_TRUE(tabs_ && results_ && lyrics_ && synced_lyrics_ && tab_lyrics_ && tab_synced_lyrics_);

  Exec(LyricsSearchType::SyncedLyrics, 1, []() {});
  Exec(LyricsSearchType::Lyrics, 2, [this]() {
    EXPECT_FALSE(tabs_->tabBar()->isVisibleTo(dialog_));
    EXPECT_TRUE(TabVisible(tab_lyrics_));
    EXPECT_FALSE(TabVisible(tab_synced_lyrics_));
    EXPECT_EQ(tab_lyrics_, tabs_->currentWidget());
    SelectResult(u"Both"_s);
    EXPECT_EQ(Result(u"Plain lyrics"_s).lyrics, lyrics_->toPlainText());
  });

}

// Regression test: the dialog is reused, a tab disabled for a result when searching for both stayed disabled when searching for only those lyrics.
TEST_F(LyricsSearchDialogTest, TabDisabledWhenSearchingForBothIsEnabledForNextSearch) {

  ASSERT_TRUE(tabs_ && results_ && lyrics_ && synced_lyrics_ && tab_lyrics_ && tab_synced_lyrics_);

  Exec(LyricsSearchType::Both, 2, [this]() {
    SelectResult(u"Plain"_s);
    EXPECT_FALSE(TabEnabled(tab_synced_lyrics_));
  });
  Exec(LyricsSearchType::SyncedLyrics, 1, [this]() {
    SelectResult(u"Both"_s);
    EXPECT_TRUE(TabEnabled(tab_synced_lyrics_));
    EXPECT_TRUE(tab_synced_lyrics_->isEnabled());
    EXPECT_EQ(tab_synced_lyrics_, tabs_->currentWidget());
    EXPECT_EQ(u"[00:01.00]Synced lyrics"_s, synced_lyrics_->toPlainText());
  });

  // The same for the lyrics tab, with a result which only has synchronized lyrics.
  LyricsSearchResult synced_result = Result(QString());
  synced_result.lyrics.clear();
  synced_result.synced_lyrics = u"[00:02.00]Only synced lyrics"_s;
  lyrics_providers_->AddProvider(new FakeLyricsProvider(u"Synced"_s, synced_result, &*lyrics_providers_));

  Exec(LyricsSearchType::Both, 3, [this]() {
    SelectResult(u"Synced"_s);
    EXPECT_FALSE(TabEnabled(tab_lyrics_));
  });
  Exec(LyricsSearchType::Lyrics, 2, [this]() {
    SelectResult(u"Plain"_s);
    EXPECT_TRUE(TabEnabled(tab_lyrics_));
    EXPECT_TRUE(tab_lyrics_->isEnabled());
    EXPECT_EQ(tab_lyrics_, tabs_->currentWidget());
    EXPECT_EQ(Result(u"Only plain lyrics"_s).lyrics, lyrics_->toPlainText());
  });

}

}  // namespace
