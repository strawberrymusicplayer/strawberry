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

#include <QCoreApplication>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QTabWidget>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include "widgets/actiondialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class ActionDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    parent_.setGeometry(40, 30, 400, 600);
    parent_.show();
  }

  void TearDown() override {
    if (dialog_) delete dialog_;
  }

  ActionDialog *CreatePage(const ActionDialog::TitlePosition title_position = ActionDialog::TitlePosition::AboveButtons) {
    dialog_ = new ActionDialog(&parent_, u"Actions"_s, u"Song"_s, title_position);
    remove_ = dialog_->AddButton(u"Remove"_s);
    clear_ = dialog_->AddButton(u"Clear"_s);
    cancel_ = dialog_->AddCancelButton(u"Cancel"_s);
    Open();
    return dialog_;
  }

  void Confirm() {
    dialog_->AskConfirmation(u"Continue?"_s, u"Remove all"_s, [this]() {
      ++confirmations_;
      dialog_->accept();
    });
  }

  // Back and Escape are window shortcuts, which require the active window.
  void Open() {
    dialog_->Open();
    EXPECT_TRUE(QTest::qWaitForWindowActive(dialog_));
  }

  ActionDialog *CreateConfirmation() {
    dialog_ = ActionDialog::CreateConfirmation(&parent_, u"Delete playlists"_s, u"Delete it?"_s, u"Delete playlist"_s);
    Open();
    return dialog_;
  }

  int Index(QWidget *widget) const { return dialog_->layout()->indexOf(widget); }

  QWidget parent_;
  QPointer<ActionDialog> dialog_;
  QPushButton *remove_ = nullptr;
  QPushButton *clear_ = nullptr;
  QPushButton *cancel_ = nullptr;
  int confirmations_ = 0;
};

TEST_F(ActionDialogTest, PageListsTitleActionsAndCancelInOrder) {

  CreatePage();
  EXPECT_EQ(dialog_->windowTitle(), u"Actions"_s);
  EXPECT_EQ(dialog_->title_label()->text(), u"Song"_s);
  EXPECT_EQ(dialog_->title_label()->textFormat(), Qt::PlainText);
  EXPECT_TRUE(dialog_->isVisible());
  EXPECT_TRUE(dialog_->testAttribute(Qt::WA_DeleteOnClose));
  EXPECT_LT(Index(dialog_->title_label()), Index(remove_));
  EXPECT_LT(Index(remove_), Index(clear_));
  EXPECT_LT(Index(clear_), Index(cancel_));
  EXPECT_FALSE(dialog_->confirm_button()->isVisible());
  for (QPushButton *button : {remove_, clear_, cancel_}) {
    EXPECT_GE(button->minimumHeight(), 48);
    EXPECT_FALSE(button->autoDefault());
  }

}

TEST_F(ActionDialogTest, TitleCanBeShownAtTheTop) {

  CreatePage(ActionDialog::TitlePosition::Top);
  EXPECT_EQ(Index(dialog_->title_label()), 0);

  delete dialog_;
  CreatePage(ActionDialog::TitlePosition::AboveButtons);
  EXPECT_EQ(Index(dialog_->title_label()) + 1, Index(remove_));

}

TEST_F(ActionDialogTest, BackAndEscapeRejectThePage) {

  for (const Qt::Key key : {Qt::Key_Back, Qt::Key_Escape}) {
    CreatePage();
    QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
    QTest::keyClick(dialog_, key);
    EXPECT_EQ(rejected.count(), 1);
    EXPECT_FALSE(dialog_->isVisible());
    delete dialog_;
  }

}

TEST_F(ActionDialogTest, CancelButtonRejectsThePage) {

  CreatePage();
  QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
  cancel_->click();
  EXPECT_EQ(rejected.count(), 1);

}

TEST_F(ActionDialogTest, ConfirmationReplacesActions) {

  CreatePage();
  Confirm();
  EXPECT_TRUE(dialog_->is_confirming());
  EXPECT_EQ(dialog_->title_label()->text(), u"Continue?"_s);
  EXPECT_FALSE(remove_->isVisible());
  EXPECT_FALSE(clear_->isVisible());
  EXPECT_TRUE(dialog_->confirm_button()->isVisible());
  EXPECT_EQ(dialog_->confirm_button()->text(), u"Remove all"_s);
  EXPECT_TRUE(cancel_->isVisible());
  EXPECT_LT(Index(clear_), Index(dialog_->confirm_button()));
  EXPECT_LT(Index(dialog_->confirm_button()), Index(cancel_));

}

TEST_F(ActionDialogTest, BackReturnsFromConfirmationToActions) {

  CreatePage();
  Confirm();
  QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
  QTest::keyClick(dialog_, Qt::Key_Back);
  EXPECT_EQ(rejected.count(), 0);
  EXPECT_TRUE(dialog_->isVisible());
  EXPECT_FALSE(dialog_->is_confirming());
  EXPECT_EQ(dialog_->title_label()->text(), u"Song"_s);
  EXPECT_TRUE(remove_->isVisible());
  EXPECT_TRUE(clear_->isVisible());
  EXPECT_FALSE(dialog_->confirm_button()->isVisible());
  EXPECT_EQ(confirmations_, 0);

  QTest::keyClick(dialog_, Qt::Key_Back);
  EXPECT_EQ(rejected.count(), 1);
  EXPECT_EQ(confirmations_, 0);

}

TEST_F(ActionDialogTest, ConfirmRunsTheActionOnce) {

  CreatePage();
  Confirm();
  QSignalSpy accepted(dialog_.data(), &QDialog::accepted);
  dialog_->confirm_button()->click();
  EXPECT_EQ(confirmations_, 1);
  EXPECT_EQ(accepted.count(), 1);

}

TEST_F(ActionDialogTest, ConfirmWithoutConfirmationDoesNothing) {

  CreatePage();
  QSignalSpy accepted(dialog_.data(), &QDialog::accepted);
  dialog_->confirm_button()->click();
  EXPECT_EQ(accepted.count(), 0);
  EXPECT_TRUE(dialog_->isVisible());

}

TEST_F(ActionDialogTest, CancelDuringConfirmationRejects) {

  CreatePage();
  Confirm();
  QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
  cancel_->click();
  EXPECT_EQ(rejected.count(), 1);
  EXPECT_EQ(confirmations_, 0);

}

TEST_F(ActionDialogTest, BothStylesCoverTheWindowAndFollowIt) {

  for (const bool confirmation : {false, true}) {
    if (dialog_) delete dialog_;
    parent_.setGeometry(40, 30, 400, 600);
    if (confirmation) {
      CreateConfirmation();
    }
    else {
      CreatePage();
    }
    EXPECT_TRUE(dialog_->windowFlags().testFlag(Qt::FramelessWindowHint));
    EXPECT_EQ(dialog_->windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog_->geometry(), parent_.geometry());
    parent_.setGeometry(50, 20, 360, 500);
    EXPECT_EQ(dialog_->geometry(), parent_.geometry());
  }

}

TEST_F(ActionDialogTest, ConfirmationShowsQuestionAndIconButtons) {

  CreateConfirmation();
  EXPECT_TRUE(dialog_->isVisible());
  EXPECT_EQ(dialog_->windowTitle(), u"Delete playlists"_s);
  EXPECT_TRUE(dialog_->testAttribute(Qt::WA_TranslucentBackground));
  EXPECT_EQ(dialog_->title_label()->text(), u"Delete it?"_s);
  EXPECT_EQ(dialog_->confirm_button()->accessibleName(), u"Delete playlist"_s);
  EXPECT_FALSE(dialog_->cancel_button()->accessibleName().isEmpty());
  EXPECT_FALSE(dialog_->confirm_button()->icon().isNull());
  EXPECT_FALSE(dialog_->cancel_button()->icon().isNull());
  EXPECT_GE(dialog_->confirm_button()->height(), 48);
  EXPECT_GE(dialog_->cancel_button()->width(), 48);

}

TEST_F(ActionDialogTest, ConfirmationAcceptsOnlyWithTheCheckmark) {

  CreateConfirmation();
  QSignalSpy accepted(dialog_.data(), &QDialog::accepted);
  dialog_->confirm_button()->click();
  EXPECT_EQ(accepted.count(), 1);

  for (const bool use_back : {false, true}) {
    delete dialog_;
    CreateConfirmation();
    QSignalSpy cancelled_accept(dialog_.data(), &QDialog::accepted);
    QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
    if (use_back) {
      QTest::keyClick(dialog_, Qt::Key_Back);
    }
    else {
      dialog_->cancel_button()->click();
    }
    EXPECT_EQ(rejected.count(), 1);
    EXPECT_EQ(cancelled_accept.count(), 0);
  }

}

TEST_F(ActionDialogTest, PageInsideATabCoversOnlyTheTabPage) {

  QTabWidget tabs;
  QWidget *page = new QWidget;
  QWidget *other_page = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(page);
  QWidget *view = new QWidget(page);
  layout->addWidget(view);
  tabs.addTab(page, u"Playlist"_s);
  tabs.addTab(other_page, u"Files"_s);
  tabs.setGeometry(40, 30, 400, 600);
  tabs.show();
  EXPECT_TRUE(QTest::qWaitForWindowExposed(&tabs));

  dialog_ = new ActionDialog(view, u"Actions"_s, u"Song"_s, ActionDialog::TitlePosition::Top);
  dialog_->AddCancelButton(u"Cancel"_s);
  dialog_->Open();
  EXPECT_FALSE(dialog_->isWindow());
  EXPECT_EQ(dialog_->parentWidget(), page);
  EXPECT_EQ(dialog_->windowModality(), Qt::NonModal);
  EXPECT_TRUE(dialog_->autoFillBackground());
  EXPECT_EQ(dialog_->geometry(), page->rect());
  tabs.resize(360, 500);
  EXPECT_EQ(dialog_->geometry(), page->rect());

  QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
  tabs.setCurrentWidget(other_page);
  EXPECT_EQ(rejected.count(), 1);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_TRUE(dialog_.isNull());

}

TEST_F(ActionDialogTest, BackRejectsThePageInsideATab) {

  QTabWidget tabs;
  QWidget *page = new QWidget;
  tabs.addTab(page, u"Playlist"_s);
  tabs.show();
  tabs.activateWindow();
  EXPECT_TRUE(QTest::qWaitForWindowActive(&tabs));

  dialog_ = new ActionDialog(page, u"Actions"_s, u"Song"_s, ActionDialog::TitlePosition::Top);
  dialog_->AddCancelButton(u"Cancel"_s);
  dialog_->Open();
  QSignalSpy rejected(dialog_.data(), &QDialog::rejected);
  QTest::keyClick(dialog_->cancel_button(), Qt::Key_Back);
  EXPECT_EQ(rejected.count(), 1);

}

TEST_F(ActionDialogTest, ConfirmationInsideATabStillCoversTheWindow) {

  QTabWidget tabs;
  QWidget *page = new QWidget;
  tabs.addTab(page, u"Playlists"_s);
  tabs.setGeometry(40, 30, 400, 600);
  tabs.show();
  EXPECT_TRUE(QTest::qWaitForWindowExposed(&tabs));

  dialog_ = ActionDialog::CreateConfirmation(page, u"Delete playlists"_s, u"Delete it?"_s, u"Delete playlist"_s);
  Open();
  EXPECT_TRUE(dialog_->isWindow());
  EXPECT_EQ(dialog_->windowModality(), Qt::ApplicationModal);
  EXPECT_EQ(dialog_->geometry(), tabs.geometry());

}

TEST_F(ActionDialogTest, ClosedDialogDeletesItself) {

  CreatePage();
  cancel_->click();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_TRUE(dialog_.isNull());

}

}  // namespace
