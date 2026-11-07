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

#include <QString>
#include <QVariant>
#include <QComboBox>
#include <QThreadPool>

#include "includes/shared_ptr.h"
#include "core/memorydatabase.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "smartplaylists/smartplaylistsearchterm.h"
#include "smartplaylists/smartplaylistsearchtermwidget.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {

class SmartPlaylistSearchTermWidgetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = make_shared<MemoryDatabase>(nullptr);
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  }

  void TearDown() override {
    // The tag completers of the fields load the tags in the global thread pool, wait for them, so they don't run after the application is gone.
    QThreadPool::globalInstance()->waitForDone();
  }

  // Selects the field like the user does, the operators are kept when the field has the same type.
  static void SelectField(SmartPlaylistSearchTermWidget *widget, const SmartPlaylistSearchTerm::Field field) {
    QComboBox *combobox_field = widget->findChild<QComboBox*>(u"field"_s);
    ASSERT_NE(nullptr, combobox_field);
    const int index = combobox_field->findData(QVariant::fromValue(field));
    ASSERT_NE(-1, index);
    combobox_field->setCurrentIndex(index);
  }

  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
};

// Regression test: changing the field between two date fields kept the operator, but showed the calendar, so a term like "in the last 7 days" was saved with a date as the number of days, and never matched.
TEST_F(SmartPlaylistSearchTermWidgetTest, ChangingDateFieldKeepsNumericDateValue) {

  SmartPlaylistSearchTermWidget widget(collection_backend_, nullptr);

  SmartPlaylistSearchTerm term;
  term.field_ = SmartPlaylistSearchTerm::Field::LastPlayed;
  term.operator_ = SmartPlaylistSearchTerm::Operator::NumericDate;
  term.value_ = 7;
  term.datetype_ = SmartPlaylistSearchTerm::DateType::Day;
  widget.SetTerm(term);
  ASSERT_EQ(7, widget.Term().value_.toInt());

  SelectField(&widget, SmartPlaylistSearchTerm::Field::DateCreated);
  if (HasFatalFailure()) return;

  const SmartPlaylistSearchTerm changed_term = widget.Term();
  EXPECT_EQ(SmartPlaylistSearchTerm::Field::DateCreated, changed_term.field_);
  EXPECT_EQ(SmartPlaylistSearchTerm::Operator::NumericDate, changed_term.operator_);
  EXPECT_EQ(7, changed_term.value_.toInt());
  EXPECT_EQ(SmartPlaylistSearchTerm::DateType::Day, changed_term.datetype_);

}

TEST_F(SmartPlaylistSearchTermWidgetTest, ChangingDateFieldKeepsRelativeDateValues) {

  SmartPlaylistSearchTermWidget widget(collection_backend_, nullptr);

  SmartPlaylistSearchTerm term;
  term.field_ = SmartPlaylistSearchTerm::Field::LastPlayed;
  term.operator_ = SmartPlaylistSearchTerm::Operator::RelativeDate;
  term.value_ = 3;
  term.second_value_ = 10;
  term.datetype_ = SmartPlaylistSearchTerm::DateType::Week;
  widget.SetTerm(term);

  SelectField(&widget, SmartPlaylistSearchTerm::Field::DateModified);
  if (HasFatalFailure()) return;

  const SmartPlaylistSearchTerm changed_term = widget.Term();
  EXPECT_EQ(SmartPlaylistSearchTerm::Field::DateModified, changed_term.field_);
  EXPECT_EQ(SmartPlaylistSearchTerm::Operator::RelativeDate, changed_term.operator_);
  EXPECT_EQ(3, changed_term.value_.toInt());
  EXPECT_EQ(10, changed_term.second_value_.toInt());
  EXPECT_EQ(SmartPlaylistSearchTerm::DateType::Week, changed_term.datetype_);

}

// Changing to a field of another type replaces the operators and shows the editor for the new type.
TEST_F(SmartPlaylistSearchTermWidgetTest, ChangingToTextFieldShowsTextEditor) {

  SmartPlaylistSearchTermWidget widget(collection_backend_, nullptr);

  SmartPlaylistSearchTerm term;
  term.field_ = SmartPlaylistSearchTerm::Field::LastPlayed;
  term.operator_ = SmartPlaylistSearchTerm::Operator::NumericDate;
  term.value_ = 7;
  term.datetype_ = SmartPlaylistSearchTerm::DateType::Day;
  widget.SetTerm(term);

  SelectField(&widget, SmartPlaylistSearchTerm::Field::Artist);
  if (HasFatalFailure()) return;

  const SmartPlaylistSearchTerm changed_term = widget.Term();
  EXPECT_EQ(SmartPlaylistSearchTerm::Field::Artist, changed_term.field_);
  EXPECT_EQ(QMetaType::QString, changed_term.value_.metaType().id());

}

}  // namespace
