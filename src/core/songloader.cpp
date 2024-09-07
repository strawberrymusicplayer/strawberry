/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <algorithm>

#ifdef HAVE_GSTREAMER
#  include <gst/gst.h>
#endif

#include <QObject>
#include <QIODevice>
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QEventLoop>

#include "core/logging.h"

#include "shared_ptr.h"
#include "signalchecker.h"
#include "player.h"
#include "song.h"
#include "songloader.h"
#include "tagreaderclient.h"
#include "database.h"
#include "sqlrow.h"
#include "engine/enginebase.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "playlistparsers/cueparser.h"
#include "playlistparsers/parserbase.h"
#include "playlistparsers/playlistparser.h"

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
#  include "device/cddasongloader.h"
#endif

namespace {
constexpr int kDefaultTimeout = 5000;
}

QSet<QString> SongLoader::sRawUriSchemes;

SongLoader::SongLoader(SharedPtr<CollectionBackendInterface> collection_backend, const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      collection_backend_(collection_backend),
      timeout_timer_(new QTimer(this)),
      playlist_parser_(new PlaylistParser(collection_backend, this)),
      cue_parser_(new CueParser(collection_backend, this)),
      parser_(nullptr),
      state_(State::WaitingForType),
      timeout_(kDefaultTimeout),
#ifdef HAVE_GSTREAMER
      fakesink_(nullptr),
      buffer_probe_cb_id_(0),
#endif
      success_(false) {

  if (sRawUriSchemes.isEmpty()) {
    sRawUriSchemes << QStringLiteral("udp")
                   << QStringLiteral("mms")
                   << QStringLiteral("mmsh")
                   << QStringLiteral("mmst")
                   << QStringLiteral("mmsu")
                   << QStringLiteral("rtsp")
                   << QStringLiteral("rtspu")
                   << QStringLiteral("rtspt")
                   << QStringLiteral("rtsph");
  }

  timeout_timer_->setSingleShot(true);

  QObject::connect(timeout_timer_, &QTimer::timeout, this, &SongLoader::Timeout);

}

SongLoader::~SongLoader() {

#ifdef HAVE_GSTREAMER
  CleanupPipeline();
#endif

}

SongLoader::Result SongLoader::Load(const QUrl &url) {

  if (url.isEmpty()) return Result::Error;

  url_ = url;

  if (url_.isLocalFile()) {
    return LoadLocal(url_.toLocalFile());
  }

  if (sRawUriSchemes.contains(url_.scheme()) || player_->HandlerForUrl(url)) {
    // The URI scheme indicates that it can't possibly be a playlist,
    // or we have a custom handler for the URL, so add it as a raw stream.
    AddAsRawStream();
    return Result::Success;
  }

  if (player_->engine()->type() == EngineBase::Type::GStreamer) {
#ifdef HAVE_GSTREAMER
    preload_func_ = std::bind(&SongLoader::LoadRemote, this);
    return Result::BlockingLoadRequired;
#else
    errors_ << tr("You need GStreamer for this URL.");
    return Result::Error;
#endif
  }
  else {
    errors_ << tr("You need GStreamer for this URL.");
    return Result::Error;
  }

  return Result::Success;

}

SongLoader::Result SongLoader::LoadFilenamesBlocking() {

  if (preload_func_) {
    return preload_func_();
  }
  else {
    errors_ << tr("Preload function was not set for blocking operation.");
    return Result::Error;
  }

}

SongLoader::Result SongLoader::LoadLocalPartial(const QString &filename) {

  qLog(Debug) << "Fast Loading local file" << filename;

  QFileInfo fileinfo(filename);

  if (!fileinfo.exists()) {
    errors_ << tr("File %1 does not exist.").arg(filename);
    return Result::Error;
  }

  // First check to see if it's a directory - if so we can load all the songs inside right away.
  if (fileinfo.isDir()) {
    LoadLocalDirectory(filename);
    return Result::Success;
  }

  // Assume it's just a normal file
  if (!Song::kRejectedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive) &&
      (TagReaderClient::Instance()->IsMediaFileBlocking(filename) ||
       Song::kAcceptedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive))) {
    Song song(Song::Source::LocalFile);
    song.InitFromFilePartial(filename, fileinfo);
    if (song.is_valid()) {
      songs_ << song;
      return Result::Success;
    }
  }

  errors_ << QObject::tr("File %1 is not recognized as a valid audio file.").arg(filename);
  return Result::Error;

}

SongLoader::Result SongLoader::LoadAudioCD() {

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  if (player_->engine()->type() == EngineBase::Type::GStreamer) {
    CddaSongLoader *cdda_song_loader = new CddaSongLoader(QUrl(), this);
    QObject::connect(cdda_song_loader, &CddaSongLoader::SongsDurationLoaded, this, &SongLoader::AudioCDTracksLoadFinishedSlot);
    QObject::connect(cdda_song_loader, &CddaSongLoader::SongsMetadataLoaded, this, &SongLoader::AudioCDTracksTagsLoaded);
    cdda_song_loader->LoadSongs();
    return Result::Success;
  }
  else {
#endif
    errors_ << tr("CD playback is only available with the GStreamer engine.");
    return Result::Error;
#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  }
#endif

}

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)

void SongLoader::AudioCDTracksLoadFinishedSlot(const SongList &songs, const QString &error) {

  songs_ = songs;
  errors_ << error;
  Q_EMIT AudioCDTracksLoadFinished();

}

void SongLoader::AudioCDTracksTagsLoaded(const SongList &songs) {

  CddaSongLoader *cdda_song_loader = qobject_cast<CddaSongLoader*>(sender());
  cdda_song_loader->deleteLater();
  songs_ = songs;
  Q_EMIT LoadAudioCDFinished(true);

}
#endif

SongLoader::Result SongLoader::LoadLocal(const QString &filename) {

  qLog(Debug) << "Loading local file" << filename;

  // Search in the database.
  QUrl url = QUrl::fromLocalFile(filename);

  QMutexLocker l(collection_backend_->db()->Mutex());
  QSqlDatabase db(collection_backend_->db()->Connect());

  CollectionQuery query(db, collection_backend_->songs_table());
  query.SetColumnSpec(QStringLiteral("%songs_table.ROWID, ") + Song::kColumnSpec);
  query.AddWhere(QStringLiteral("url"), url.toEncoded());

  if (query.Exec() && query.Next()) {
    // We may have many results when the file has many sections
    do {
      Song song(Song::Source::Collection);
      song.InitFromQuery(query, true);

      if (song.is_valid()) {
        songs_ << song;
      }
    } while (query.Next());

    return Result::Success;
  }

  // It's not in the database, load it asynchronously.
  preload_func_ = std::bind(&SongLoader::LoadLocalAsync, this, filename);
  return Result::BlockingLoadRequired;

}

SongLoader::Result SongLoader::LoadLocalAsync(const QString &filename) {

  QFileInfo fileinfo(filename);

  if (!fileinfo.exists()) {
    errors_ << tr("File %1 does not exist.").arg(filename);
    return Result::Error;
  }

  // First check to see if it's a directory - if so we will load all the songs inside right away.
  if (fileinfo.isDir()) {
    LoadLocalDirectory(filename);
    return Result::Success;
  }

  // It's a local file, so check if it looks like a playlist. Read the first few bytes.
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    errors_ << tr("Could not open file %1 for reading: %2").arg(filename, file.errorString());
    return Result::Error;
  }
  QByteArray data(file.read(PlaylistParser::kMagicSize));
  file.close();

  ParserBase *parser = playlist_parser_->ParserForMagic(data);
  if (!parser) {
    // Check the file extension as well, maybe the magic failed, or it was a basic M3U file which is just a plain list of filenames.
    parser = playlist_parser_->ParserForExtension(PlaylistParser::Type::Load, fileinfo.suffix().toLower());
  }

  if (parser) {  // It's a playlist!
    qLog(Debug) << "Parsing using" << parser->name();
    LoadPlaylist(parser, filename);
    return Result::Success;
  }

  // Check if it's a CUE file
  QString matching_cue = CueParser::FindCueFilename(filename);
  if (QFile::exists(matching_cue)) {
    // It's a CUE - create virtual tracks
    QFile cue(matching_cue);
    if (cue.open(QIODevice::ReadOnly)) {
      const SongList songs = cue_parser_->Load(&cue, matching_cue, QDir(filename.section(u'/', 0, -2)));
      cue.close();
      for (const Song &song : songs) {
        if (song.is_valid()) songs_ << song;
      }
      return Result::Success;
    }
    else {
      errors_ << tr("Could not open CUE file %1 for reading: %2").arg(matching_cue, cue.errorString());
      return Result::Error;
    }
  }

  // Assume it's just a normal file
  if (!Song::kRejectedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive) &&
      (TagReaderClient::Instance()->IsMediaFileBlocking(filename) ||
       Song::kAcceptedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive))) {
    Song song(Song::Source::LocalFile);
    song.InitFromFilePartial(filename, fileinfo);
    if (song.is_valid()) {
      songs_ << song;
      return Result::Success;
    }
  }

  errors_ << QObject::tr("File %1 is not recognized as a valid audio file.").arg(filename);
  return Result::Error;

}

void SongLoader::LoadMetadataBlocking() {

  for (int i = 0; i < songs_.size(); i++) {
    EffectiveSongLoad(&songs_[i]);
  }

}

void SongLoader::EffectiveSongLoad(Song *song) {

  if (!song || !song->url().isLocalFile()) return;

  if (song->init_from_file() && song->filetype() != Song::FileType::Unknown) {
    // Maybe we loaded the metadata already, for example from a cuesheet.
    return;
  }

  // First, try to get the song from the collection
  Song collection_song = collection_backend_->GetSongByUrl(song->url());
  if (collection_song.is_valid()) {
    *song = collection_song;
  }
  else {
    // It's a normal media file
    const QString filename = song->url().toLocalFile();
    const TagReaderClient::Result result = TagReaderClient::Instance()->ReadFileBlocking(filename, song);
    if (!result.success()) {
      qLog(Error) << "Could not read file" << song->url() << result.error;
    }
  }

}

void SongLoader::LoadPlaylist(ParserBase *parser, const QString &filename) {

  QFile file(filename);
  if (file.open(QIODevice::ReadOnly)) {
    songs_ = parser->Load(&file, filename, QFileInfo(filename).path());
    file.close();
  }
  else {
    errors_ << tr("Could not open playlist file %1 for reading: %2").arg(filename, file.errorString());
  }

}

static bool CompareSongs(const Song &left, const Song &right) {

  // Order by artist, album, disc, track
  if (left.artist() < right.artist()) return true;
  if (left.artist() > right.artist()) return false;
  if (left.album() < right.album()) return true;
  if (left.album() > right.album()) return false;
  if (left.disc() < right.disc()) return true;
  if (left.disc() > right.disc()) return false;
  if (left.track() < right.track()) return true;
  if (left.track() > right.track()) return false;
  return left.url() < right.url();

}

void SongLoader::LoadLocalDirectory(const QString &filename) {

  QDirIterator it(filename, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDirIterator::Subdirectories);

  while (it.hasNext()) {
    LoadLocalPartial(it.next());
  }

  std::stable_sort(songs_.begin(), songs_.end(), CompareSongs);

  // Load the first song:
  // all songs will be loaded async, but we want the first one in our list to be fully loaded,
  // so if the user has the "Start playing when adding to playlist" preference behaviour set,
  // it can enjoy the first song being played (seek it, have moodbar, etc.)
  if (!songs_.isEmpty()) EffectiveSongLoad(&(*songs_.begin()));
}

void SongLoader::AddAsRawStream() {

  Song song(Song::SourceFromURL(url_));
  song.set_valid(true);
  song.set_filetype(Song::FileType::Stream);
  song.set_url(url_);
  song.set_title(url_.toString());
  songs_ << song;

}

void SongLoader::Timeout() {

  state_ = State::Finished;
  success_ = false;
  StopTypefind();

}

void SongLoader::StopTypefind() {

#ifdef HAVE_GSTREAMER
  // Destroy the pipeline
  if (pipeline_) {
    gst_element_set_state(&*pipeline_, GST_STATE_NULL);
    CleanupPipeline();
  }
#endif
  timeout_timer_->stop();

  if (success_ && parser_) {
    qLog(Debug) << "Parsing" << url_ << "with" << parser_->name();

    // Parse the playlist
    QBuffer buf(&buffer_);
    if (buf.open(QIODevice::ReadOnly)) {
      songs_ = parser_->Load(&buf);
      buf.close();
    }

  }
  else if (success_) {
    qLog(Debug) << "Loading" << url_ << "as raw stream";

    // It wasn't a playlist - just put the URL in as a stream
    AddAsRawStream();
  }

  Q_EMIT LoadRemoteFinished();

}

#ifdef HAVE_GSTREAMER
SongLoader::Result SongLoader::LoadRemote() {

  qLog(Debug) << "Loading remote file" << url_;

  // It's not a local file so we have to fetch it to see what it is.
  // We use gstreamer to do this since it handles funky URLs for us (http://, ssh://, etc) and also has typefinder plugins.
  // First we wait for typefinder to tell us what it is.  If it's not text/plain or text/uri-list assume it's a song and return success.
  // Otherwise wait to get 512 bytes of data and do magic on it - if the magic fails then we don't know what it is so return failure.
  // If the magic succeeds then we know for sure it's a playlist - so read the rest of the file, parse the playlist and return success.

  ScheduleTimeoutAsync();

  // Create the pipeline - it gets unreffed if it goes out of scope
  SharedPtr<GstElement> pipeline(gst_pipeline_new(nullptr), std::bind(&gst_object_unref, std::placeholders::_1));

  // Create the source element automatically based on the URL
  GstElement *source = gst_element_make_from_uri(GST_URI_SRC, url_.toEncoded().constData(), nullptr, nullptr);
  if (!source) {
    errors_ << tr("Couldn't create GStreamer source element for %1").arg(url_.toString());
    return Result::Error;
  }
  gst_bin_add(GST_BIN(&*pipeline), source);

  g_object_set(source, "ssl-strict", FALSE, nullptr);

  // Create the other elements and link them up
  GstElement *typefind = gst_element_factory_make("typefind", nullptr);
  if (!typefind) {
    errors_ << tr("Couldn't create GStreamer typefind element for %1").arg(url_.toString());
    return Result::Error;
  }
  gst_bin_add(GST_BIN(&*pipeline), typefind);

  fakesink_ = gst_element_factory_make("fakesink", nullptr);
  if (!fakesink_) {
    errors_ << tr("Couldn't create GStreamer fakesink element for %1").arg(url_.toString());
    return Result::Error;
  }
  gst_bin_add(GST_BIN(&*pipeline), fakesink_);

  if (!gst_element_link_many(source, typefind, fakesink_, nullptr)) {
    errors_ << tr("Couldn't link GStreamer source, typefind and fakesink elements for %1").arg(url_.toString());
    return Result::Error;
  }

  // Connect callbacks
  CHECKED_GCONNECT(typefind, "have-type", &TypeFound, this);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(&*pipeline));
  if (bus) {
    gst_bus_set_sync_handler(bus, BusCallbackSync, this, nullptr);
    gst_bus_add_watch(bus, BusWatchCallback, this);
    gst_object_unref(bus);
  }

  // Add a probe to the sink so we can capture the data if it's a playlist
  GstPad *pad = gst_element_get_static_pad(fakesink_, "sink");
  if (pad) {
    buffer_probe_cb_id_ = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, &DataReady, this, nullptr);
    gst_object_unref(pad);
  }

  QEventLoop loop;
  QObject::connect(this, &SongLoader::LoadRemoteFinished, &loop, &QEventLoop::quit);

  // Start "playing"
  pipeline_ = pipeline;
  gst_element_set_state(&*pipeline, GST_STATE_PLAYING);

  // Wait until loading is finished
  loop.exec();

  return Result::Success;

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::TypeFound(GstElement*, uint, GstCaps *caps, void *self) {

  SongLoader *instance = static_cast<SongLoader*>(self);

  if (instance->state_ != State::WaitingForType) return;

  // Check the mimetype
  instance->mime_type_ = QString::fromUtf8(gst_structure_get_name(gst_caps_get_structure(caps, 0)));
  qLog(Debug) << "Mime type is" << instance->mime_type_;
  if (instance->mime_type_ == QStringLiteral("text/plain") || instance->mime_type_ == QStringLiteral("text/uri-list")) {
    // Yeah it might be a playlist, let's get some data and have a better look
    instance->state_ = State::WaitingForMagic;
    return;
  }

  // Nope, not a playlist - we're done
  instance->StopTypefindAsync(true);

}
#endif

#ifdef HAVE_GSTREAMER
GstPadProbeReturn SongLoader::DataReady(GstPad*, GstPadProbeInfo *info, gpointer self) {

  SongLoader *instance = reinterpret_cast<SongLoader*>(self);

  if (instance->state_ == State::Finished) {
    return GST_PAD_PROBE_OK;
  }

  GstBuffer *buffer = gst_pad_probe_info_get_buffer(info);
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);

  // Append the data to the buffer
  instance->buffer_.append(reinterpret_cast<const char*>(map.data), static_cast<qint64>(map.size));
  qLog(Debug) << "Received total" << instance->buffer_.size() << "bytes";
  gst_buffer_unmap(buffer, &map);

  if (instance->state_ == State::WaitingForMagic && (instance->buffer_.size() >= PlaylistParser::kMagicSize || !instance->IsPipelinePlaying())) {
    // Got enough that we can test the magic
    instance->MagicReady();
  }

  return GST_PAD_PROBE_OK;
}
#endif

#ifdef HAVE_GSTREAMER
gboolean SongLoader::BusWatchCallback(GstBus*, GstMessage *msg, gpointer self) {

  SongLoader *instance = reinterpret_cast<SongLoader*>(self);

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      instance->ErrorMessageReceived(msg);
      break;

    default:
      break;
  }

  return TRUE;
}
#endif

#ifdef HAVE_GSTREAMER
GstBusSyncReply SongLoader::BusCallbackSync(GstBus*, GstMessage *msg, gpointer self) {

  SongLoader *instance = reinterpret_cast<SongLoader*>(self);

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
      instance->EndOfStreamReached();
      break;

    case GST_MESSAGE_ERROR:
      instance->ErrorMessageReceived(msg);
      break;

    default:
      break;
  }
  return GST_BUS_PASS;
}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::ErrorMessageReceived(GstMessage *msg) {

  if (state_ == State::Finished) return;

  GError *error = nullptr;
  gchar *debugs = nullptr;

  gst_message_parse_error(msg, &error, &debugs);
  qLog(Error) << error->message;
  qLog(Error) << debugs;

  QString message_str = QString::fromUtf8(error->message);

  g_error_free(error);
  g_free(debugs);

  if (state_ == State::WaitingForType && message_str == QString::fromUtf8(gst_error_get_message(GST_STREAM_ERROR, GST_STREAM_ERROR_TYPE_NOT_FOUND))) {
    // Don't give up - assume it's a playlist and see if one of our parsers can read it.
    state_ = State::WaitingForMagic;
    return;
  }

  StopTypefindAsync(false);

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::EndOfStreamReached() {

  qLog(Debug) << Q_FUNC_INFO << static_cast<int>(state_);

  switch (state_) {
    case State::Finished:
      break;

    case State::WaitingForMagic:
      // Do the magic on the data we have already
      MagicReady();
      if (state_ == State::Finished) break;
    // It looks like a playlist, so parse it

    [[fallthrough]];
    case State::WaitingForData:
      // It's a playlist and we've got all the data - finish and parse it
      StopTypefindAsync(true);
      break;

    case State::WaitingForType:
      StopTypefindAsync(false);
      break;
  }

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::MagicReady() {

  qLog(Debug) << Q_FUNC_INFO;

  parser_ = playlist_parser_->ParserForMagic(buffer_, mime_type_);

  if (!parser_) {
    qLog(Warning) << url_.toString() << "is text, but not a recognised playlist";
    // It doesn't look like a playlist, so just finish
    StopTypefindAsync(false);
    return;
  }

  // We'll get more data and parse the whole thing in EndOfStreamReached

  qLog(Debug) << "Magic says" << parser_->name();

  if (parser_->name() == QStringLiteral("ASX/INI") && url_.scheme() == QStringLiteral("http")) {
    // This is actually a weird MS-WMSP stream. Changing the protocol to MMS from HTTP makes it playable.
    parser_ = nullptr;
    url_.setScheme(QStringLiteral("mms"));
    StopTypefindAsync(true);
  }

  state_ = State::WaitingForData;

  if (!IsPipelinePlaying()) {
    EndOfStreamReached();
  }

}
#endif

#ifdef HAVE_GSTREAMER
bool SongLoader::IsPipelinePlaying() {

  GstState state = GST_STATE_NULL;
  GstState pending_state = GST_STATE_NULL;
  GstStateChangeReturn ret = gst_element_get_state(&*pipeline_, &state, &pending_state, GST_SECOND);

  if (ret == GST_STATE_CHANGE_ASYNC && pending_state == GST_STATE_PLAYING) {
    // We're still on the way to playing
    return true;
  }
  return state == GST_STATE_PLAYING;

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::StopTypefindAsync(const bool success) {

  state_ = State::Finished;
  success_ = success;

  QMetaObject::invokeMethod(this, &SongLoader::StopTypefind, Qt::QueuedConnection);

}
#endif

void SongLoader::ScheduleTimeoutAsync() {

  if (QThread::currentThread() == thread()) {
    ScheduleTimeout();
  }
  else {
    QMetaObject::invokeMethod(this, &SongLoader::ScheduleTimeout, Qt::QueuedConnection);
  }

}

void SongLoader::ScheduleTimeout() {

  timeout_timer_->start(timeout_);

}

#ifdef HAVE_GSTREAMER

void SongLoader::CleanupPipeline() {

  if (pipeline_) {

    gst_element_set_state(&*pipeline_, GST_STATE_NULL);

    if (fakesink_ && buffer_probe_cb_id_ != 0) {
      GstPad *pad = gst_element_get_static_pad(fakesink_, "src");
      if (pad) {
        gst_pad_remove_probe(pad, buffer_probe_cb_id_);
        gst_object_unref(pad);
      }
    }

    {
      GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(&*pipeline_));
      if (bus) {
        gst_bus_remove_watch(bus);
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
      }
    }

    pipeline_.reset();

  }

  state_ = State::Finished;

}

#endif
