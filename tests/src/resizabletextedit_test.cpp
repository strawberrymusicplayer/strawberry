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

#include "gtest_include.h"

#include <QString>
#include <QFont>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>

#include "widgets/resizabletextedit.h"

using namespace Qt::Literals::StringLiterals;

namespace {

bool HasBoldText(const QTextDocument *document) {

  for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      if (it.fragment().isValid() && it.fragment().charFormat().fontWeight() > QFont::Normal) return true;
    }
  }

  return false;

}

// Marks the first line bold with a separate cursor, like the active line of synchronized lyrics in the context.
void SetFirstLineBold(ResizableTextEdit *textedit) {

  const QTextBlock text_block = textedit->document()->firstBlock();
  QTextCursor cursor(text_block);
  cursor.setPosition(text_block.position() + text_block.length() - 1, QTextCursor::KeepAnchor);
  QTextCharFormat char_format;
  char_format.setFontWeight(QFont::Bold);
  cursor.setCharFormat(char_format);

}

// Regression test: setting new text took the format at the cursor of the text edit, so all the new text was bold when the cursor was in a bold line.
TEST(ResizableTextEditTest, SetPlainTextDoesNotKeepFormat) {

  ResizableTextEdit textedit;
  textedit.setReadOnly(true);

  textedit.SetPlainText(u"Line 1\nLine 2"_s);
  SetFirstLineBold(&textedit);
  ASSERT_TRUE(HasBoldText(textedit.document()));

  textedit.SetPlainText(u"New line 1\nNew line 2"_s);
  EXPECT_FALSE(HasBoldText(textedit.document()));

}

// The user clicked in the bold line, so the cursor of the text edit is there.
TEST(ResizableTextEditTest, SetPlainTextDoesNotKeepFormatAtCursor) {

  ResizableTextEdit textedit;
  textedit.setReadOnly(true);

  textedit.SetPlainText(u"Line 1\nLine 2"_s);
  SetFirstLineBold(&textedit);
  QTextCursor cursor = textedit.textCursor();
  cursor.setPosition(3);
  textedit.setTextCursor(cursor);
  ASSERT_TRUE(textedit.currentCharFormat().fontWeight() > QFont::Normal);

  textedit.SetPlainText(u"New line 1\nNew line 2"_s);
  EXPECT_FALSE(HasBoldText(textedit.document()));

  // Clearing and inserting text with a cursor, like the synchronized lyrics are set, is not bold either.
  SetFirstLineBold(&textedit);
  cursor = textedit.textCursor();
  cursor.setPosition(3);
  textedit.setTextCursor(cursor);
  textedit.Clear();
  QTextCursor insert_cursor(textedit.document());
  insert_cursor.insertText(u"Line 1"_s);
  insert_cursor.insertBlock();
  insert_cursor.insertText(u"Line 2"_s);
  EXPECT_FALSE(HasBoldText(textedit.document()));

}

}  // namespace
