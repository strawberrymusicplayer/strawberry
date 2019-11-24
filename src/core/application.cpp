/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
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

#include "engine/devicefinders.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#endif
#include "collection/collection.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/lastfmcoverprovider.h"
#include "covermanager/discogscoverprovider.h"
#include "covermanager/musicbrainzcoverprovider.h"
#include "covermanager/deezercoverprovider.h"

#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/auddlyricsprovider.h"
#include "lyrics/ovhlyricsprovider.h"
#include "lyrics/lololyricsprovider.h"
#include "lyrics/chartlyricsprovider.h"

#include "scrobbler/audioscrobbler.h"

#include "internet/internetservices.h"
#include "internet/internetsearch.h"

#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "covermanager/tidalcoverprovider.h"
#endif

#ifdef HAVE_QOBUZ
#  include "qobuz/qobuzservice.h"
#endif

#ifdef HAVE_SUBSONIC
#  include "subsonic/subsonicservice.h"
#endif

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarcontroller.h"
#  include "moodbar/moodbarloader.h"
#endif

class ApplicationImpl {
 public:
  explicit ApplicationImpl(Application *app) :
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
        device_finders_([=]() { return new DeviceFinders(app); }),
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
          cover_providers->AddProvider(new LastFmCoverProvider(app, app));
          cover_providers->AddProvider(new DiscogsCoverProvider(app, app));
          cover_providers->AddProvider(new MusicbrainzCoverProvider(app, app));
          cover_providers->AddProvider(new DeezerCoverProvider(app, app));
#ifdef HAVE_TIDAL
          cover_providers->AddProvider(new TidalCoverProvider(app, app));
#endif
          return cover_providers;
        }),
        album_cover_loader_([=]() {
          AlbumCoverLoader *loader = new AlbumCoverLoader(app);
          app->MoveToNewThread(loader);
          return loader;
        }),
        current_albumcover_loader_([=]() { return new CurrentAlbumCoverLoader(app, app); }),
        lyrics_providers_([=]() {
          LyricsProviders *lyrics_providers = new LyricsProviders(app);
          lyrics_providers->AddProvider(new AuddLyricsProvider(app));
          lyrics_providers->AddProvider(new OVHLyricsProvider(app));
          lyrics_providers->AddProvider(new LoloLyricsProvider(app));
          lyrics_providers->AddProvider(new ChartLyricsProvider(app));
          return lyrics_providers;
        }),
        internet_services_([=]() {
          InternetServices *internet_services = new InternetServices(app);
#ifdef HAVE_TIDAL
          internet_services->AddService(new TidalService(app, internet_services));
#endif
#ifdef HAVE_QOBUZ
          internet_services->AddService(new QobuzService(app, internet_services));
#endif
#ifdef HAVE_SUBSONIC
          internet_services->AddService(new SubsonicService(app, internet_services));
#endif
          return internet_services;
        }),
#ifdef HAVE_TIDAL
        tidal_search_([=]() { return new InternetSearch(app, Song::Source_Tidal, app); }),
#endif
#ifdef HAVE_QOBUZ
        qobuz_search_([=]() { return new InternetSearch(app, Song::Source_Qobuz, app); }),
#endif
        scrobbler_([=]() { return new AudioScrobbler(app, app); }),

#ifdef HAVE_MOODBAR
        moodbar_loader_([=]() { return new MoodbarLoader(app, app); }),
        moodbar_controller_([=]() { return new MoodbarController(app, app); }),
#endif
       dummy_([=]() { return nullptr; })

  {}

  Lazy<TagReaderClient> tag_reader_client_;
  Lazy<Database> database_;
  Lazy<Appearance> appearance_;
  Lazy<TaskManager> task_manager_;
  Lazy<Player> player_;
  Lazy<DeviceFinders> device_finders_;
#ifndef Q_OS_WIN
  Lazy<DeviceManager> device_manager_;
#endif
  Lazy<SCollection> collection_;
  Lazy<PlaylistBackend> playlist_backend_;
  Lazy<PlaylistManager> playlist_manager_;
  Lazy<CoverProviders> cover_providers_;
  Lazy<AlbumCoverLoader> album_cover_loader_;
  Lazy<CurrentAlbumCoverLoader> current_albumcover_loader_;
  Lazy<LyricsProviders> lyrics_providers_;
  Lazy<InternetServices> internet_services_;
#ifdef HAVE_TIDAL
  Lazy<InternetSearch> tidal_search_;
#endif
#ifdef HAVE_QOBUZ
  Lazy<InternetSearch> qobuz_search_;
#endif
  Lazy<AudioScrobbler> scrobbler_;
#ifdef HAVE_MOODBAR
  Lazy<MoodbarLoader> moodbar_loader_;
  Lazy<MoodbarController> moodbar_controller_;
#endif
  Lazy<QVariant> dummy_;

};

Application::Application(QObject *parent)
    : QObject(parent), p_(new ApplicationImpl(this)) {

  device_finders()->Init();
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
    thread->deleteLater();
  }

}

QThread *Application::MoveToNewThread(QObject *object) {

  QThread *thread = new QThread(this);

  MoveToThread(object, thread);

  thread->start();
  threads_ << thread;

  return thread;

}

void Application::MoveToThread(QObject *object, QThread *thread) {
  object->setParent(nullptr);
  object->moveToThread(thread);
  qLog(Debug) << object << "moved to thread" << thread;
}

void Application::Exit() {

  wait_for_exit_ << tag_reader_client()
                 << collection()
                 << playlist_backend()
                 << album_cover_loader()
#ifndef Q_OS_WIN
                 << device_manager()
#endif
                 << internet_services();

  connect(tag_reader_client(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  tag_reader_client()->ExitAsync();

  connect(collection(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  collection()->Exit();

  connect(playlist_backend(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  playlist_backend()->ExitAsync();

  connect(album_cover_loader(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  album_cover_loader()->ExitAsync();

#ifndef Q_OS_WIN
  connect(device_manager(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  device_manager()->Exit();
#endif

  connect(internet_services(), SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  internet_services()->Exit();

}

void Application::ExitReceived() {

  QObject *obj = static_cast<QObject*>(sender());
  disconnect(obj, 0, this, 0);

  qLog(Debug) << obj << "successfully exited.";

  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) {
    database()->Close();
    connect(database(), SIGNAL(ExitFinished()), this, SIGNAL(ExitFinished()));
    database()->ExitAsync();
  }

}

void Application::AddError(const QString& message) { emit ErrorAdded(message); }
void Application::ReloadSettings() { emit SettingsChanged(); }
void Application::OpenSettingsDialogAtPage(SettingsDialog::Page page) { emit SettingsDialogRequested(page); }

TagReaderClient *Application::tag_reader_client() const { return p_->tag_reader_client_.get(); }
Appearance *Application::appearance() const { return p_->appearance_.get(); }
Database *Application::database() const { return p_->database_.get(); }
TaskManager *Application::task_manager() const { return p_->task_manager_.get(); }
Player *Application::player() const { return p_->player_.get(); }
DeviceFinders *Application::device_finders() const { return p_->device_finders_.get(); }
#ifndef Q_OS_WIN
DeviceManager *Application::device_manager() const { return p_->device_manager_.get(); }
#endif
SCollection *Application::collection() const { return p_->collection_.get(); }
CollectionBackend *Application::collection_backend() const { return collection()->backend(); }
CollectionModel *Application::collection_model() const { return collection()->model(); }
AlbumCoverLoader *Application::album_cover_loader() const { return p_->album_cover_loader_.get(); }
CoverProviders *Application::cover_providers() const { return p_->cover_providers_.get(); }
CurrentAlbumCoverLoader *Application::current_albumcover_loader() const { return p_->current_albumcover_loader_.get(); }
LyricsProviders *Application::lyrics_providers() const { return p_->lyrics_providers_.get(); }
PlaylistBackend *Application::playlist_backend() const { return p_->playlist_backend_.get(); }
PlaylistManager *Application::playlist_manager() const { return p_->playlist_manager_.get(); }
InternetServices *Application::internet_services() const { return p_->internet_services_.get(); }
#ifdef HAVE_TIDAL
InternetSearch *Application::tidal_search() const { return p_->tidal_search_.get(); }
#endif
#ifdef HAVE_QOBUZ
InternetSearch *Application::qobuz_search() const { return p_->qobuz_search_.get(); }
#endif
AudioScrobbler *Application::scrobbler() const { return p_->scrobbler_.get(); }
#ifdef HAVE_MOODBAR
MoodbarController *Application::moodbar_controller() const { return p_->moodbar_controller_.get(); }
MoodbarLoader *Application::moodbar_loader() const { return p_->moodbar_loader_.get(); }
#endif
