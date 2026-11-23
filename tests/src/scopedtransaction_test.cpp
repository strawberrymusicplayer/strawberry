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

#include <QtGlobal>
#include <QString>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/scopedtransaction.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class ScopedTransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {

    ASSERT_TRUE(database_dir_.isValid());
    db_ = QSqlDatabase::addDatabase(u"QSQLITE"_s, kConnectionName);
    db_.setDatabaseName(database_dir_.filePath(u"test.db"_s));
    ASSERT_TRUE(db_.open());

    // A deferred foreign key is only checked when the transaction is committed, so a missing parent makes the commit fail.
    QSqlQuery q(db_);
    ASSERT_TRUE(q.exec(u"PRAGMA foreign_keys = ON"_s));
    ASSERT_TRUE(q.exec(u"CREATE TABLE parent (id INTEGER PRIMARY KEY)"_s));
    ASSERT_TRUE(q.exec(u"CREATE TABLE child (parent_id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED)"_s));

  }

  void TearDown() override {
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnectionName);
  }

  int ChildCount() {
    QSqlQuery q(db_);
    if (!q.exec(u"SELECT COUNT(*) FROM child"_s) || !q.next()) return -1;
    return q.value(0).toInt();
  }

  static constexpr QLatin1StringView kConnectionName = "scopedtransaction_test"_L1;
  QTemporaryDir database_dir_;
  QSqlDatabase db_;
};

TEST_F(ScopedTransactionTest, CommitSucceeds) {

  ScopedTransaction t(&db_);
  ASSERT_TRUE(t.started());
  QSqlQuery q(db_);
  ASSERT_TRUE(q.exec(u"INSERT INTO parent (id) VALUES (1)"_s));
  ASSERT_TRUE(q.exec(u"INSERT INTO child (parent_id) VALUES (1)"_s));

  EXPECT_TRUE(t.Commit());
  EXPECT_TRUE(t.error().isEmpty()) << t.error().toStdString();
  EXPECT_EQ(1, ChildCount());

}

// The error from a failed commit is saved before the transaction is rolled back, so the rollback doesn't replace it.
TEST_F(ScopedTransactionTest, FailedCommitKeepsError) {

  ScopedTransaction t(&db_);
  ASSERT_TRUE(t.started());
  QSqlQuery q(db_);
  ASSERT_TRUE(q.exec(u"INSERT INTO child (parent_id) VALUES (1)"_s));

  EXPECT_FALSE(t.Commit());
  EXPECT_TRUE(t.error().contains(u"FOREIGN KEY"_s)) << t.error().toStdString();
  EXPECT_EQ(0, ChildCount());

}

TEST_F(ScopedTransactionTest, FailedStartKeepsError) {

  // A transaction which is already open on the same connection makes starting another one fail.
  ASSERT_TRUE(db_.transaction());
  ScopedTransaction t(&db_);

  EXPECT_FALSE(t.started());
  EXPECT_FALSE(t.error().isEmpty());
  EXPECT_TRUE(db_.rollback());

}

}  // namespace
