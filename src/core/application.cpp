/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include "application.h"

#include <functional>

#include <QObject>
#include <QThread>
#include <QVariant>
#include <QString>

#include "core/closure.h"
#include "core/lazy.h"
#include "core/tagreaderclient.h"
#include "core/song.h"

#include "database.h"
#include "taskmanager.h"
#include "player.h"
#include "appearance.h"

#include "engine/enginedevice.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#endif
#include "collection/collection.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/currentartloader.h"
#include "covermanager/lastfmcoverprovider.h"
#include "covermanager/discogscoverprovider.h"
#include "covermanager/musicbrainzcoverprovider.h"

#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/auddlyricsprovider.h"
#include "lyrics/apiseedslyricsprovider.h"

#include "internet/internetservices.h"
#include "internet/internetsearch.h"

#ifdef HAVE_STREAM_TIDAL
#  include "tidal/tidalservice.h"
#endif
#ifdef HAVE_STREAM_DEEZER
#  include "deezer/deezerservice.h"
#endif

bool Application::kIsPortable = false;

class ApplicationImpl {
 public:
  ApplicationImpl(Application *app) :
       tag_reader_client_([=]() {
          TagReaderClient *client = new TagReaderClient(app);
          app->MoveToNewThread(client);
          client->Start();
          return client;
        }),
        database_([=]() {
          Database *db = new Database(app, app);
          app->MoveToNewThread(db);
          DoInAMinuteOrSo(db, SLOT(DoBackup()));
          return db;
        }),
        appearance_([=]() { return new Appearance(app); }),
        task_manager_([=]() { return new TaskManager(app); }),
        player_([=]() { return new Player(app, app); }),
        enginedevice_([=]() { return new EngineDevice(app); }),
#ifndef Q_OS_WIN
        device_manager_([=]() { return new DeviceManager(app, app); }),
#endif
        collection_([=]() { return new SCollection(app, app); }),
        playlist_backend_([=]() {
          PlaylistBackend *backend = new PlaylistBackend(app, app);
          app->MoveToThread(backend, database_->thread());
          return backend;
        }),
        playlist_manager_([=]() { return new PlaylistManager(app); }),
        cover_providers_([=]() {
          CoverProviders *cover_providers = new CoverProviders(app);
          // Initialize the repository of cover providers.
          cover_providers->AddProvider(new LastFmCoverProvider(app));
          cover_providers->AddProvider(new DiscogsCoverProvider(app));
          cover_providers->AddProvider(new MusicbrainzCoverProvider(app));
          return cover_providers;
        }),
        album_cover_loader_([=]() {
          AlbumCoverLoader *loader = new AlbumCoverLoader(app);
          app->MoveToNewThread(loader);
          return loader;
        }),
        current_art_loader_([=]() { return new CurrentArtLoader(app, app); }),
        lyrics_providers_([=]() {
          LyricsProviders *lyrics_providers = new LyricsProviders(app);
          lyrics_providers->AddProvider(new AuddLyricsProvider(app));
          lyrics_providers->AddProvider(new APISeedsLyricsProvider(app));
          return lyrics_providers;
        }),
        internet_services_([=]() {
          InternetServices *internet_services = new InternetServices(app);
#ifdef HAVE_STREAM_TIDAL
          internet_services->AddService(new TidalService(app, internet_services));
#endif
#ifdef HAVE_STREAM_DEEZER
          internet_services->AddService(new DeezerService(app, internet_services));
#endif
          return internet_services;
        }),
#ifdef HAVE_STREAM_TIDAL
        tidal_search_([=]() { return new InternetSearch(app, Song::Source_Tidal, app); }),
#endif
#ifdef HAVE_STREAM_DEEZER
        deezer_search_([=]() { return new InternetSearch(app, Song::Source_Deezer, app); }),
#endif
        dummy_([=]() { return new QVariant; })
  {}

  Lazy<TagReaderClient> tag_reader_client_;
  Lazy<Database> database_;
  Lazy<Appearance> appearance_;
  Lazy<TaskManager> task_manager_;
  Lazy<Player> player_;
  Lazy<EngineDevice> enginedevice_;
#ifndef Q_OS_WIN
  Lazy<DeviceManager> device_manager_;
#endif
  Lazy<SCollection> collection_;
  Lazy<PlaylistBackend> playlist_backend_;
  Lazy<PlaylistManager> playlist_manager_;
  Lazy<CoverProviders> cover_providers_;
  Lazy<AlbumCoverLoader> album_cover_loader_;
  Lazy<CurrentArtLoader> current_art_loader_;
  Lazy<LyricsProviders> lyrics_providers_;
  Lazy<InternetServices> internet_services_;
#ifdef HAVE_STREAM_TIDAL
  Lazy<InternetSearch> tidal_search_;
#endif
#ifdef HAVE_STREAM_DEEZER
  Lazy<InternetSearch> deezer_search_;
#endif
  Lazy<QVariant> dummy_;

};

Application::Application(QObject *parent)
    : QObject(parent), p_(new ApplicationImpl(this)) {

  enginedevice()->Init();
  collection()->Init();
  tag_reader_client();

}

Application::~Application() {

  // It's important that the device manager is deleted before the database.
  // Deleting the database deletes all objects that have been created in its thread, including some device collection backends.
#ifndef Q_OS_WIN
  p_->device_manager_.reset();
#endif

  for (QThread *thread : threads_) {
    thread->quit();
  }

  for (QThread *thread : threads_) {
    thread->wait();
  }
}

void Application::MoveToNewThread(QObject *object) {

  QThread *thread = new QThread(this);

  MoveToThread(object, thread);

  thread->start();
  threads_ << thread;
}

void Application::MoveToThread(QObject *object, QThread *thread) {
  object->setParent(nullptr);
  object->moveToThread(thread);
}

void Application::AddError(const QString& message) { emit ErrorAdded(message); }
void Application::ReloadSettings() { emit SettingsChanged(); }
void Application::OpenSettingsDialogAtPage(SettingsDialog::Page page) { emit SettingsDialogRequested(page); }

TagReaderClient *Application::tag_reader_client() const { return p_->tag_reader_client_.get(); }
Appearance *Application::appearance() const { return p_->appearance_.get(); }
Database *Application::database() const { return p_->database_.get(); }
TaskManager *Application::task_manager() const { return p_->task_manager_.get(); }
Player *Application::player() const { return p_->player_.get(); }
EngineDevice *Application::enginedevice() const { return p_->enginedevice_.get(); }
#ifndef Q_OS_WIN
DeviceManager *Application::device_manager() const { return p_->device_manager_.get(); }
#endif
SCollection *Application::collection() const { return p_->collection_.get(); }
CollectionBackend *Application::collection_backend() const { return collection()->backend(); }
CollectionModel *Application::collection_model() const { return collection()->model(); }
AlbumCoverLoader *Application::album_cover_loader() const { return p_->album_cover_loader_.get(); }
CoverProviders *Application::cover_providers() const { return p_->cover_providers_.get(); }
CurrentArtLoader *Application::current_art_loader() const { return p_->current_art_loader_.get(); }
LyricsProviders *Application::lyrics_providers() const { return p_->lyrics_providers_.get(); }
PlaylistBackend *Application::playlist_backend() const { return p_->playlist_backend_.get(); }
PlaylistManager *Application::playlist_manager() const { return p_->playlist_manager_.get(); }
InternetServices *Application::internet_services() const { return p_->internet_services_.get(); }
#ifdef HAVE_STREAM_TIDAL
InternetSearch *Application::tidal_search() const { return p_->tidal_search_.get(); }
#endif
#ifdef HAVE_STREAM_DEEZER
InternetSearch *Application::deezer_search() const { return p_->deezer_search_.get(); }
#endif
