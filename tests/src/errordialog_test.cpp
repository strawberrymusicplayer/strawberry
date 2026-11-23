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

#include "gtest_include.h"

#include <QString>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTest>

#include "dialogs/errordialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class ErrorDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    messages_ = dialog_.findChild<QTextEdit*>(u"messages"_s);
    ASSERT_TRUE(messages_);
  }

  // Shows two messages, so the dialog is open with both.
  void ShowMessages() {
    dialog_.ShowMessage(u"First error"_s);
    dialog_.ShowDialog();
    dialog_.ShowMessage(u"Second error"_s);
    ASSERT_TRUE(dialog_.isVisible());
    ASSERT_TRUE(messages_->toPlainText().contains(u"First error"_s));
    ASSERT_TRUE(messages_->toPlainText().contains(u"Second error"_s));
  }

  // Shows a new message, which should be the only message in the dialog.
  void ExpectOnlyNewMessage() {
    dialog_.ShowMessage(u"Third error"_s);
    dialog_.ShowDialog();
    EXPECT_FALSE(messages_->toPlainText().contains(u"First error"_s));
    EXPECT_FALSE(messages_->toPlainText().contains(u"Second error"_s));
    EXPECT_EQ(messages_->toPlainText(), u"Third error"_s);
  }

  ErrorDialog dialog_;
  QTextEdit *messages_ = nullptr;
};

TEST_F(ErrorDialogTest, EscapeClearsMessages) {

  ShowMessages();

  QTest::keyClick(&dialog_, Qt::Key_Escape);
  ASSERT_FALSE(dialog_.isVisible());

  ExpectOnlyNewMessage();

}

TEST_F(ErrorDialogTest, CloseButtonClearsMessages) {

  ShowMessages();

  QDialogButtonBox *buttonbox = dialog_.findChild<QDialogButtonBox*>(u"buttonbox"_s);
  ASSERT_TRUE(buttonbox);
  QPushButton *close_button = buttonbox->button(QDialogButtonBox::Close);
  ASSERT_TRUE(close_button);
  close_button->click();
  ASSERT_FALSE(dialog_.isVisible());

  ExpectOnlyNewMessage();

}

TEST_F(ErrorDialogTest, CloseClearsMessages) {

  ShowMessages();

  dialog_.close();
  ASSERT_FALSE(dialog_.isVisible());

  ExpectOnlyNewMessage();

}

}  // namespace
