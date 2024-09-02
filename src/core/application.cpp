/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
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

#include "application.h"

#include <utility>
#include <functional>
#include <chrono>

#include <QObject>
#include <QThread>
#include <QString>

#include "core/logging.h"

#include "shared_ptr.h"
#include "lazy.h"
#include "tagreaderclient.h"
#include "database.h"
#include "taskmanager.h"
#include "player.h"
#include "networkaccessmanager.h"

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
#include "covermanager/musixmatchcoverprovider.h"
#include "covermanager/opentidalcoverprovider.h"

#include "lyrics/lyricsproviders.h"
#include "lyrics/geniuslyricsprovider.h"
#include "lyrics/ovhlyricsprovider.h"
#include "lyrics/lololyricsprovider.h"
#include "lyrics/musixmatchlyricsprovider.h"
#include "lyrics/chartlyricsprovider.h"
#include "lyrics/songlyricscomlyricsprovider.h"
#include "lyrics/azlyricscomlyricsprovider.h"
#include "lyrics/elyricsnetlyricsprovider.h"
#include "lyrics/letraslyricsprovider.h"
#include "lyrics/lyricfindlyricsprovider.h"

#include "scrobbler/audioscrobbler.h"
#include "scrobbler/lastfmscrobbler.h"
#include "scrobbler/librefmscrobbler.h"
#include "scrobbler/listenbrainzscrobbler.h"
#include "scrobbler/lastfmimport.h"
#ifdef HAVE_SUBSONIC
#  include "scrobbler/subsonicscrobbler.h"
#endif

#include "streaming/streamingservices.h"

#ifdef HAVE_SUBSONIC
#  include "subsonic/subsonicservice.h"
#endif

#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "covermanager/tidalcoverprovider.h"
#endif

#ifdef HAVE_SPOTIFY
#  include "spotify/spotifyservice.h"
#  include "covermanager/spotifycoverprovider.h"
#endif

#ifdef HAVE_QOBUZ
#  include "qobuz/qobuzservice.h"
#  include "covermanager/qobuzcoverprovider.h"
#endif

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarcontroller.h"
#  include "moodbar/moodbarloader.h"
#endif

#include "radios/radioservices.h"
#include "radios/radiobackend.h"

using std::make_shared;
using namespace std::chrono_literals;

class ApplicationImpl {
 public:
  explicit ApplicationImpl(Application *app) :
       tag_reader_client_([app](){
          TagReaderClient *client = new TagReaderClient();
          app->MoveToNewThread(client);
          client->Start();
          return client;
        }),
        database_([app]() {
          Database *db = new Database(app);
          app->MoveToNewThread(db);
          QTimer::singleShot(30s, db, &Database::DoBackup);
          return db;
        }),
        task_manager_([]() { return new TaskManager(); }),
        player_([app]() { return new Player(app); }),
        network_([]() { return new NetworkAccessManager(); }),
        device_finders_([]() { return new DeviceFinders(); }),
#ifndef Q_OS_WIN
        device_manager_([app]() { return new DeviceManager(app); }),
#endif
        collection_([app]() { return new SCollection(app); }),
        playlist_backend_([this, app]() {
          PlaylistBackend *backend = new PlaylistBackend(app);
          app->MoveToThread(backend, database_->thread());
          return backend;
        }),
        playlist_manager_([app]() { return new PlaylistManager(app); }),
        cover_providers_([app]() {
          CoverProviders *cover_providers = new CoverProviders();
          // Initialize the repository of cover providers.
          cover_providers->AddProvider(new LastFmCoverProvider(app, app->network()));
          cover_providers->AddProvider(new MusicbrainzCoverProvider(app, app->network()));
          cover_providers->AddProvider(new DiscogsCoverProvider(app, app->network()));
          cover_providers->AddProvider(new DeezerCoverProvider(app, app->network()));
          cover_providers->AddProvider(new MusixmatchCoverProvider(app, app->network()));
          cover_providers->AddProvider(new OpenTidalCoverProvider(app, app->network()));
#ifdef HAVE_TIDAL
          cover_providers->AddProvider(new TidalCoverProvider(app, app->network()));
#endif
#ifdef HAVE_SPOTIFY
          cover_providers->AddProvider(new SpotifyCoverProvider(app, app->network()));
#endif
#ifdef HAVE_QOBUZ
          cover_providers->AddProvider(new QobuzCoverProvider(app, app->network()));
#endif
          cover_providers->ReloadSettings();
          return cover_providers;
        }),
        album_cover_loader_([app]() {
          AlbumCoverLoader *loader = new AlbumCoverLoader();
          app->MoveToNewThread(loader);
          return loader;
        }),
        current_albumcover_loader_([app]() { return new CurrentAlbumCoverLoader(app); }),
        lyrics_providers_([app]() {
          LyricsProviders *lyrics_providers = new LyricsProviders(app);
          // Initialize the repository of lyrics providers.
          lyrics_providers->AddProvider(new GeniusLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new OVHLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new LoloLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new MusixmatchLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new ChartLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new SongLyricsComLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new AzLyricsComLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new ElyricsNetLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new LetrasLyricsProvider(lyrics_providers->network()));
          lyrics_providers->AddProvider(new LyricFindLyricsProvider(lyrics_providers->network()));
          lyrics_providers->ReloadSettings();
          return lyrics_providers;
        }),
        streaming_services_([app]() {
          StreamingServices *streaming_services = new StreamingServices();
#ifdef HAVE_SUBSONIC
          streaming_services->AddService(make_shared<SubsonicService>(app));
#endif
#ifdef HAVE_TIDAL
          streaming_services->AddService(make_shared<TidalService>(app));
#endif
#ifdef HAVE_SPOTIFY
          streaming_services->AddService(make_shared<SpotifyService>(app));
#endif
#ifdef HAVE_QOBUZ
          streaming_services->AddService(make_shared<QobuzService>(app));
#endif
          return streaming_services;
        }),
        radio_services_([app]() { return new RadioServices(app); }),
        scrobbler_([app]() {
          AudioScrobbler *scrobbler = new AudioScrobbler(app);
          scrobbler->AddService(make_shared<LastFMScrobbler>(scrobbler->settings(), app->network()));
          scrobbler->AddService(make_shared<LibreFMScrobbler>(scrobbler->settings(), app->network()));
          scrobbler->AddService(make_shared<ListenBrainzScrobbler>(scrobbler->settings(), app->network()));
#ifdef HAVE_SUBSONIC
          scrobbler->AddService(make_shared<SubsonicScrobbler>(scrobbler->settings(), app));
#endif
          return scrobbler;
        }),
#ifdef HAVE_MOODBAR
        moodbar_loader_([app]() { return new MoodbarLoader(app); }),
        moodbar_controller_([app]() { return new MoodbarController(app); }),
#endif
        lastfm_import_([app]() { return new LastFMImport(app->network()); })
  {}

  Lazy<TagReaderClient> tag_reader_client_;
  Lazy<Database> database_;
  Lazy<TaskManager> task_manager_;
  Lazy<Player> player_;
  Lazy<NetworkAccessManager> network_;
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
  Lazy<StreamingServices> streaming_services_;
  Lazy<RadioServices> radio_services_;
  Lazy<AudioScrobbler> scrobbler_;
#ifdef HAVE_MOODBAR
  Lazy<MoodbarLoader> moodbar_loader_;
  Lazy<MoodbarController> moodbar_controller_;
#endif
  Lazy<LastFMImport> lastfm_import_;

};

Application::Application(QObject *parent)
    : QObject(parent), p_(new ApplicationImpl(this)) {

  setObjectName(QLatin1String(metaObject()->className()));

  device_finders()->Init();
  collection()->Init();
  tag_reader_client();

  QObject::connect(&*database(), &Database::Error, this, &Application::ErrorAdded);

}

Application::~Application() {

   qLog(Debug) << "Terminating application";

  for (QThread *thread : std::as_const(threads_)) {
    thread->quit();
  }

  for (QThread *thread : std::as_const(threads_)) {
    thread->wait();
    thread->deleteLater();
  }

}

QThread *Application::MoveToNewThread(QObject *object) {

  QThread *thread = new QThread(this);

  thread->setObjectName(object->objectName());

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

  wait_for_exit_ << &*tag_reader_client()
                 << &*collection()
                 << &*playlist_backend()
                 << &*album_cover_loader()
#ifndef Q_OS_WIN
                 << &*device_manager()
#endif
                 << &*streaming_services()
                 << &*radio_services()->radio_backend();

  QObject::connect(&*tag_reader_client(), &TagReaderClient::ExitFinished, this, &Application::ExitReceived);
  tag_reader_client()->ExitAsync();

  QObject::connect(&*collection(), &SCollection::ExitFinished, this, &Application::ExitReceived);
  collection()->Exit();

  QObject::connect(&*playlist_backend(), &PlaylistBackend::ExitFinished, this, &Application::ExitReceived);
  playlist_backend()->ExitAsync();

  QObject::connect(&*album_cover_loader(), &AlbumCoverLoader::ExitFinished, this, &Application::ExitReceived);
  album_cover_loader()->ExitAsync();

#ifndef Q_OS_WIN
  QObject::connect(&*device_manager(), &DeviceManager::ExitFinished, this, &Application::ExitReceived);
  device_manager()->Exit();
#endif

  QObject::connect(&*streaming_services(), &StreamingServices::ExitFinished, this, &Application::ExitReceived);
  streaming_services()->Exit();

  QObject::connect(&*radio_services()->radio_backend(), &RadioBackend::ExitFinished, this, &Application::ExitReceived);
  radio_services()->radio_backend()->ExitAsync();

}

void Application::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);

  qLog(Debug) << obj << "successfully exited.";

  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) {
    database()->Close();
    QObject::connect(&*database(), &Database::ExitFinished, this, &Application::ExitFinished);
    database()->ExitAsync();
  }

}

void Application::AddError(const QString &message) { Q_EMIT ErrorAdded(message); }
void Application::ReloadSettings() { Q_EMIT SettingsChanged(); }
void Application::OpenSettingsDialogAtPage(SettingsDialog::Page page) { Q_EMIT SettingsDialogRequested(page); }

SharedPtr<TagReaderClient> Application::tag_reader_client() const { return p_->tag_reader_client_.ptr(); }
SharedPtr<Database> Application::database() const { return p_->database_.ptr(); }
SharedPtr<TaskManager> Application::task_manager() const { return p_->task_manager_.ptr(); }
SharedPtr<Player> Application::player() const { return p_->player_.ptr(); }
SharedPtr<NetworkAccessManager> Application::network() const { return p_->network_.ptr(); }
SharedPtr<DeviceFinders> Application::device_finders() const { return p_->device_finders_.ptr(); }
#ifndef Q_OS_WIN
SharedPtr<DeviceManager> Application::device_manager() const { return p_->device_manager_.ptr(); }
#endif
SharedPtr<SCollection> Application::collection() const { return p_->collection_.ptr(); }
SharedPtr<CollectionBackend> Application::collection_backend() const { return collection()->backend(); }
CollectionModel *Application::collection_model() const { return collection()->model(); }
SharedPtr<AlbumCoverLoader> Application::album_cover_loader() const { return p_->album_cover_loader_.ptr(); }
SharedPtr<CoverProviders> Application::cover_providers() const { return p_->cover_providers_.ptr(); }
SharedPtr<CurrentAlbumCoverLoader> Application::current_albumcover_loader() const { return p_->current_albumcover_loader_.ptr(); }
SharedPtr<LyricsProviders> Application::lyrics_providers() const { return p_->lyrics_providers_.ptr(); }
SharedPtr<PlaylistBackend> Application::playlist_backend() const { return p_->playlist_backend_.ptr(); }
SharedPtr<PlaylistManager> Application::playlist_manager() const { return p_->playlist_manager_.ptr(); }
SharedPtr<StreamingServices> Application::streaming_services() const { return p_->streaming_services_.ptr(); }
SharedPtr<RadioServices> Application::radio_services() const { return p_->radio_services_.ptr(); }
SharedPtr<AudioScrobbler> Application::scrobbler() const { return p_->scrobbler_.ptr(); }
SharedPtr<LastFMImport> Application::lastfm_import() const { return p_->lastfm_import_.ptr(); }
#ifdef HAVE_MOODBAR
SharedPtr<MoodbarController> Application::moodbar_controller() const { return p_->moodbar_controller_.ptr(); }
SharedPtr<MoodbarLoader> Application::moodbar_loader() const { return p_->moodbar_loader_.ptr(); }
#endif
