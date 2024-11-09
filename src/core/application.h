/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef APPLICATION_H
#define APPLICATION_H

#include "config.h"

#include <glib.h>

#include <QObject>
#include <QList>
#include <QString>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"

class QThread;

class TaskManager;
class ApplicationImpl;
class TagReaderClient;
class Database;
class DeviceFinders;
class UrlHandlers;
class Player;
class NetworkAccessManager;
class CollectionLibrary;
class CollectionBackend;
class CollectionModel;
class PlaylistBackend;
class PlaylistManager;
class DeviceManager;
class CoverProviders;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class LyricsProviders;
class AudioScrobbler;
class LastFMImport;
class StreamingServices;
class RadioServices;
#ifdef HAVE_MOODBAR
class MoodbarController;
class MoodbarLoader;
#endif

class Application : public QObject {
  Q_OBJECT

 public:
  explicit Application(QObject *parent = nullptr);
  ~Application() override;

  SharedPtr<TagReaderClient> tagreader_client() const;
  SharedPtr<Database> database() const;
  SharedPtr<TaskManager> task_manager() const;
  SharedPtr<Player> player() const;
  SharedPtr<NetworkAccessManager> network() const;
  SharedPtr<DeviceFinders> device_finders() const;
  SharedPtr<UrlHandlers> url_handlers() const;
  SharedPtr<DeviceManager> device_manager() const;

  SharedPtr<CollectionLibrary> collection() const;
  SharedPtr<CollectionBackend> collection_backend() const;
  CollectionModel *collection_model() const;

  SharedPtr<PlaylistBackend> playlist_backend() const;
  SharedPtr<PlaylistManager> playlist_manager() const;

  SharedPtr<CoverProviders> cover_providers() const;
  SharedPtr<AlbumCoverLoader> albumcover_loader() const;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader() const;

  SharedPtr<LyricsProviders> lyrics_providers() const;

  SharedPtr<AudioScrobbler> scrobbler() const;

  SharedPtr<StreamingServices> streaming_services() const;
  SharedPtr<RadioServices> radio_services() const;

#ifdef HAVE_MOODBAR
  SharedPtr<MoodbarController> moodbar_controller() const;
  SharedPtr<MoodbarLoader> moodbar_loader() const;
#endif

  SharedPtr<LastFMImport> lastfm_import() const;

  void Exit();

  QThread *MoveToNewThread(QObject *object);
  static void MoveToThread(QObject *object, QThread *thread);

 private Q_SLOTS:
  void ExitReceived();

 Q_SIGNALS:
  void ExitFinished();

 private:
  static gpointer GLibMainLoopThreadFunc(gpointer data);

 private:
  ScopedPtr<ApplicationImpl> p_;
  GThread *g_thread_;
  QList<QThread*> threads_;
  QList<QObject*> wait_for_exit_;
};

#endif  // APPLICATION_H
