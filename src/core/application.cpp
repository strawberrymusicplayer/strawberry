/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <glib.h>

#include <QObject>
#include <QThread>
#include <QString>
#include <QMetaObject>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QTimer>

#include "core/logging.h"

#include "includes/shared_ptr.h"
#include "includes/lazy.h"
#include "core/database.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/player.h"
#include "tagreader/tagreaderclient.h"
#include "engine/devicefinders.h"
#include "core/urlhandlers.h"
#include "device/devicemanager.h"
#include "collection/collectionlibrary.h"
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
       tagreader_client_([app](){
          TagReaderClient *client = new TagReaderClient();
          app->MoveToNewThread(client);
          return client;
        }),
        database_([app]() {
          Database *database = new Database(app->task_manager());
          app->MoveToNewThread(database);
          QTimer::singleShot(30s, database, &Database::DoBackup);
          return database;
        }),
        task_manager_([]() { return new TaskManager(); }),
        player_([app]() { return new Player(app->task_manager(), app->url_handlers(), app->playlist_manager()); }),
        network_([]() { return new NetworkAccessManager(); }),
        device_finders_([]() { return new DeviceFinders(); }),
        url_handlers_([]() { return new UrlHandlers(); }),
        device_manager_([app]() { return new DeviceManager(app->task_manager(), app->database(), app->tagreader_client(), app->albumcover_loader()); }),
        collection_([app]() { return new CollectionLibrary(app->database(), app->task_manager(), app->tagreader_client(), app->albumcover_loader()); }),
        playlist_backend_([this, app]() {
          PlaylistBackend *playlist_backend = new PlaylistBackend(app->database(), app->tagreader_client(), app->collection_backend());
          app->MoveToThread(playlist_backend, database_->thread());
          return playlist_backend;
        }),
        playlist_manager_([app]() { return new PlaylistManager(app->task_manager(), app->tagreader_client(), app->url_handlers(), app->playlist_backend(), app->collection_backend(), app->current_albumcover_loader()); }),
        cover_providers_([app]() {
          CoverProviders *cover_providers = new CoverProviders();
          // Initialize the repository of cover providers.
          cover_providers->AddProvider(new LastFmCoverProvider(app->network()));
          cover_providers->AddProvider(new MusicbrainzCoverProvider(app->network()));
          cover_providers->AddProvider(new DiscogsCoverProvider(app->network()));
          cover_providers->AddProvider(new DeezerCoverProvider(app->network()));
          cover_providers->AddProvider(new MusixmatchCoverProvider(app->network()));
          cover_providers->AddProvider(new OpenTidalCoverProvider(app->network()));
#ifdef HAVE_TIDAL
          cover_providers->AddProvider(new TidalCoverProvider(app->streaming_services()->Service<TidalService>(), app->network()));
#endif
#ifdef HAVE_SPOTIFY
          cover_providers->AddProvider(new SpotifyCoverProvider(app->streaming_services()->Service<SpotifyService>(), app->network()));
#endif
#ifdef HAVE_QOBUZ
          cover_providers->AddProvider(new QobuzCoverProvider(app->streaming_services()->Service<QobuzService>(), app->network()));
#endif
          cover_providers->ReloadSettings();
          return cover_providers;
        }),
        albumcover_loader_([app]() {
          AlbumCoverLoader *loader = new AlbumCoverLoader(app->tagreader_client());
          app->MoveToNewThread(loader);
          return loader;
        }),
        current_albumcover_loader_([app]() { return new CurrentAlbumCoverLoader(app->albumcover_loader()); }),
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
          streaming_services->AddService(make_shared<SubsonicService>(app->task_manager(), app->database(), app->url_handlers(), app->albumcover_loader()));
#endif
#ifdef HAVE_TIDAL
          streaming_services->AddService(make_shared<TidalService>(app->task_manager(), app->database(), app->network(), app->url_handlers(), app->albumcover_loader()));
#endif
#ifdef HAVE_SPOTIFY
          streaming_services->AddService(make_shared<SpotifyService>(app->task_manager(), app->database(), app->network(), app->albumcover_loader()));
#endif
#ifdef HAVE_QOBUZ
          streaming_services->AddService(make_shared<QobuzService>(app->task_manager(), app->database(), app->network(), app->url_handlers(), app->albumcover_loader()));
#endif
          return streaming_services;
        }),
        radio_services_([app]() { return new RadioServices(app->task_manager(), app->network(), app->database(), app->albumcover_loader()); }),
        scrobbler_([app]() {
          AudioScrobbler *scrobbler = new AudioScrobbler(app);
          scrobbler->AddService(make_shared<LastFMScrobbler>(scrobbler->settings(), app->network()));
          scrobbler->AddService(make_shared<LibreFMScrobbler>(scrobbler->settings(), app->network()));
          scrobbler->AddService(make_shared<ListenBrainzScrobbler>(scrobbler->settings(), app->network()));
#ifdef HAVE_SUBSONIC
          scrobbler->AddService(make_shared<SubsonicScrobbler>(scrobbler->settings(), app->network(), app->streaming_services()->Service<SubsonicService>(), app));
#endif
          return scrobbler;
        }),
#ifdef HAVE_MOODBAR
        moodbar_loader_([app]() { return new MoodbarLoader(app); }),
        moodbar_controller_([app]() { return new MoodbarController(app->player(), app->moodbar_loader()); }),
#endif
        lastfm_import_([app]() { return new LastFMImport(app->network()); })
  {}

  Lazy<TagReaderClient> tagreader_client_;
  Lazy<Database> database_;
  Lazy<TaskManager> task_manager_;
  Lazy<Player> player_;
  Lazy<NetworkAccessManager> network_;
  Lazy<DeviceFinders> device_finders_;
  Lazy<UrlHandlers> url_handlers_;
  Lazy<DeviceManager> device_manager_;
  Lazy<CollectionLibrary> collection_;
  Lazy<PlaylistBackend> playlist_backend_;
  Lazy<PlaylistManager> playlist_manager_;
  Lazy<CoverProviders> cover_providers_;
  Lazy<AlbumCoverLoader> albumcover_loader_;
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
    : QObject(parent),
      p_(new ApplicationImpl(this)),
      g_thread_(nullptr) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  const QMetaObject *mo = QAbstractEventDispatcher::instance(QCoreApplication::instance()->thread())->metaObject();
  if (mo && strcmp(mo->className(), "QEventDispatcherGlib") != 0 && strcmp(mo->superClass()->className(), "QEventDispatcherGlib") != 0) {
    g_thread_ = g_thread_new(nullptr, Application::GLibMainLoopThreadFunc, nullptr);
  }

  device_finders()->Init();
  collection()->Init();
  tagreader_client();

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

  if (g_thread_) g_thread_unref(g_thread_);

}

gpointer Application::GLibMainLoopThreadFunc(gpointer data) {

  Q_UNUSED(data)

  qLog(Info) << "Creating GLib main event loop.";

  GMainLoop *gloop = g_main_loop_new(nullptr, false);
  g_main_loop_run(gloop);
  g_main_loop_unref(gloop);

  return nullptr;

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

  wait_for_exit_ << &*tagreader_client()
                 << &*collection()
                 << &*playlist_backend()
                 << &*albumcover_loader()
                 << &*device_manager()
                 << &*streaming_services()
                 << &*radio_services()->radio_backend();

  QObject::connect(&*tagreader_client(), &TagReaderClient::ExitFinished, this, &Application::ExitReceived);
  tagreader_client()->ExitAsync();

  QObject::connect(&*collection(), &CollectionLibrary::ExitFinished, this, &Application::ExitReceived);
  collection()->Exit();

  QObject::connect(&*playlist_backend(), &PlaylistBackend::ExitFinished, this, &Application::ExitReceived);
  playlist_backend()->ExitAsync();

  QObject::connect(&*albumcover_loader(), &AlbumCoverLoader::ExitFinished, this, &Application::ExitReceived);
  albumcover_loader()->ExitAsync();

  QObject::connect(&*device_manager(), &DeviceManager::ExitFinished, this, &Application::ExitReceived);
  device_manager()->Exit();

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

SharedPtr<TagReaderClient> Application::tagreader_client() const { return p_->tagreader_client_.ptr(); }
SharedPtr<Database> Application::database() const { return p_->database_.ptr(); }
SharedPtr<TaskManager> Application::task_manager() const { return p_->task_manager_.ptr(); }
SharedPtr<Player> Application::player() const { return p_->player_.ptr(); }
SharedPtr<NetworkAccessManager> Application::network() const { return p_->network_.ptr(); }
SharedPtr<DeviceFinders> Application::device_finders() const { return p_->device_finders_.ptr(); }
SharedPtr<UrlHandlers> Application::url_handlers() const { return p_->url_handlers_.ptr(); }
SharedPtr<DeviceManager> Application::device_manager() const { return p_->device_manager_.ptr(); }
SharedPtr<CollectionLibrary> Application::collection() const { return p_->collection_.ptr(); }
SharedPtr<CollectionBackend> Application::collection_backend() const { return collection()->backend(); }
CollectionModel *Application::collection_model() const { return collection()->model(); }
SharedPtr<AlbumCoverLoader> Application::albumcover_loader() const { return p_->albumcover_loader_.ptr(); }
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
