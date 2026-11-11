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
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QMutexLocker>

#include <atomic>
#include <chrono>
#include <thread>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/taskmanager.h"

using std::make_shared;
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

// Returns the first value of every row of a query on a database file, without Database.
QVariantList QueryValues(const QString &filename, const QString &query) {

  const QString connection_name = u"database_test_query_"_s + filename;
  QVariantList values;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_name);
    db.setDatabaseName(filename);
    if (db.open()) {
      QSqlQuery q(db);
      if (q.exec(query)) {
        while (q.next()) {
          values << q.value(0);
        }
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connection_name);

  return values;

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

// Creates the tables of a device in a new database from the device schema, with Unix or Windows line endings, and returns the schema objects of the device.
QString CreateDeviceTables(const QString &filename, const bool windows_line_endings) {

  QFile schema_file(u":/schema/device-schema.sql"_s);
  if (!schema_file.open(QIODevice::ReadOnly)) return QString();
  QString schema = QString::fromUtf8(schema_file.readAll()).replace("\r\n"_L1, "\n"_L1);
  schema.replace("%deviceid"_L1, u"1"_s);
  if (windows_line_endings) {
    schema.replace(u'\n', "\r\n"_L1);
  }

  {
    Database database(nullptr, nullptr, filename);
    if (!database.startup_error().isEmpty()) return QString();
    QSqlDatabase db = database.Connect();
    QString error;
    const bool success = database.ExecSchemaCommands(db, schema, 0, error);
    database.Close();
    if (!success) return QString();
  }

  return QueryValue(filename, u"SELECT group_concat(type || ':' || name, ',') FROM (SELECT type, name FROM sqlite_master WHERE name LIKE 'device\\_1\\_%' ESCAPE '\\' ORDER BY name)"_s).toString();

}

// Regression test: the device schema was split into commands on Unix line endings only, so with Windows line endings only the first command was run.
TEST(DatabaseTest, SchemaWithWindowsLineEndings) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString unix_objects = CreateDeviceTables(temp_dir.filePath(u"unix.db"_s), false);
  ASSERT_GT(unix_objects.count(u','), 1) << unix_objects.toStdString();
  EXPECT_EQ(unix_objects, CreateDeviceTables(temp_dir.filePath(u"windows.db"_s), true));

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

// Adds playlists to the database, so it has more pages.
bool AddPlaylists(const QString &filename, const int count) {

  QStringList commands;
  commands << u"BEGIN"_s;
  for (int i = 0; i < count; ++i) {
    commands << QStringLiteral("INSERT INTO playlists (name) VALUES ('Playlist %1 %2')").arg(i).arg(QString(200, u'x'));
  }
  commands << u"COMMIT"_s;
  return ExecSql(filename, commands);

}

TEST(DatabaseTest, BackupCopiesDatabase) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(AddPlaylists(filename, 10));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(QFile::exists(backup_filename));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));
  EXPECT_EQ(10, QueryValue(backup_filename, u"SELECT COUNT(*) FROM playlists"_s).toInt());

}

// The old backup is replaced in one step, a new backup must replace it.
TEST(DatabaseTest, BackupReplacesOldBackup) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(AddPlaylists(filename, 10));

  {
    Database database(make_shared<TaskManager>(), nullptr, filename);
    ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
    database.DoBackup();
    database.Close();
  }
  ASSERT_EQ(10, QueryValue(backup_filename, u"SELECT COUNT(*) FROM playlists"_s).toInt());

  ASSERT_TRUE(AddPlaylists(filename, 5));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  EXPECT_EQ(15, QueryValue(backup_filename, u"SELECT COUNT(*) FROM playlists"_s).toInt());
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}

// Regression test: the backup held the database mutex during the integrity check and the copy, so the other threads, also the GUI thread, waited for it.
TEST(DatabaseTest, BackupDoesNotHoldDatabaseMutex) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();

  // Another thread holds the database mutex, like a query in another thread, until the backup is done or for 10 seconds.
  constexpr int kMutexHoldMsec = 10000;
  std::atomic<bool> mutex_locked = false;
  std::atomic<bool> backup_done = false;
  std::thread mutex_thread([&database, &mutex_locked, &backup_done]() {
    QMutexLocker l(database.Mutex());
    mutex_locked = true;
    QElapsedTimer timer;
    timer.start();
    while (!backup_done && timer.elapsed() < kMutexHoldMsec) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
  while (!mutex_locked) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  QElapsedTimer timer;
  timer.start();
  database.DoBackup();
  const qint64 backup_msec = timer.elapsed();
  backup_done = true;
  mutex_thread.join();
  database.Close();

  EXPECT_LT(backup_msec, kMutexHoldMsec / 2);
  EXPECT_TRUE(QFile::exists(filename + u".bak"_s));

}

// The backup waits for a write from another connection to finish, like the other database connections do, instead of failing.
TEST(DatabaseTest, BackupWaitsForWrite) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();

  // Another connection adds a playlist in an exclusive transaction, which is committed after the backup has started copying.
  // The backup creates the temporary file just before it copies the database.
  const QString temporary_filename = filename + u".bak.tmp"_s;
  std::atomic<bool> write_started = false;
  std::atomic<bool> backup_started_before_commit = false;
  std::atomic<bool> write_committed = false;
  std::thread write_thread([&filename, &temporary_filename, &write_started, &backup_started_before_commit, &write_committed]() {
    const QString connection_name = u"database_test_write"_s;
    {
      QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_name);
      db.setDatabaseName(filename);
      if (db.open()) {
        QSqlQuery q(db);
        if (q.exec(u"BEGIN EXCLUSIVE"_s) && q.exec(u"INSERT INTO playlists (name) VALUES ('Written during backup')"_s)) {
          write_started = true;
          QElapsedTimer timer;
          timer.start();
          while (!QFile::exists(temporary_filename) && timer.elapsed() < 10000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          backup_started_before_commit = QFile::exists(temporary_filename);
          // Keep the lock until the backup has tried to copy the database.
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
          write_committed = q.exec(u"COMMIT"_s);
        }
      }
      write_started = true;
      db.close();
    }
    QSqlDatabase::removeDatabase(connection_name);
  });
  while (!write_started) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  database.DoBackup();
  write_thread.join();
  database.Close();

  ASSERT_TRUE(backup_started_before_commit);
  ASSERT_TRUE(write_committed);
  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(QFile::exists(backup_filename));
  EXPECT_EQ(1, QueryValue(backup_filename, u"SELECT COUNT(*) FROM playlists WHERE name = 'Written during backup'"_s).toInt());

}

// A database with a newer schema, from a newer version of Strawberry, must not replace the backup.
TEST(DatabaseTest, NewerSchemaIsNotBackedUp) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(ExecSql(filename, QStringList() << QStringLiteral("UPDATE schema_version SET version=%1").arg(Database::kSchemaVersion + 1)));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  EXPECT_FALSE(QFile::exists(filename + u".bak"_s));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}

// A corrupt database must not replace the backup.
TEST(DatabaseTest, CorruptDatabaseKeepsBackup) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(AddPlaylists(filename, 200));

  const QString backup_filename = filename + u".bak"_s;
  {
    Database database(make_shared<TaskManager>(), nullptr, filename);
    ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
    database.DoBackup();
    database.Close();
  }
  const QByteArray backup_data = ReadFile(backup_filename);
  ASSERT_FALSE(backup_data.isEmpty());

  // Overwrite the last page, which has playlists, so it's not a valid page anymore.
  const int page_size = QueryValue(filename, u"PRAGMA page_size"_s).toInt();
  ASSERT_GT(page_size, 0);
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_GT(file.size(), page_size * 2);
    ASSERT_TRUE(file.seek(file.size() - page_size));
    ASSERT_EQ(page_size, file.write(QByteArray(page_size, '\xff')));
  }

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  EXPECT_EQ(backup_data, ReadFile(backup_filename));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}

// A database with more pages than are copied in one step is copied in several steps.
TEST(DatabaseTest, BackupCopiesLargeDatabase) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));

  // The size of the data is set from the page size, which depends on how SQLite is built, so the database has more than two steps of pages.
  const int page_size = QueryValue(filename, u"PRAGMA page_size"_s).toInt();
  ASSERT_GT(page_size, 0);
  QStringList commands = QStringList() << u"CREATE TABLE filler (data BLOB)"_s;
  for (int i = 0; i < 10; ++i) {
    commands << QStringLiteral("INSERT INTO filler (data) VALUES (randomblob(%1))").arg(page_size * 256);
  }
  ASSERT_TRUE(ExecSql(filename, commands));
  ASSERT_GT(QueryValue(filename, u"PRAGMA page_count"_s).toInt(), 2048);
  const QVariantList data = QueryValues(filename, u"SELECT data FROM filler ORDER BY rowid"_s);
  ASSERT_EQ(10, data.count());

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(QFile::exists(backup_filename));
  EXPECT_TRUE(data == QueryValues(backup_filename, u"SELECT data FROM filler ORDER BY rowid"_s));
  EXPECT_EQ(QueryValue(filename, u"PRAGMA page_count"_s).toInt(), QueryValue(backup_filename, u"PRAGMA page_count"_s).toInt());

}

#ifndef Q_OS_WIN32
// The backup must not be readable by other users when the database isn't.
TEST(DatabaseTest, BackupHasDatabasePermissions) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  const QFile::Permissions permissions = QFile::ReadOwner | QFile::WriteOwner;
  ASSERT_TRUE(QFile::setPermissions(filename, permissions));

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(QFile::exists(backup_filename));
  EXPECT_EQ(permissions, QFile::permissions(backup_filename) & (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther));

}

// A read-only database is backed up to a backup which can be replaced the next time.
TEST(DatabaseTest, BackupOfReadOnlyDatabase) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner));
  if (QFileInfo(filename).isWritable()) {
    GTEST_SKIP() << "The database file can't be made read-only, the test is running as root.";
  }

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  database.DoBackup();
  database.Close();

  const QString backup_filename = filename + u".bak"_s;
  ASSERT_TRUE(QFile::exists(backup_filename));
  EXPECT_EQ(QFile::ReadOwner | QFile::WriteOwner, QFile::permissions(backup_filename) & (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther));
  EXPECT_EQ(QVariant(Database::kSchemaVersion), QueryValue(backup_filename, u"SELECT version FROM schema_version"_s));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}
#endif

// A corrupt schema_version table makes reading the schema version fail, the corruption must still be reported.
TEST(DatabaseTest, BackupOfCorruptSchemaVersionIsReported) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));
  const int page_size = QueryValue(filename, u"PRAGMA page_size"_s).toInt();
  const int root_page = QueryValue(filename, u"SELECT rootpage FROM sqlite_master WHERE type = 'table' AND name = 'schema_version'"_s).toInt();
  ASSERT_GT(page_size, 0);
  ASSERT_GT(root_page, 1);

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  QStringList errors;
  QObject::connect(&database, &Database::Error, &database, [&errors](const QString &error) { errors << error; });

  // Overwrite the page of the schema_version table while Strawberry is running.
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.seek(static_cast<qint64>(root_page - 1) * page_size));
    ASSERT_EQ(page_size, file.write(QByteArray(page_size, '\xff')));
  }

  database.DoBackup();
  database.Close();

  EXPECT_FALSE(errors.isEmpty());
  EXPECT_FALSE(QFile::exists(filename + u".bak"_s));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}

// A database which is damaged while Strawberry is running fails the copy, which must be reported and keep the backup.
TEST(DatabaseTest, BackupOfDamagedDatabaseIsReported) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"strawberry.db"_s);
  ASSERT_TRUE(CreateDatabase(filename));

  const QString backup_filename = filename + u".bak"_s;
  {
    Database database(make_shared<TaskManager>(), nullptr, filename);
    ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
    database.DoBackup();
    database.Close();
  }
  const QByteArray backup_data = ReadFile(backup_filename);
  ASSERT_FALSE(backup_data.isEmpty());

  Database database(make_shared<TaskManager>(), nullptr, filename);
  ASSERT_TRUE(database.startup_error().isEmpty()) << database.startup_error().toStdString();
  QStringList errors;
  QObject::connect(&database, &Database::Error, &database, [&errors](const QString &error) { errors << error; });

  // Overwrite the database header, so the file isn't recognized as a database anymore.
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_EQ(16, file.write(QByteArray(16, '\xff')));
  }

  database.DoBackup();
  database.Close();

  ASSERT_EQ(1, errors.count());
  EXPECT_TRUE(errors.first().contains(u"corrupt"_s)) << errors.first().toStdString();
  EXPECT_EQ(backup_data, ReadFile(backup_filename));
  EXPECT_FALSE(QFile::exists(filename + u".bak.tmp"_s));

}

}  // namespace
