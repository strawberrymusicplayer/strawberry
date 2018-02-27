/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef APPLICATION_H_
#define APPLICATION_H_

#include "config.h"

#include <memory>

#include <QObject>

#include "settings/settingsdialog.h"

class ApplicationImpl;
class TagReaderClient;
class Database;
class Appearance;
class TaskManager;
class Player;
class DeviceManager;
class Collection;
class PlaylistBackend;
class PlaylistManager;
class AlbumCoverLoader;
class CoverProviders;
class CurrentArtLoader;
class CollectionBackend;
class CollectionModel;
class EngineDevice;

class Application : public QObject {
  Q_OBJECT

 public:
  static bool kIsPortable;

  explicit Application(QObject *parent = nullptr);
  ~Application();

  const QString &language_name() const { return language_name_; }
  // Same as language_name, but remove the region code at the end if there is one
  QString language_without_region() const;
  void set_language_name(const QString &name) { language_name_ = name; }

  TagReaderClient *tag_reader_client() const;
  Database *database() const;
  Appearance *appearance() const;
  TaskManager *task_manager() const;
  Player *player() const;
  EngineDevice *enginedevice() const;
  DeviceManager *device_manager() const;

  Collection *collection() const;
  
  PlaylistBackend *playlist_backend() const;
  PlaylistManager *playlist_manager() const;

  CoverProviders *cover_providers() const;
  AlbumCoverLoader *album_cover_loader() const;
  CurrentArtLoader *current_art_loader() const;
  
  CollectionBackend *collection_backend() const;
  CollectionModel *collection_model() const;

  void MoveToNewThread(QObject *object);
  void MoveToThread(QObject *object, QThread *thread);

 public slots:
  void AddError(const QString &message);
  void ReloadSettings();
  void OpenSettingsDialogAtPage(SettingsDialog::Page page);

signals:
  void ErrorAdded(const QString &message);
  void SettingsChanged();
  void SettingsDialogRequested(SettingsDialog::Page page);

 private:
  QString language_name_;
  std::unique_ptr<ApplicationImpl> p_;
  QList<QThread*> threads_;

};

#endif  // APPLICATION_H_
