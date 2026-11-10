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
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/database.h"

using namespace Qt::Literals::StringLiterals;

namespace {

QByteArray ReadFile(const QString &filename) {

  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) return QByteArray();
  return file.readAll();

}

// Runs SQL commands on a database file, without Database.
bool ExecSql(const QString &filename, const QStringList &commands) {

  const QString connection_name = u"database_test_"_s + filename;
  bool success = true;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_name);
    db.setDatabaseName(filename);
    success = db.open();
    for (const QString &command : commands) {
      if (!success) break;
      QSqlQuery q(db);
      success = q.exec(command);
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connection_name);

  return success;

}

// Returns the first value of a query on a database file, without Database.
QVariant QueryValue(const QString &filename, const QString &query) {

  const QString connection_name = u"database_test_query_"_s + filename;
  QVariant value;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_name);
    db.setDatabaseName(filename);
    if (db.open()) {
      QSqlQuery q(db);
      if (q.exec(query) && q.next()) {
        value = q.value(0);
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connection_name);

  return value;

}

// Creates a database with the current schema, like the first time Strawberry is started.
bool CreateDatabase(const QString &filename) {

  Database database(nullptr, nullptr, filename);
  const bool success = database.startup_error().isEmpty() && database.startup_schema_version() == Database::kSchemaVersion;
  database.Close();
  return success;

}

TEST(DatabaseTest, NewDatabaseIsCreated) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);

  Database database(nullptr, nullptr, filename);
  EXPECT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  EXPECT_EQ(Database::kSchemaVersion, database.startup_schema_version());
  database.Close();

}

// Regression test: a file which isn't a database was set up as a new database, which failed and aborted Strawberry with qFatal().
TEST(DatabaseTest, NotADatabaseIsAnError) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  const QByteArray data = QByteArray("This is not an SQLite database, it must not be changed.\n").repeated(200);
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(data.size(), file.write(data));
  }

  Database database(nullptr, nullptr, filename);
  const QString error = database.startup_error();
  database.Close();

  EXPECT_FALSE(error.isEmpty());
  EXPECT_TRUE(error.contains(QFileInfo(filename).fileName())) << error.toStdString();
  EXPECT_EQ(data, ReadFile(filename));

}

// Regression test: a database which can't be updated aborted Strawberry with qFatal().
TEST(DatabaseTest, FailedUpdateIsAnError) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(ExecSql(filename, QStringList() << QStringLiteral("UPDATE schema_version SET version=%1").arg(Database::kSchemaVersion - 1)));

  ASSERT_TRUE(QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::ReadUser));
  if (QFileInfo(filename).isWritable()) {
    GTEST_SKIP() << "The database file can't be made read-only, the test is running as root.";
  }
  const QByteArray data = ReadFile(filename);

  Database database(nullptr, nullptr, filename);
  const QString error = database.startup_error();
  database.Close();
  QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

  EXPECT_TRUE(error.contains(QString::number(Database::kSchemaVersion))) << error.toStdString();
  EXPECT_EQ(data, ReadFile(filename));

}

// A schema update which fails after changing data is rolled back, so the data and the schema version are unchanged, and it's updated again the next time.
TEST(DatabaseTest, FailedUpdateIsRolledBack) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));

  // Schema 30 removes the Plex token from plex_songs first, then from playlist_items, which fails when the table is missing.
  const QString art_automatic = u"https://plex.example:32400/library/metadata/1/thumb/2?X-Plex-Token=secret"_s;
  ASSERT_TRUE(ExecSql(filename, QStringList() << u"UPDATE schema_version SET version=29"_s
                                              << QStringLiteral("INSERT INTO plex_songs (url, art_automatic) VALUES ('plex:/library/parts/1/file.flac', '%1')").arg(art_automatic)
                                              << u"DROP TABLE playlist_items"_s));

  Database database(nullptr, nullptr, filename);
  const QString error = database.startup_error();
  database.Close();

  EXPECT_TRUE(error.contains(u"30"_s)) << error.toStdString();
  EXPECT_EQ(29, QueryValue(filename, u"SELECT version FROM schema_version"_s).toInt());
  EXPECT_EQ(art_automatic, QueryValue(filename, u"SELECT art_automatic FROM plex_songs"_s).toString());

}

// Schema commands are not run when their transaction can't be started, so they don't change a transaction already open on the connection.
TEST(DatabaseTest, SchemaCommandsWithoutTransactionChangeNothing) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);

  Database database(nullptr, nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();

  // A transaction which is already open on the same connection makes starting another one fail.
  QSqlDatabase db = database.Connect();
  ASSERT_TRUE(db.transaction());
  QString error;
  const bool success = database.ExecSchemaCommands(db, u"CREATE TABLE schema_test (id INTEGER);\n\nCREATE TABLE schema_test (id INTEGER);"_s, Database::kSchemaVersion, error);
  ASSERT_TRUE(db.commit());
  const bool table_created = db.tables().contains(u"schema_test"_s);
  db = QSqlDatabase();
  database.Close();

  EXPECT_FALSE(success);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_FALSE(table_created);

}

// Regression test: a database with a schema too old to be updated aborted Strawberry with qFatal().
TEST(DatabaseTest, SchemaTooOldIsAnError) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(ExecSql(filename, QStringList() << u"CREATE TABLE schema_version (version INTEGER NOT NULL)"_s << u"INSERT INTO schema_version (version) VALUES (5)"_s));
  const QByteArray data = ReadFile(filename);

  Database database(nullptr, nullptr, filename);
  const QString error = database.startup_error();
  database.Close();

  EXPECT_TRUE(error.contains(u"5"_s)) << error.toStdString();
  EXPECT_EQ(data, ReadFile(filename));

}

}  // namespace
