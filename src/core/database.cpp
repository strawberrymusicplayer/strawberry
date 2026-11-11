/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <filesystem>
#include <system_error>

#include <sqlite3.h>

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QSqlDriver>
#include <QSqlDatabase>
#include <QSqlError>
#include <QScopeGuard>

#include "logging.h"
#include "standardpaths.h"
#include "taskmanager.h"
#include "database.h"
#include "sqlquery.h"
#include "scopedtransaction.h"

using namespace Qt::Literals::StringLiterals;

const int Database::kSchemaVersion = 30;

namespace {
constexpr char kDatabaseFilename[] = "strawberry.db";
constexpr int kMinSupportedSchemaVersion = 10;
constexpr char kMagicAllSongsTables[] = "%allsongstables";
// How long to wait for another connection to finish writing, before a query fails because the database is locked.
constexpr int kBusyTimeoutMsec = 30000;

// The backup is copied in steps, so the database isn't locked for writing during the whole copy.
constexpr int kBackupStepPages = 1024;

// A write from another connection restarts the copy, after this many restarts the rest is copied in one step, so the backup finishes while the collection is scanned.
constexpr int kBackupMaxRestarts = 3;

std::filesystem::path FilesystemPath(const QString &filename) {
#ifdef Q_OS_WIN32
  return std::filesystem::path(filename.toStdWString());
#else
  return std::filesystem::path(QFile::encodeName(filename).toStdString());
#endif
}

}  // namespace

int Database::sNextConnectionId = 1;
QMutex Database::sNextConnectionIdMutex;

Database::Database(SharedPtr<TaskManager> task_manager, QObject *parent, const QString &database_name)
    : QObject(parent),
      task_manager_(task_manager),
      injected_database_name_(database_name),
      query_hash_(0),
      startup_schema_version_(-1),
      original_thread_(nullptr) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  original_thread_ = thread();

  {
    QMutexLocker l(&sNextConnectionIdMutex);
    connection_id_ = sNextConnectionId++;
  }

  directory_ = QDir::toNativeSeparators(StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation)).replace(u"Strawberry"_s, u"strawberry"_s);

  QMutexLocker l(&mutex_);
  Connect();

}

Database::~Database() {

  QMutexLocker l(&connect_mutex_);

  const QStringList connection_names = QSqlDatabase::connectionNames();
  for (const QString &connection_id : connection_names) {
    qLog(Error) << "Connection" << connection_id << "is still open!";
  }

}

void Database::ExitAsync() {
  QMetaObject::invokeMethod(this, &Database::Exit, Qt::QueuedConnection);
}

void Database::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());
  Close();
  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

QSqlDatabase Database::Connect() {

  QMutexLocker l(&connect_mutex_);

  // Create the directory if it doesn't exist
  if (!QFile::exists(directory_)) {
    QDir dir;
    if (!dir.mkpath(directory_)) {
      qLog(Error) << "Failed to create database directory" << directory_;
    }
  }

  const QString connection_id = QStringLiteral("%1_thread_%2").arg(connection_id_).arg(reinterpret_cast<quint64>(QThread::currentThread()));

  // Try to find an existing connection for this thread
  QSqlDatabase db;
  if (QSqlDatabase::connectionNames().contains(connection_id)) {
    db = QSqlDatabase::database(connection_id);
  }
  else {
    db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_id);
  }
  if (db.isOpen()) {
    return db;
  }
  db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=%1").arg(kBusyTimeoutMsec));
  // qLog(Debug) << "Opened database with connection id" << connection_id;

  if (injected_database_name_.isNull()) {
    db.setDatabaseName(directory_ + u'/' + QLatin1String(kDatabaseFilename));
  }
  else {
    db.setDatabaseName(injected_database_name_);
  }

  if (!db.open()) {
    Q_EMIT Error(u"Database: "_s + db.lastError().text());
    SetStartupError(db, db.lastError().text());
    return db;
  }

  // A file which isn't a database, or is corrupt, can't be read, so it must not be set up as a new database.
  int table_count = 0;
  {
    SqlQuery q(db);
    if (!q.prepare(u"SELECT COUNT(*) FROM sqlite_master WHERE type='table'"_s) || !q.Exec() || !q.next()) {
      SetStartupError(db, q.lastError().text());
      return db;
    }
    table_count = q.value(0).toInt();
  }

  if (table_count == 0) {
    // Set up initial schema
    qLog(Info) << "Creating initial database schema";
    QString error;
    if (!UpdateDatabaseSchema(0, db, error)) {
      SetStartupError(db, tr("Could not create the database: %1").arg(error));
      return db;
    }
  }

  const int schema_version = SchemaVersion(&db);
  if (schema_version < kMinSupportedSchemaVersion) {
    SetStartupError(db, tr("The database has schema version %1, which is too old to be updated, the oldest supported version is %2.").arg(schema_version).arg(kMinSupportedSchemaVersion));
    return db;
  }

  // Attach external databases
  QStringList keys = attached_databases_.keys();
  for (const QString &key : std::as_const(keys)) {
    QString filename = attached_databases_.value(key).filename_;

    if (!injected_database_name_.isNull()) filename = injected_database_name_;

    // Attach the db
    SqlQuery q(db);
    q.prepare(u"ATTACH DATABASE :filename AS :alias"_s);
    q.BindValue(u":filename"_s, filename);
    q.BindValue(u":alias"_s, key);
    if (!q.Exec()) {
      SetStartupError(db, tr("Could not attach the database %1: %2").arg(key, q.lastError().text()));
      return db;
    }
  }

  if (startup_schema_version_ == -1) {
    QString error;
    if (!UpdateMainSchema(&db, error)) {
      SetStartupError(db, error);
      return db;
    }
  }

  // We might have to initialize the schema in some attached databases now, if they were deleted and don't match up with the main schema version.
  keys = attached_databases_.keys();
  for (const QString &key : std::as_const(keys)) {
    if (attached_databases_.value(key).is_temporary_ && attached_databases_.value(key).schema_.isEmpty()) {
      continue;
    }
    // Find out if there are any tables in this database
    SqlQuery q(db);
    q.prepare(QStringLiteral("SELECT ROWID FROM %1.sqlite_master WHERE type='table'").arg(key));
    if (!q.Exec() || !q.next()) {
      q.finish();
      QString error;
      if (!ExecSchemaCommandsFromFile(db, attached_databases_.value(key).schema_, 0, error)) {
        SetStartupError(db, tr("Could not create the database %1: %2").arg(key, error));
        return db;
      }
    }
  }

  return db;

}

void Database::Close() {

  QMutexLocker l(&connect_mutex_);

  const QString connection_id = QStringLiteral("%1_thread_%2").arg(connection_id_).arg(reinterpret_cast<quint64>(QThread::currentThread()));

  // Try to find an existing connection for this thread
  if (QSqlDatabase::connectionNames().contains(connection_id)) {
    {
      QSqlDatabase db = QSqlDatabase::database(connection_id);
      if (db.isOpen()) {
        db.close();
        // qLog(Debug) << "Closed database with connection id" << connection_id;
      }
    }
    QSqlDatabase::removeDatabase(connection_id);
  }

}

int Database::SchemaVersion(QSqlDatabase *db) {

  // Get the database's schema version
  int schema_version = 0;
  {
    SqlQuery q(*db);
    q.prepare(u"SELECT version FROM schema_version"_s);
    if (q.Exec() && q.next()) {
      schema_version = q.value(0).toInt();
    }
    // Implicit invocation of ~SqlQuery() when leaving the scope to release any remaining database locks!
  }
  return schema_version;

}

int Database::SchemaVersion(sqlite3 *connection) {

  int schema_version = 0;
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(connection, "SELECT version FROM schema_version", -1, &statement, nullptr) == SQLITE_OK) {
    if (sqlite3_step(statement) == SQLITE_ROW) {
      schema_version = sqlite3_column_int(statement, 0);
    }
  }
  sqlite3_finalize(statement);

  return schema_version;

}

bool Database::UpdateMainSchema(QSqlDatabase *db, QString &error) {

  int schema_version = SchemaVersion(db);
  startup_schema_version_ = schema_version;

  if (schema_version > kSchemaVersion) {
    qLog(Warning) << "The database schema (version" << schema_version << ") is newer than I was expecting";
    return true;
  }
  if (schema_version < kSchemaVersion) {
    // Update the schema
    for (int v = schema_version + 1; v <= kSchemaVersion; ++v) {
      QString update_error;
      if (!UpdateDatabaseSchema(v, *db, update_error)) {
        error = tr("Could not update the database to schema version %1: %2").arg(v).arg(update_error);
        return false;
      }
    }
  }

  return true;

}

QString Database::startup_error() {

  QMutexLocker l(&connect_mutex_);
  return startup_error_;

}

void Database::SetStartupError(const QSqlDatabase &db, const QString &error) {

  qLog(Error) << "Database" << db.databaseName() << error;

  // The first error is the cause, the errors after it are caused by it.
  if (startup_error_.isEmpty()) {
    startup_error_ = tr("Strawberry can't use the database %1.").arg(QDir::toNativeSeparators(db.databaseName())) + u"\n\n"_s + error;
  }

}

void Database::RecreateAttachedDb(const QString &database_name) {

  if (!attached_databases_.contains(database_name)) {
    qLog(Warning) << "Attached database does not exist:" << database_name;
    return;
  }

  const QString filename = attached_databases_.value(database_name).filename_;

  QMutexLocker l(&mutex_);
  {
    QSqlDatabase db(Connect());

    SqlQuery q(db);
    q.prepare(u"DETACH DATABASE :alias"_s);
    q.BindValue(u":alias"_s, database_name);
    if (!q.Exec()) {
      qLog(Warning) << "Failed to detach database" << database_name;
      return;
    }

    if (!QFile::remove(filename)) {
      qLog(Warning) << "Failed to remove file" << filename;
    }
  }

  // We can't just re-attach the database now because it needs to be done for each thread.
  // Close all the database connections, so each thread will re-attach it when they next connect.
  const QStringList connection_names = QSqlDatabase::connectionNames();
  for (const QString &name : connection_names) {
    QSqlDatabase::removeDatabase(name);
  }

}

void Database::AttachDatabase(const QString &database_name, const AttachedDatabase &database) {
  attached_databases_[database_name] = database;
}

void Database::AttachDatabaseOnDbConnection(const QString &database_name, const AttachedDatabase &database, QSqlDatabase &db) {

  AttachDatabase(database_name, database);

  // Attach the db
  SqlQuery q(db);
  q.prepare(u"ATTACH DATABASE :filename AS :alias"_s);
  q.BindValue(u":filename"_s, database.filename_);
  q.BindValue(u":alias"_s, database_name);
  if (!q.Exec()) {
    qFatal("Couldn't attach external database '%s'", database_name.toLatin1().constData());
  }

}

void Database::DetachDatabase(const QString &database_name) {

  QMutexLocker l(&mutex_);
  {
    QSqlDatabase db(Connect());

    SqlQuery q(db);
    q.prepare(u"DETACH DATABASE :alias"_s);
    q.BindValue(u":alias"_s, database_name);
    if (!q.Exec()) {
      qLog(Warning) << "Failed to detach database" << database_name;
      return;
    }
  }

  attached_databases_.remove(database_name);

}

bool Database::UpdateDatabaseSchema(const int version, QSqlDatabase &db, QString &error) {

  QString filename;
  if (version == 0) {
    filename = u":/schema/schema.sql"_s;
  }
  else {
    filename = QStringLiteral(":/schema/schema-%1.sql").arg(version);
    qLog(Debug) << "Applying database schema update" << version << "from" << filename;
  }

  return ExecSchemaCommandsFromFile(db, filename, version - 1, error);

}

void Database::UrlEncodeFilenameColumn(const QString &table, QSqlDatabase &db) {

  SqlQuery select(db);
  select.prepare(QStringLiteral("SELECT ROWID, filename FROM %1").arg(table));
  SqlQuery update(db);
  update.prepare(QStringLiteral("UPDATE %1 SET filename=:filename WHERE ROWID=:id").arg(table));
  if (!select.Exec()) {
    ReportErrors(select);
  }

  while (select.next()) {
    const int rowid = select.value(0).toInt();
    const QString filename = select.value(1).toString();

    if (filename.isEmpty() || filename.contains("://"_L1)) {
      continue;
    }

    const QUrl url = QUrl::fromLocalFile(filename);

    update.BindValue(u":filename"_s, url.toEncoded());
    update.BindValue(u":id"_s, rowid);
    if (!update.Exec()) {
      ReportErrors(update);
    }
  }

}

bool Database::ExecSchemaCommandsFromFile(QSqlDatabase &db, const QString &filename, const int schema_version, QString &error, const bool in_transaction) {

  // Open and read the database schema
  QFile schema_file(filename);
  if (!schema_file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Couldn't open schema file" << filename << "for reading:" << schema_file.errorString();
    error = QStringLiteral("%1: %2").arg(filename, schema_file.errorString());
    return false;
  }
  QByteArray data = schema_file.readAll();
  QString schema = QString::fromUtf8(data);
  if (schema.contains("\r\n"_L1)) {
    schema = schema.replace("\r\n"_L1, "\n"_L1);
  }
  schema_file.close();

  return ExecSchemaCommands(db, schema, schema_version, error, in_transaction);

}

bool Database::ExecSchemaCommands(QSqlDatabase &db, const QString &schema, const int schema_version, QString &error, const bool in_transaction) {

  // Run each command
  static const QRegularExpression regex_split_commands(u"; *\n\n"_s);
  QStringList commands = schema.split(regex_split_commands);

  // We don't want this list to reflect possible DB schema changes, so we initialize it before executing any statements.
  // If no outer transaction is provided the song tables need to be queried before beginning an inner transaction!
  // Otherwise DROP TABLE commands on song tables may fail due to database locks.
  const QStringList song_tables(SongsTables(db, schema_version));

  // On failure, the inner transaction is rolled back, an outer transaction is rolled back by the caller.
  if (!in_transaction) {
    ScopedTransaction inner_transaction(&db);
    // Without a transaction, the commands would change a transaction already open on the connection, which is not rolled back here.
    if (!inner_transaction.started()) {
      error = inner_transaction.error();
      return false;
    }
    if (!ExecSongTablesCommands(db, song_tables, commands, error)) {
      return false;
    }
    if (!inner_transaction.Commit()) {
      error = inner_transaction.error();
      return false;
    }
    return true;
  }

  return ExecSongTablesCommands(db, song_tables, commands, error);

}

bool Database::ExecSongTablesCommands(QSqlDatabase &db, const QStringList &song_tables, const QStringList &commands, QString &error) {

  for (const QString &command : commands) {
    // There are now lots of "songs" tables that need to have the same schema: songs and device_*_songs.
    // We allow a magic value in the schema files to update all songs tables at once.
    if (command.contains(QLatin1String(kMagicAllSongsTables))) {
      for (const QString &table : song_tables) {
        qLog(Info) << "Updating" << table << "for" << kMagicAllSongsTables;
        QString new_command(command);
        new_command.replace(QLatin1String(kMagicAllSongsTables), table);
        SqlQuery query(db);
        // When preparing fails, executing replaces the error with "No query".
        if (!query.prepare(new_command) || !query.Exec()) {
          ReportErrors(query);
          error = query.lastError().text();
          return false;
        }
      }
    }
    else {
      SqlQuery query(db);
      if (!query.prepare(command) || !query.Exec()) {
        ReportErrors(query);
        error = query.lastError().text();
        return false;
      }
    }
  }

  return true;

}

QStringList Database::SongsTables(QSqlDatabase &db, const int schema_version) {

  Q_UNUSED(schema_version);

  QStringList ret;

  // look for the tables in the main db
  const QStringList &tables = db.tables();
  for (const QString &table : tables) {
    if (table == "songs"_L1 || table.endsWith("_songs"_L1)) ret << table;
  }

  // look for the tables in attached dbs
  const QStringList keys = attached_databases_.keys();
  for (const QString &key : keys) {
    SqlQuery q(db);
    q.prepare(QStringLiteral("SELECT NAME FROM %1.sqlite_master WHERE type='table' AND (name='songs' OR name LIKE '%songs')").arg(key));
    if (q.Exec()) {
      while (q.next()) {
        QString tab_name = key + QLatin1Char('.') + q.value(0).toString();
        ret << tab_name;
      }
    }
    else {
      ReportErrors(q);
    }
  }

  ret << u"playlist_items"_s;

  return ret;

}

void Database::ReportErrors(const SqlQuery &query) {

  const QSqlError sql_error = query.lastError();
  if (sql_error.isValid()) {
    qLog(Error) << "Unable to execute SQL query:" << sql_error;
    qLog(Error) << "Failed SQL query:" << query.LastQuery();
    Q_EMIT Error(tr("Unable to execute SQL query: %1").arg(sql_error.text()));
    Q_EMIT Error(tr("Failed SQL query: %1").arg(query.LastQuery()));
  }

}

void Database::ReportError(const QString &error) {

  qLog(Error) << error;
  Q_EMIT Error(error);

}

bool Database::IntegrityCheck(sqlite3 *connection) {

  qLog(Debug) << "Starting database integrity check";
  const int task_id = task_manager_->StartTask(tr("Integrity check"));
  const QScopeGuard integrity_check_finish = qScopeGuard([this, task_id]() { task_manager_->SetTaskFinished(task_id); });

  // Ask for 10 error messages at most.
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(connection, "PRAGMA integrity_check(10)", -1, &statement, nullptr) != SQLITE_OK) {
    ReportError(u"Database integrity check failed: "_s + QString::fromUtf8(sqlite3_errmsg(connection)));
    return false;
  }

  bool ok = false;
  bool error_reported = false;
  int ret = SQLITE_OK;
  while ((ret = sqlite3_step(statement)) == SQLITE_ROW) {
    const QString message = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));

    // If no errors are found, a single row with the value "ok" is returned
    if (message == "ok"_L1) {
      ok = true;
      break;
    }

    if (!error_reported) {
      Q_EMIT Error(tr("Database corruption detected."));
    }
    Q_EMIT Error(u"Database: "_s + message);
    error_reported = true;
  }

  if (ret != SQLITE_ROW && ret != SQLITE_DONE) {
    ReportError(u"Database integrity check failed: "_s + QString::fromUtf8(sqlite3_errmsg(connection)));
  }

  sqlite3_finalize(statement);

  return ok;

}

void Database::DoBackup() {

  QSqlDatabase db(Connect());

  if (!db.isOpen()) return;

  // The database mutex is not held, since it would block the other threads, also the GUI thread, during the integrity check and the copy.
  // SQLite locks the database while it's copied, and the schema version and integrity are checked on the copy.
  BackupFile(db.databaseName());

}

bool Database::OpenDatabase(const QString &filename, sqlite3 **connection) {

  const QByteArray filename_data = filename.toUtf8();
  int ret = sqlite3_open(filename_data.constData(), connection);
  if (ret != 0) {
    if (*connection) {
      const char *error_message = sqlite3_errmsg(*connection);
      qLog(Error) << "Failed to open database for backup:" << filename << error_message;
    }
    else {
      qLog(Error) << "Failed to open database for backup:" << filename;
    }
    return false;
  }
  return true;

}

void Database::BackupFile(const QString &filename) {

  qLog(Debug) << "Starting database backup";
  const QString backup_filename = QStringLiteral("%1.bak").arg(filename);
  // The database is copied to a temporary file, which only replaces the backup when it passes the integrity check, so a corrupt database doesn't replace the backup.
  const QString temporary_filename = QStringLiteral("%1.bak.tmp").arg(filename);
  const int task_id = task_manager_->StartTask(tr("Backing up database"));

  sqlite3 *source_connection = nullptr;
  sqlite3 *dest_connection = nullptr;

  const QScopeGuard db_backup_finish = qScopeGuard([this, task_id, &source_connection, &dest_connection, &temporary_filename]() {
    if (source_connection) {
      sqlite3_close(source_connection);
    }
    if (dest_connection) {
      sqlite3_close(dest_connection);
    }
    if (QFile::exists(temporary_filename)) {
      QFile::remove(temporary_filename);
    }
    task_manager_->SetTaskFinished(task_id);
  });

  if (QFile::exists(temporary_filename) && !QFile::remove(temporary_filename)) {
    qLog(Error) << "Failed to remove old temporary database backup" << temporary_filename;
    return;
  }

  // The copy gets the same permissions as the database, so a database which is only readable by the owner isn't copied to a file which other users can read.
  // The copy is always writable by the owner, since SQLite can't copy a read-only database to a read-only file, and a read-only backup can't be replaced on Windows.
  {
    QFile temporary_file(temporary_filename);
    if (!temporary_file.open(QIODevice::WriteOnly | QIODevice::NewOnly, QFile::permissions(filename) | QFile::WriteOwner)) {
      qLog(Error) << "Failed to create temporary database backup" << temporary_filename << temporary_file.errorString();
      return;
    }
    temporary_file.close();
  }

  bool success = OpenDatabase(filename, &source_connection);
  if (!success) {
    return;
  }

  success = OpenDatabase(temporary_filename, &dest_connection);
  if (!success) {
    return;
  }

  // Wait for writes from other connections like the other database connections do, instead of failing the backup.
  sqlite3_busy_timeout(source_connection, kBusyTimeoutMsec);
  sqlite3_busy_timeout(dest_connection, kBusyTimeoutMsec);

  sqlite3_backup *backup = sqlite3_backup_init(dest_connection, "main", source_connection, "main");
  if (!backup) {
    const char *error_message = sqlite3_errmsg(dest_connection);
    qLog(Error) << "Failed to start database backup:" << error_message;
    return;
  }

  // The database is locked for writing during each step, between the steps other connections can write to it.
  int ret = SQLITE_OK;
  int restarts = 0;
  int pages_copied = 0;
  do {
    ret = sqlite3_backup_step(backup, restarts < kBackupMaxRestarts ? kBackupStepPages : -1);
    const int page_count = sqlite3_backup_pagecount(backup);
    const int pages_copied_after_step = page_count - sqlite3_backup_remaining(backup);
    if (ret == SQLITE_OK && pages_copied_after_step <= pages_copied) {
      ++restarts;
    }
    pages_copied = pages_copied_after_step;
    task_manager_->SetTaskProgress(task_id, static_cast<quint64>(pages_copied), static_cast<quint64>(page_count));
  } while (ret == SQLITE_OK);
  sqlite3_backup_finish(backup);

  if (ret != SQLITE_DONE) {
    const int primary_result_code = ret & 0xFF;
    if (primary_result_code == SQLITE_CORRUPT || primary_result_code == SQLITE_NOTADB) {
      ReportError(u"Database backup failed, the database is corrupt: "_s + QString::fromUtf8(sqlite3_errstr(ret)));
    }
    else {
      qLog(Error) << "Database backup failed:" << sqlite3_errstr(ret);
    }
    return;
  }

  sqlite3_close(source_connection);
  source_connection = nullptr;

  // The pages are copied as they are, so the copy is corrupt if the database is.
  // The integrity is checked first, since reading the schema version fails when the schema_version table is corrupt, and the corruption must be reported.
  if (!IntegrityCheck(dest_connection)) {
    qLog(Error) << "Database integrity check failed, the database backup is not replaced.";
    return;
  }

  const int schema_version = SchemaVersion(dest_connection);
  if (schema_version != kSchemaVersion) {
    qLog(Error) << "Database schema version" << schema_version << "is not" << kSchemaVersion << ", the database backup is not replaced.";
    return;
  }

  sqlite3_close(dest_connection);
  dest_connection = nullptr;

  // Replace the old backup in one step, so it's kept if the new backup can't be moved in place, or Strawberry is stopped.
  // QFile::rename() doesn't replace an existing file, std::filesystem::rename() does, with rename() on Unix and MoveFileExW() on Windows.
  std::error_code error_code;
  std::filesystem::rename(FilesystemPath(temporary_filename), FilesystemPath(backup_filename), error_code);
  if (error_code) {
    qLog(Error) << "Failed to replace database backup" << backup_filename << "with" << temporary_filename << QString::fromStdString(error_code.message());
    return;
  }

  qLog(Debug) << "Database backup finished";

}
