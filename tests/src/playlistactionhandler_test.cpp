/*
 * Strawberry Music Player
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

#include "gtest_include.h"

#include <memory>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTest>
#include <QTouchEvent>
#include <QTreeView>
#include <QUrl>

#include "core/mimedata.h"
#include "widgets/playlistactionhandler.h"

using namespace Qt::Literals::StringLiterals;

class PlaylistActionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    interval_ = QApplication::styleHints()->mousePressAndHoldInterval();
    QApplication::styleHints()->setMousePressAndHoldInterval(20);
    for (int i = 0; i < 80; ++i) {
      model_.appendRow(new QStandardItem(i == 0 ? u"Album <one>"_s : QString::number(i)));
    }
    view_.setModel(&model_);
    view_.resize(320, 240);
    handler_ = new PlaylistActionHandler(&view_, u"Collection"_s, {u"Add album to current playlist"_s, u"Queue album"_s, u"Queue album to play next"_s},
        [](const QModelIndex &index) { return index.row() != 1; },
        [this](const QModelIndex &index) {
          source_ = index;
          auto *data = new MimeData;
          data->name_for_new_playlist_ = index.data().toString();
          data->setUrls({QUrl(u"file:///owned/album/song.wav"_s)});
          return data;
        });
    QObject::connect(handler_, &PlaylistActionHandler::AddToPlaylist, &view_, [this](QMimeData *data) {
      result_.reset(qobject_cast<MimeData *>(data));
      ++emissions_;
    });
    view_.show();
    QCoreApplication::processEvents();
  }

  void TearDown() override {
    QApplication::styleHints()->setMousePressAndHoldInterval(interval_);
  }

  void Press(const int row = 0) {
    QTest::mousePress(view_.viewport(), Qt::LeftButton, Qt::NoModifier, view_.visualRect(model_.index(row, 0)).center());
  }

  QDialog *Hold() {
    Press();
    QTest::qWait(60);
    QTest::mouseRelease(view_.viewport(), Qt::LeftButton, Qt::NoModifier, view_.visualRect(model_.index(0, 0)).center());
    return Dialog();
  }

  QDialog *Dialog() {
    for (QDialog *dialog : view_.findChildren<QDialog *>()) {
      if (dialog->isVisible()) return dialog;
    }
    return nullptr;
  }

  QPushButton *Button(QDialog *dialog, const QString &text) {
    for (QPushButton *button : dialog->findChildren<QPushButton *>()) {
      if (button->text() == text) return button;
    }
    return nullptr;
  }

  int interval_ = 0;
  PlaylistActionHandler *handler_ = nullptr;
  QStandardItemModel model_;
  QListView view_;
  QModelIndex source_;
  std::unique_ptr<MimeData> result_;
  int emissions_ = 0;
};

TEST_F(PlaylistActionHandlerTest, CreateUsesHeldItemAndOverridesPlaybackPreferences) {

  view_.selectionModel()->select(model_.index(2, 0), QItemSelectionModel::Select);
  QSignalSpy clicked(&view_, &QListView::clicked);
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  EXPECT_EQ(0, clicked.count());
  QLabel *title = dialog->findChild<QLabel *>();
  ASSERT_NE(nullptr, title);
  EXPECT_EQ(Qt::PlainText, title->textFormat());
  EXPECT_EQ(u"Collection > Album <one>"_s, title->text());
  QPushButton *create = Button(dialog, u"Create saved playlist"_s);
  ASSERT_NE(nullptr, create);
  EXPECT_GE(create->minimumHeight(), 48);
  QTest::mouseClick(create, Qt::LeftButton);
  ASSERT_NE(nullptr, result_);
  EXPECT_EQ(1, emissions_);
  EXPECT_EQ(model_.index(0, 0), source_);
  EXPECT_TRUE(result_->override_user_settings_);
  EXPECT_TRUE(result_->open_in_new_playlist_);
  EXPECT_FALSE(result_->play_now_);
  EXPECT_FALSE(result_->from_doubleclick_);
  EXPECT_FALSE(result_->clear_first_);
  EXPECT_FALSE(result_->enqueue_now_);
  EXPECT_FALSE(result_->enqueue_next_now_);
  EXPECT_EQ(u"Album <one>"_s, result_->name_for_new_playlist_);
  EXPECT_EQ(1, result_->urls().count());
  EXPECT_EQ(nullptr, Dialog());

}

TEST_F(PlaylistActionHandlerTest, CreateAndPlayStartsPlaybackExplicitly) {

  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *play = Button(dialog, u"Create and play"_s);
  ASSERT_NE(nullptr, play);
  QTest::mouseClick(play, Qt::LeftButton);
  ASSERT_NE(nullptr, result_);
  EXPECT_TRUE(result_->override_user_settings_);
  EXPECT_TRUE(result_->open_in_new_playlist_);
  EXPECT_TRUE(result_->play_now_);

}

TEST_F(PlaylistActionHandlerTest, AddToCurrentPlaylistDoesNotApplyUserPlaybackPreferences) {

  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *add = Button(dialog, u"Add album to current playlist"_s);
  ASSERT_NE(nullptr, add);
  QTest::mouseClick(add, Qt::LeftButton);
  ASSERT_NE(nullptr, result_);
  EXPECT_TRUE(result_->override_user_settings_);
  EXPECT_FALSE(result_->open_in_new_playlist_);
  EXPECT_FALSE(result_->play_now_);
  EXPECT_FALSE(result_->enqueue_now_);
  EXPECT_FALSE(result_->enqueue_next_now_);

}

TEST_F(PlaylistActionHandlerTest, QueueActionsSetExplicitQueueFlags) {

  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *enqueue = Button(dialog, u"Queue album"_s);
  ASSERT_NE(nullptr, enqueue);
  QTest::mouseClick(enqueue, Qt::LeftButton);
  ASSERT_NE(nullptr, result_);
  EXPECT_TRUE(result_->override_user_settings_);
  EXPECT_FALSE(result_->play_now_);
  EXPECT_TRUE(result_->enqueue_now_);
  EXPECT_FALSE(result_->enqueue_next_now_);

  result_.reset();
  dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *enqueue_next = Button(dialog, u"Queue album to play next"_s);
  ASSERT_NE(nullptr, enqueue_next);
  QTest::mouseClick(enqueue_next, Qt::LeftButton);
  ASSERT_NE(nullptr, result_);
  EXPECT_TRUE(result_->override_user_settings_);
  EXPECT_FALSE(result_->play_now_);
  EXPECT_FALSE(result_->enqueue_now_);
  EXPECT_TRUE(result_->enqueue_next_now_);

}

TEST_F(PlaylistActionHandlerTest, BackDoesNotCreatePlaylistOrMoveParent) {

  const int scroll = view_.verticalScrollBar()->value();
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *back = Button(dialog, u"Back"_s);
  ASSERT_NE(nullptr, back);
  QTest::mouseClick(back, Qt::LeftButton);
  EXPECT_EQ(0, emissions_);
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(scroll, view_.verticalScrollBar()->value());

}

TEST_F(PlaylistActionHandlerTest, AndroidBackDoesNotCreatePlaylist) {

  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QTest::keyClick(dialog, Qt::Key_Back);
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, ShortTapStillClicksNormally) {

  QSignalSpy clicked(&view_, &QListView::clicked);
  const QPoint position = view_.visualRect(model_.index(0, 0)).center();
  QTest::mouseClick(view_.viewport(), Qt::LeftButton, Qt::NoModifier, position);
  QTest::qWait(60);
  EXPECT_EQ(1, clicked.count());
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, MovingOrScrollingCancelsLongPress) {

  Press();
  QTest::mouseMove(view_.viewport(), QPoint(300, 180));
  QTest::qWait(60);
  EXPECT_EQ(nullptr, Dialog());
  QTest::mouseRelease(view_.viewport(), Qt::LeftButton);
  Press();
  view_.verticalScrollBar()->setValue(10);
  QTest::qWait(60);
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, IneligibleRowsDoNotOpenActions) {

  Press(1);
  QTest::qWait(60);
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, RemovedOrResetItemsCannotBeUsed) {

  Press();
  model_.removeRow(0);
  QTest::qWait(60);
  EXPECT_EQ(nullptr, Dialog());
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  model_.clear();
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, HidingParentCancelsActions) {

  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  view_.hide();
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, TouchCancelStopsPendingGestureButKeepsOpenActions) {

  Press();
  QTouchEvent cancelled(QEvent::TouchCancel);
  QCoreApplication::sendEvent(view_.viewport(), &cancelled);
  QTest::qWait(60);
  EXPECT_EQ(nullptr, Dialog());
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QCoreApplication::sendEvent(view_.viewport(), &cancelled);
  EXPECT_EQ(dialog, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, TapAfterNativeTouchCancellationStillWorks) {

  Press();
  QTest::qWait(60);
  QDialog *dialog = Dialog();
  ASSERT_NE(nullptr, dialog);
  QTouchEvent cancelled(QEvent::TouchCancel);
  QCoreApplication::sendEvent(view_.viewport(), &cancelled);
  QTest::keyClick(dialog, Qt::Key_Back);
  QSignalSpy clicked(&view_, &QListView::clicked);
  const QPoint position = view_.visualRect(model_.index(1, 0)).center();
  QTest::mouseClick(view_.viewport(), Qt::LeftButton, Qt::NoModifier, position);
  EXPECT_EQ(1, clicked.count());
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, ExtraActionRunsForHeldItem) {

  QModelIndex triggered;
  handler_->AddAction(u"Open homepage"_s, [](const QModelIndex &index) { return index.row() == 0; }, [&triggered](const QModelIndex &index) { triggered = index; });
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  QPushButton *homepage = Button(dialog, u"Open homepage"_s);
  ASSERT_NE(nullptr, homepage);
  EXPECT_GE(homepage->minimumHeight(), 48);
  QTest::mouseClick(homepage, Qt::LeftButton);
  EXPECT_EQ(model_.index(0, 0), triggered);
  EXPECT_EQ(nullptr, Dialog());
  EXPECT_EQ(0, emissions_);

}

TEST_F(PlaylistActionHandlerTest, UnavailableExtraActionIsHidden) {

  bool triggered = false;
  handler_->AddAction(u"Donate"_s, [](const QModelIndex &) { return false; }, [&triggered](const QModelIndex &) { triggered = true; });
  QDialog *dialog = Hold();
  ASSERT_NE(nullptr, dialog);
  EXPECT_EQ(nullptr, Button(dialog, u"Donate"_s));
  EXPECT_NE(nullptr, Button(dialog, u"Create and play"_s));
  QTest::keyClick(dialog, Qt::Key_Back);
  EXPECT_FALSE(triggered);

}

TEST_F(PlaylistActionHandlerTest, TitleUsesFirstColumnOfHeldRow) {

  QStandardItemModel model;
  model.appendRow({new QStandardItem(u"Station"_s), new QStandardItem(u"Country"_s)});
  QTreeView view;
  view.setModel(&model);
  view.resize(320, 240);
  new PlaylistActionHandler(&view, u"Radios"_s, {u"Add"_s, u"Queue"_s, u"Queue next"_s}, [](const QModelIndex &) { return true; }, [](const QModelIndex &) { return new MimeData; });
  view.show();
  QCoreApplication::processEvents();
  const QPoint position = view.visualRect(model.index(0, 1)).center();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, position);
  QTest::qWait(60);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, position);
  QDialog *dialog = view.findChild<QDialog *>();
  ASSERT_NE(nullptr, dialog);
  QLabel *title = dialog->findChild<QLabel *>();
  ASSERT_NE(nullptr, title);
  EXPECT_EQ(u"Radios > Station"_s, title->text());

}
