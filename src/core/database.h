/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DATABASE_H
#define DATABASE_H

#include "config.h"

#include <sqlite3.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

class QThread;
class Application;

class Database : public QObject {
  Q_OBJECT

 public:
  explicit Database(Application *app, QObject *parent = nullptr, const QString &database_name = QString());
  ~Database() override;

  struct AttachedDatabase {
    AttachedDatabase() {}
    AttachedDatabase(const QString &filename, const QString &schema, bool is_temporary)
        : filename_(filename), schema_(schema), is_temporary_(is_temporary) {}

    QString filename_;
    QString schema_;
    bool is_temporary_;
  };

  static const int kSchemaVersion;
  static const char *kDatabaseFilename;
  static const char *kMagicAllSongsTables;

  void ExitAsync();
  QSqlDatabase Connect();
  void Close();
  bool CheckErrors(const QSqlQuery &query);
  QMutex *Mutex() { return &mutex_; }

  void RecreateAttachedDb(const QString &database_name);
  void ExecSchemaCommands(QSqlDatabase &db, const QString &schema, int schema_version, bool in_transaction = false);

  int startup_schema_version() const { return startup_schema_version_; }
  int current_schema_version() const { return kSchemaVersion; }

  void AttachDatabase(const QString &database_name, const AttachedDatabase &database);
  void AttachDatabaseOnDbConnection(const QString &database_name, const AttachedDatabase &database, QSqlDatabase &db);
  void DetachDatabase(const QString &database_name);

 signals:
  void ExitFinished();
  void Error(const QString &message);

 private slots:
  void Exit();

 public slots:
  void DoBackup();

 private:
  int SchemaVersion(QSqlDatabase *db);
  void UpdateMainSchema(QSqlDatabase *db);

  void ExecSchemaCommandsFromFile(QSqlDatabase &db, const QString &filename, int schema_version, bool in_transaction = false);
  void ExecSongTablesCommands(QSqlDatabase &db, const QStringList &song_tables, const QStringList &commands);

  void UpdateDatabaseSchema(int version, QSqlDatabase &db);
  void UrlEncodeFilenameColumn(const QString &table, QSqlDatabase &db);
  QStringList SongsTables(QSqlDatabase &db, int schema_version) const;
  bool IntegrityCheck(QSqlDatabase db);
  void BackupFile(const QString &filename);
  bool OpenDatabase(const QString &filename, sqlite3 **connection) const;

  Application *app_;

  // Alias -> filename
  QMap<QString, AttachedDatabase> attached_databases_;

  QString directory_;
  QMutex connect_mutex_;
  QMutex mutex_;

  // This ID makes the QSqlDatabase name unique to the object as well as the thread
  int connection_id_;

  static QMutex sNextConnectionIdMutex;
  static int sNextConnectionId;

  // Used by tests
  QString injected_database_name_;

  uint query_hash_;
  QStringList query_cache_;

  // This is the schema version of Strawberry's DB from the app's last run.
  int startup_schema_version_;

  QThread *original_thread_;

};

class MemoryDatabase : public Database {
 public:
  explicit MemoryDatabase(Application *app, QObject *parent = nullptr)
      : Database(app, parent, ":memory:") {}
  ~MemoryDatabase() override {
    // Make sure Qt doesn't reuse the same database
    QSqlDatabase::removeDatabase(Connect().connectionName());
  }
};

#endif  // DATABASE_H
