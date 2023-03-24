/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <sqlite3.h>
#include <boost/scope_exit.hpp>

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
#include <QStandardPaths>

#include "core/logging.h"
#include "taskmanager.h"
#include "database.h"
#include "application.h"
#include "sqlquery.h"
#include "scopedtransaction.h"

const char *Database::kDatabaseFilename = "strawberry.db";
const int Database::kSchemaVersion = 16;
const int Database::kMinSupportedSchemaVersion = 10;
const char *Database::kMagicAllSongsTables = "%allsongstables";

int Database::sNextConnectionId = 1;
QMutex Database::sNextConnectionIdMutex;

Database::Database(Application *app, QObject *parent, const QString &database_name) :
      QObject(parent),
      app_(app),
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
      mutex_(QMutex::Recursive),
#endif
      injected_database_name_(database_name),
      query_hash_(0),
      startup_schema_version_(-1),
      original_thread_(nullptr) {

  original_thread_ = thread();

  {
    QMutexLocker l(&sNextConnectionIdMutex);
    connection_id_ = sNextConnectionId++;
  }

  directory_ = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

  QMutexLocker l(&mutex_);
  Connect();

}

Database::~Database() {

  QMutexLocker l(&connect_mutex_);

  for (QString &connection_id : QSqlDatabase::connectionNames()) {
    qLog(Error) << "Connection" << connection_id << "is still open!";
  }

}

void Database::ExitAsync() {
  QMetaObject::invokeMethod(this, "Exit", Qt::QueuedConnection);
}

void Database::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());
  Close();
  moveToThread(original_thread_);
  emit ExitFinished();

}

QSqlDatabase Database::Connect() {

  QMutexLocker l(&connect_mutex_);

  // Create the directory if it doesn't exist
  if (!QFile::exists(directory_)) {
    QDir dir;
    if (!dir.mkpath(directory_)) {
    }
  }

  const QString connection_id = QString("%1_thread_%2").arg(connection_id_).arg(reinterpret_cast<quint64>(QThread::currentThread()));

  // Try to find an existing connection for this thread
  QSqlDatabase db;
  if (QSqlDatabase::connectionNames().contains(connection_id)) {
    db = QSqlDatabase::database(connection_id);
  }
  else {
    db = QSqlDatabase::addDatabase("QSQLITE", connection_id);
  }
  if (db.isOpen()) {
    return db;
  }
  db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=30000");
  //qLog(Debug) << "Opened database with connection id" << connection_id;

  if (injected_database_name_.isNull()) {
    db.setDatabaseName(directory_ + "/" + kDatabaseFilename);
  }
  else {
    db.setDatabaseName(injected_database_name_);
  }

  if (!db.open()) {
    app_->AddError("Database: " + db.lastError().text());
    return db;
  }

  if (db.tables().count() == 0) {
    // Set up initial schema
    qLog(Info) << "Creating initial database schema";
    UpdateDatabaseSchema(0, db);
  }

  if (SchemaVersion(&db) < kMinSupportedSchemaVersion) {
    qFatal("Database schema too old.");
  }

  // Attach external databases
  QStringList keys = attached_databases_.keys();
  for (const QString &key : keys) {
    QString filename = attached_databases_[key].filename_;

    if (!injected_database_name_.isNull()) filename = injected_database_name_;

    // Attach the db
    SqlQuery q(db);
    q.prepare("ATTACH DATABASE :filename AS :alias");
    q.BindValue(":filename", filename);
    q.BindValue(":alias", key);
    if (!q.Exec()) {
      qFatal("Couldn't attach external database '%s'", key.toLatin1().constData());
    }
  }

  if (startup_schema_version_ == -1) {
    UpdateMainSchema(&db);
  }

  // We might have to initialize the schema in some attached databases now, if they were deleted and don't match up with the main schema version.
  keys = attached_databases_.keys();
  for (const QString &key : keys) {
    if (attached_databases_[key].is_temporary_ && attached_databases_[key].schema_.isEmpty()) {
      continue;
    }
    // Find out if there are any tables in this database
    SqlQuery q(db);
    q.prepare(QString("SELECT ROWID FROM %1.sqlite_master WHERE type='table'").arg(key));
    if (!q.Exec() || !q.next()) {
      q.finish();
      ExecSchemaCommandsFromFile(db, attached_databases_[key].schema_, 0);
    }
  }

  return db;

}

void Database::Close() {

  QMutexLocker l(&connect_mutex_);

  const QString connection_id = QString("%1_thread_%2").arg(connection_id_).arg(reinterpret_cast<quint64>(QThread::currentThread()));

  // Try to find an existing connection for this thread
  if (QSqlDatabase::connectionNames().contains(connection_id)) {
    {
      QSqlDatabase db = QSqlDatabase::database(connection_id);
      if (db.isOpen()) {
        db.close();
        //qLog(Debug) << "Closed database with connection id" << connection_id;
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
    q.prepare("SELECT version FROM schema_version");
    if (q.Exec() && q.next()) {
      schema_version = q.value(0).toInt();
    }
    // Implicit invocation of ~SqlQuery() when leaving the scope to release any remaining database locks!
  }
  return schema_version;

}

void Database::UpdateMainSchema(QSqlDatabase *db) {

  int schema_version = SchemaVersion(db);
  startup_schema_version_ = schema_version;

  if (schema_version > kSchemaVersion) {
    qLog(Warning) << "The database schema (version" << schema_version << ") is newer than I was expecting";
    return;
  }
  if (schema_version < kSchemaVersion) {
    // Update the schema
    for (int v = schema_version + 1; v <= kSchemaVersion; ++v) {
      UpdateDatabaseSchema(v, *db);
    }
  }
}

void Database::RecreateAttachedDb(const QString &database_name) {

  if (!attached_databases_.contains(database_name)) {
    qLog(Warning) << "Attached database does not exist:" << database_name;
    return;
  }

  const QString filename = attached_databases_[database_name].filename_;

  QMutexLocker l(&mutex_);
  {
    QSqlDatabase db(Connect());

    SqlQuery q(db);
    q.prepare("DETACH DATABASE :alias");
    q.BindValue(":alias", database_name);
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
  for (const QString &name : QSqlDatabase::connectionNames()) {
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
  q.prepare("ATTACH DATABASE :filename AS :alias");
  q.BindValue(":filename", database.filename_);
  q.BindValue(":alias", database_name);
  if (!q.Exec()) {
    qFatal("Couldn't attach external database '%s'", database_name.toLatin1().constData());
  }

}

void Database::DetachDatabase(const QString &database_name) {

  QMutexLocker l(&mutex_);
  {
    QSqlDatabase db(Connect());

    SqlQuery q(db);
    q.prepare("DETACH DATABASE :alias");
    q.BindValue(":alias", database_name);
    if (!q.Exec()) {
      qLog(Warning) << "Failed to detach database" << database_name;
      return;
    }
  }

  attached_databases_.remove(database_name);

}

void Database::UpdateDatabaseSchema(int version, QSqlDatabase &db) {

  QString filename;
  if (version == 0) {
    filename = ":/schema/schema.sql";
  }
  else {
    filename = QString(":/schema/schema-%1.sql").arg(version);
    qLog(Debug) << "Applying database schema update" << version << "from" << filename;
  }

  ExecSchemaCommandsFromFile(db, filename, version - 1);

}

void Database::UrlEncodeFilenameColumn(const QString &table, QSqlDatabase &db) {

  SqlQuery select(db);
  select.prepare(QString("SELECT ROWID, filename FROM %1").arg(table));
  SqlQuery update(db);
  update.prepare(QString("UPDATE %1 SET filename=:filename WHERE ROWID=:id").arg(table));
  if (!select.Exec()) {
    ReportErrors(select);
  }

  while (select.next()) {
    const int rowid = select.value(0).toInt();
    const QString filename = select.value(1).toString();

    if (filename.isEmpty() || filename.contains("://")) {
      continue;
    }

    const QUrl url = QUrl::fromLocalFile(filename);

    update.BindValue(":filename", url.toEncoded());
    update.BindValue(":id", rowid);
    if (!update.Exec()) {
      ReportErrors(update);
    }
  }

}

void Database::ExecSchemaCommandsFromFile(QSqlDatabase &db, const QString &filename, int schema_version, bool in_transaction) {

  // Open and read the database schema
  QFile schema_file(filename);
  if (!schema_file.open(QIODevice::ReadOnly)) {
    qFatal("Couldn't open schema file %s for reading: %s", filename.toUtf8().constData(), schema_file.errorString().toUtf8().constData());
  }
  QByteArray data = schema_file.readAll();
  QString schema = QString::fromUtf8(data);
  if (schema.contains("\r\n")) {
    schema = schema.replace("\r\n", "\n");
  }
  schema_file.close();
  ExecSchemaCommands(db, schema, schema_version, in_transaction);

}

void Database::ExecSchemaCommands(QSqlDatabase &db, const QString &schema, int schema_version, bool in_transaction) {

  // Run each command
  QStringList commands;
  commands = schema.split(QRegularExpression("; *\n\n"));

  // We don't want this list to reflect possible DB schema changes, so we initialize it before executing any statements.
  // If no outer transaction is provided the song tables need to be queried before beginning an inner transaction!
  // Otherwise DROP TABLE commands on song tables may fail due to database locks.
  const QStringList song_tables(SongsTables(db, schema_version));

  if (!in_transaction) {
    ScopedTransaction inner_transaction(&db);
    ExecSongTablesCommands(db, song_tables, commands);
    inner_transaction.Commit();
  }
  else {
    ExecSongTablesCommands(db, song_tables, commands);
  }

}

void Database::ExecSongTablesCommands(QSqlDatabase &db, const QStringList &song_tables, const QStringList &commands) {

  for (const QString &command : commands) {
    // There are now lots of "songs" tables that need to have the same schema: songs and device_*_songs.
    // We allow a magic value in the schema files to update all songs tables at once.
    if (command.contains(kMagicAllSongsTables)) {
      for (const QString &table : song_tables) {
        // Another horrible hack: device songs tables don't have matching _fts tables, so if this command tries to touch one, ignore it.
        if (table.startsWith("device_") && command.contains(QString(kMagicAllSongsTables) + "_fts")) {
          continue;
        }

        qLog(Info) << "Updating" << table << "for" << kMagicAllSongsTables;
        QString new_command(command);
        new_command.replace(kMagicAllSongsTables, table);
        SqlQuery query(db);
        query.prepare(new_command);
        if (!query.Exec()) {
          ReportErrors(query);
          qFatal("Unable to update music collection database");
        }
      }
    }
    else {
      SqlQuery query(db);
      query.prepare(command);
      if (!query.Exec()) {
        ReportErrors(query);
        qFatal("Unable to update music collection database");
      }
    }
  }

}

QStringList Database::SongsTables(QSqlDatabase &db, const int schema_version) {

  Q_UNUSED(schema_version);

  QStringList ret;

  // look for the tables in the main db
  for (const QString &table : db.tables()) {
    if (table == "songs" || table.endsWith("_songs")) ret << table;
  }

  // look for the tables in attached dbs
  QStringList keys = attached_databases_.keys();
  for (const QString &key : keys) {
    SqlQuery q(db);
    q.prepare(QString("SELECT NAME FROM %1.sqlite_master WHERE type='table' AND name='songs' OR name LIKE '%songs'").arg(key));
    if (q.Exec()) {
      while (q.next()) {
        QString tab_name = key + "." + q.value(0).toString();
        ret << tab_name;
      }
    }
    else {
      ReportErrors(q);
    }
  }

  ret << "playlist_items";

  return ret;

}

void Database::ReportErrors(const SqlQuery &query) {

  const QSqlError sql_error = query.lastError();
  if (sql_error.isValid()) {
    qLog(Error) << "Unable to execute SQL query: " << sql_error;
    qLog(Error) << "Failed query: " << query.LastQuery();
    QString error;
    error += "Unable to execute SQL query: " + sql_error.text() + "<br />";
    error += "Failed query: " + query.LastQuery();
    emit Error(error);
  }

}

bool Database::IntegrityCheck(const QSqlDatabase &db) {

  qLog(Debug) << "Starting database integrity check";
  const int task_id = app_->task_manager()->StartTask(tr("Integrity check"));

  bool ok = false;
  bool error_reported = false;
  // Ask for 10 error messages at most.
  SqlQuery q(db);
  q.prepare("PRAGMA integrity_check(10)");
  if (q.Exec()) {
    while (q.next()) {
      QString message = q.value(0).toString();

      // If no errors are found, a single row with the value "ok" is returned
      if (message == "ok") {
        ok = true;
        break;
      }
      else {
        if (!error_reported) { app_->AddError(tr("Database corruption detected.")); }
        app_->AddError("Database: " + message);
        error_reported = true;
      }
    }
  }
  else {
    ReportErrors(q);
  }

  app_->task_manager()->SetTaskFinished(task_id);

  return ok;

}

void Database::DoBackup() {

  QSqlDatabase db(Connect());

  if (!db.isOpen()) return;

  // Before we overwrite anything, make sure the database is not corrupt
  QMutexLocker l(&mutex_);

  const bool ok = IntegrityCheck(db);
  if (ok && SchemaVersion(&db) == kSchemaVersion) {
    BackupFile(db.databaseName());
  }

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
  QString dest_filename = QString("%1.bak").arg(filename);
  const int task_id = app_->task_manager()->StartTask(tr("Backing up database"));

  sqlite3 *source_connection = nullptr;
  sqlite3 *dest_connection = nullptr;

  BOOST_SCOPE_EXIT((&source_connection)(&dest_connection)(task_id)(app_)) {  // clazy:exclude=rule-of-three NOLINT(google-explicit-constructor)
    // Harmless to call sqlite3_close() with a nullptr pointer.
    sqlite3_close(source_connection);
    sqlite3_close(dest_connection);
    app_->task_manager()->SetTaskFinished(task_id);
  }
  BOOST_SCOPE_EXIT_END

  bool success = OpenDatabase(filename, &source_connection);
  if (!success) {
    return;
  }

  success = OpenDatabase(dest_filename, &dest_connection);
  if (!success) {
    return;
  }

  sqlite3_backup *backup = sqlite3_backup_init(dest_connection, "main", source_connection, "main");
  if (!backup) {
    const char *error_message = sqlite3_errmsg(dest_connection);
    qLog(Error) << "Failed to start database backup:" << error_message;
    return;
  }

  int ret = SQLITE_OK;
  do {
    ret = sqlite3_backup_step(backup, 16);
    const int page_count = sqlite3_backup_pagecount(backup);
    app_->task_manager()->SetTaskProgress(task_id, page_count - sqlite3_backup_remaining(backup), page_count);
  }
  while (ret == SQLITE_OK);

  if (ret != SQLITE_DONE) {
    qLog(Error) << "Database backup failed";
  }

  sqlite3_backup_finish(backup);

}
