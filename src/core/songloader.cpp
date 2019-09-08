/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <stdlib.h>
#include <memory>
#include <algorithm>

#ifdef HAVE_GSTREAMER
#  include <gst/gst.h>
#endif

#include <QObject>
#include <QIODevice>
#include <QBuffer>
#include <QByteArray>
#include <QtAlgorithms>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QEventLoop>
#include <QtDebug>

#include "core/logging.h"

#include "signalchecker.h"
#include "player.h"
#include "song.h"
#include "songloader.h"
#include "tagreaderclient.h"
#include "engine/enginetype.h"
#include "engine/enginebase.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "collection/sqlrow.h"
#include "playlistparsers/cueparser.h"
#include "playlistparsers/parserbase.h"
#include "playlistparsers/playlistparser.h"

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
#  include "device/cddasongloader.h"
#endif

using std::placeholders::_1;
using std::bind;
using std::stable_sort;
using std::shared_ptr;

QSet<QString> SongLoader::sRawUriSchemes;
const int SongLoader::kDefaultTimeout = 5000;

SongLoader::SongLoader(CollectionBackendInterface *collection, const Player *player, QObject *parent) :
      QObject(parent),
      timeout_timer_(new QTimer(this)),
      playlist_parser_(new PlaylistParser(collection, this)),
      cue_parser_(new CueParser(collection, this)),
      timeout_(kDefaultTimeout),
      state_(WaitingForType),
      success_(false),
      parser_(nullptr),
      collection_(collection),
      player_(player) {

  if (sRawUriSchemes.isEmpty()) {
    sRawUriSchemes << "udp"
                   << "mms"
                   << "mmsh"
                   << "mmst"
                   << "mmsu"
                   << "rtsp"
                   << "rtspu"
                   << "rtspt"
                   << "rtsph";
  }

  timeout_timer_->setSingleShot(true);

  connect(timeout_timer_, SIGNAL(timeout()), SLOT(Timeout()));

}

SongLoader::~SongLoader() {

#ifdef HAVE_GSTREAMER
  if (pipeline_) {
    state_ = Finished;
    gst_element_set_state(pipeline_.get(), GST_STATE_NULL);
  }
#endif

}

SongLoader::Result SongLoader::Load(const QUrl &url) {

  url_ = url;

  if (url_.isLocalFile()) {
    return LoadLocal(url_.toLocalFile());
  }

  if (sRawUriSchemes.contains(url_.scheme()) || player_->HandlerForUrl(url)) {
    // The URI scheme indicates that it can't possibly be a playlist,
    // or we have a custom handler for the URL, so add it as a raw stream.
    AddAsRawStream();
    return Success;
  }

  if (player_->engine()->type() == Engine::GStreamer) {
#ifdef HAVE_GSTREAMER
    preload_func_ = std::bind(&SongLoader::LoadRemote, this);
    return BlockingLoadRequired;
#else
    errors_ << tr("You need GStreamer for this URL.");
    return Error;
#endif
  }
  else {
    errors_ << tr("You need GStreamer for this URL.");
    return Error;
  }

  return Success;

}

SongLoader::Result SongLoader::LoadFilenamesBlocking() {

  if (preload_func_) {
    return preload_func_();
  }
  else {
    errors_ << tr("Preload function was not set for blocking operation.");
    return Error;
  }

}

SongLoader::Result SongLoader::LoadLocalPartial(const QString &filename) {

  qLog(Debug) << "Fast Loading local file" << filename;
  // First check to see if it's a directory - if so we can load all the songs inside right away.
  if (QFileInfo(filename).isDir()) {
    LoadLocalDirectory(filename);
    return Success;
  }
  Song song;
  song.InitFromFilePartial(filename);
  if (song.is_valid()) {
    songs_ << song;
    return Success;
  }
  else {
    errors_ << song.error();
    return Error;
  }

}

SongLoader::Result SongLoader::LoadAudioCD() {

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  if (player_->engine()->type() == Engine::GStreamer) {
    CddaSongLoader *cdda_song_loader = new CddaSongLoader(QUrl(), this);
    connect(cdda_song_loader, SIGNAL(SongsDurationLoaded(SongList)), this, SLOT(AudioCDTracksLoadedSlot(SongList)));
    connect(cdda_song_loader, SIGNAL(SongsMetadataLoaded(SongList)), this, SLOT(AudioCDTracksTagsLoaded(SongList)));
    cdda_song_loader->LoadSongs();
    return Success;
  }
  else {
#endif
    errors_ << tr("CD playback is only available with the GStreamer engine.");
    return Error;
#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  }
#endif

}

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
void SongLoader::AudioCDTracksLoadedSlot(const SongList &songs) {
  songs_ = songs;
  emit AudioCDTracksLoaded();
}

void SongLoader::AudioCDTracksTagsLoaded(const SongList &songs) {
  CddaSongLoader *cdda_song_loader = qobject_cast<CddaSongLoader*>(sender());
  cdda_song_loader->deleteLater();
  songs_ = songs;
  emit LoadAudioCDFinished(true);
}
#endif

SongLoader::Result SongLoader::LoadLocal(const QString &filename) {

  qLog(Debug) << "Loading local file" << filename;

  // Search in the database.
  QUrl url = QUrl::fromLocalFile(filename);

  CollectionQuery query;
  query.SetColumnSpec("%songs_table.ROWID, " + Song::kColumnSpec);
  query.AddWhere("url", url.toEncoded());

  if (collection_->ExecQuery(&query) && query.Next()) {
    // We may have many results when the file has many sections
    do {
      Song song;
      song.InitFromQuery(query, true);

      if (song.is_valid()) {
        songs_ << song;
      }
    }
    while (query.Next());

    return Success;
  }

  // It's not in the database, load it asynchronously.
  preload_func_ = std::bind(&SongLoader::LoadLocalAsync, this, filename);
  return BlockingLoadRequired;

}

SongLoader::Result SongLoader::LoadLocalAsync(const QString &filename) {

  // First check to see if it's a directory - if so we will load all the songs inside right away.
  if (QFileInfo(filename).isDir()) {
    LoadLocalDirectory(filename);
    return Success;
  }

  // It's a local file, so check if it looks like a playlist. Read the first few bytes.
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    errors_ << tr("Could not open file %1").arg(filename);
    return Error;
  }
  QByteArray data(file.read(PlaylistParser::kMagicSize));

  ParserBase *parser = playlist_parser_->ParserForMagic(data);
  if (!parser) {
    // Check the file extension as well, maybe the magic failed, or it was a basic M3U file which is just a plain list of filenames.
    parser = playlist_parser_->ParserForExtension(QFileInfo(filename).suffix().toLower());
  }

  if (parser) { // It's a playlist!
    qLog(Debug) << "Parsing using" << parser->name();
    LoadPlaylist(parser, filename);
    return Success;
  }

  // Check if it's a cue file
  QString matching_cue = filename.section('.', 0, -2) + ".cue";
  if (QFile::exists(matching_cue)) {
    // it's a cue - create virtual tracks
    QFile cue(matching_cue);
    cue.open(QIODevice::ReadOnly);

    SongList song_list = cue_parser_->Load(&cue, matching_cue, QDir(filename.section('/', 0, -2)));
    for (const Song &song : song_list) {
      if (song.is_valid()) songs_ << song;
    }
    return Success;
  }

  // Assume it's just a normal file
  Song song;
  song.InitFromFilePartial(filename);
  if (song.is_valid()) {
    songs_ << song;
    return Success;
  }
  else {
    errors_ << song.error();
    return Error;
  }

}

void SongLoader::LoadMetadataBlocking() {
  for (int i = 0; i < songs_.size(); i++) {
    EffectiveSongLoad(&songs_[i]);
  }
}

void SongLoader::EffectiveSongLoad(Song *song) {

  if (!song) return;

  if (song->filetype() != Song::FileType_Unknown) {
    // Maybe we loaded the metadata already, for example from a cuesheet.
    return;
  }

  // First, try to get the song from the collection
  Song collection_song = collection_->GetSongByUrl(song->url());
  if (collection_song.is_valid()) {
    *song = collection_song;
  }
  else {
    // it's a normal media file
    QString filename = song->url().toLocalFile();
    TagReaderClient::Instance()->ReadFileBlocking(filename, song);
  }

}

void SongLoader::LoadPlaylist(ParserBase *parser, const QString &filename) {
  QFile file(filename);
  file.open(QIODevice::ReadOnly);
  songs_ = parser->Load(&file, filename, QFileInfo(filename).path());
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
  Song song(Song::Source_Stream);
  song.set_valid(true);
  song.set_filetype(Song::FileType_Stream);
  song.set_url(url_);
  song.set_title(url_.toString());
  songs_ << song;
}

void SongLoader::Timeout() {
  state_ = Finished;
  success_ = false;
  StopTypefind();
}

void SongLoader::StopTypefind() {

#ifdef HAVE_GSTREAMER
  // Destroy the pipeline
  if (pipeline_) {
    gst_element_set_state(pipeline_.get(), GST_STATE_NULL);
    pipeline_.reset();
  }
#endif
  timeout_timer_->stop();

  if (success_ && parser_) {
    qLog(Debug) << "Parsing" << url_ << "with" << parser_->name();

    // Parse the playlist
    QBuffer buf(&buffer_);
    buf.open(QIODevice::ReadOnly);
    songs_ = parser_->Load(&buf);

  }
  else if (success_) {
    qLog(Debug) << "Loading" << url_ << "as raw stream";

    // It wasn't a playlist - just put the URL in as a stream
    AddAsRawStream();
  }

  emit LoadRemoteFinished();

}

#ifdef HAVE_GSTREAMER
SongLoader::Result SongLoader::LoadRemote() {

  qLog(Debug) << "Loading remote file" << url_;

  // It's not a local file so we have to fetch it to see what it is.
  // We use gstreamer to do this since it handles funky URLs for us (http://, ssh://, etc) and also has typefinder plugins.
  // First we wait for typefinder to tell us what it is.  If it's not text/plain or text/uri-list assume it's a song and return success.
  // Otherwise wait to get 512 bytes of data and do magic on it - if the magic fails then we don't know what it is so return failure.
  // If the magic succeeds then we know for sure it's a playlist - so read the rest of the file, parse the playlist and return success.

  timeout_timer_->start(timeout_);

  // Create the pipeline - it gets unreffed if it goes out of scope
  std::shared_ptr<GstElement> pipeline(gst_pipeline_new(nullptr), std::bind(&gst_object_unref, _1));

  // Create the source element automatically based on the URL
  GstElement *source = gst_element_make_from_uri(GST_URI_SRC, url_.toEncoded().constData(), nullptr, nullptr);
  if (!source) {
    errors_ << tr("Couldn't create gstreamer source element for %1").arg(url_.toString());
    return Error;
  }

  // Create the other elements and link them up
  GstElement *typefind = gst_element_factory_make("typefind", nullptr);
  GstElement *fakesink = gst_element_factory_make("fakesink", nullptr);

  gst_bin_add_many(GST_BIN(pipeline.get()), source, typefind, fakesink, nullptr);
  gst_element_link_many(source, typefind, fakesink, nullptr);

  // Connect callbacks
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline.get()));
  CHECKED_GCONNECT(typefind, "have-type", &TypeFound, this);
  gst_bus_set_sync_handler(bus, BusCallbackSync, this, nullptr);
  gst_bus_add_watch(bus, BusCallback, this);

  // Add a probe to the sink so we can capture the data if it's a playlist
  GstPad *pad = gst_element_get_static_pad(fakesink, "sink");
  gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, &DataReady, this, nullptr);
  gst_object_unref(pad);

  QEventLoop loop;
  loop.connect(this, SIGNAL(LoadRemoteFinished()), SLOT(quit()));

  // Start "playing"
  gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
  pipeline_ = pipeline;

  // Wait until loading is finished
  loop.exec();

  return Success;

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::TypeFound(GstElement *, uint, GstCaps *caps, void *self) {

  SongLoader *instance = static_cast<SongLoader*>(self);

  if (instance->state_ != WaitingForType) return;

  // Check the mimetype
  instance->mime_type_ = gst_structure_get_name(gst_caps_get_structure(caps, 0));
  qLog(Debug) << "Mime type is" << instance->mime_type_;
  if (instance->mime_type_ == "text/plain" || instance->mime_type_ == "text/uri-list") {
    // Yeah it might be a playlist, let's get some data and have a better look
    instance->state_ = WaitingForMagic;
    return;
  }

  // Nope, not a playlist - we're done
  instance->StopTypefindAsync(true);

}
#endif

#ifdef HAVE_GSTREAMER
GstPadProbeReturn SongLoader::DataReady(GstPad*, GstPadProbeInfo *info, gpointer self) {

  SongLoader *instance = reinterpret_cast<SongLoader*>(self);

  if (instance->state_ == Finished)
    return GST_PAD_PROBE_OK;

  GstBuffer *buffer = gst_pad_probe_info_get_buffer(info);
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);

  // Append the data to the buffer
  instance->buffer_.append(reinterpret_cast<const char*>(map.data), map.size);
  qLog(Debug) << "Received total" << instance->buffer_.size() << "bytes";
  gst_buffer_unmap(buffer, &map);

  if (instance->state_ == WaitingForMagic && (instance->buffer_.size() >= PlaylistParser::kMagicSize || !instance->IsPipelinePlaying())) {
    // Got enough that we can test the magic
    instance->MagicReady();
  }

  return GST_PAD_PROBE_OK;
}
#endif

#ifdef HAVE_GSTREAMER
gboolean SongLoader::BusCallback(GstBus *, GstMessage *msg, gpointer self) {

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
GstBusSyncReply SongLoader::BusCallbackSync(GstBus *, GstMessage *msg, gpointer self) {

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

  if (state_ == Finished) return;

  GError *error;
  gchar *debugs;

  gst_message_parse_error(msg, &error, &debugs);
  qLog(Error) << __PRETTY_FUNCTION__ << error->message;
  qLog(Error) << __PRETTY_FUNCTION__ << debugs;

  QString message_str = error->message;

  g_error_free(error);
  free(debugs);

  if (state_ == WaitingForType && message_str == gst_error_get_message(GST_STREAM_ERROR, GST_STREAM_ERROR_TYPE_NOT_FOUND)) {
    // Don't give up - assume it's a playlist and see if one of our parsers can read it.
    state_ = WaitingForMagic;
    return;
  }

  StopTypefindAsync(false);

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::EndOfStreamReached() {

  qLog(Debug) << Q_FUNC_INFO << state_;
  switch (state_) {
    case Finished:
      break;

    case WaitingForMagic:
      // Do the magic on the data we have already
      MagicReady();
      if (state_ == Finished) break;
    // It looks like a playlist, so parse it

    // fallthrough
    case WaitingForData:
      // It's a playlist and we've got all the data - finish and parse it
      StopTypefindAsync(true);
      break;

    case WaitingForType:
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
  if (parser_->name() == "ASX/INI" && url_.scheme() == "http") {
    // This is actually a weird MS-WMSP stream. Changing the protocol to MMS from HTTP makes it playable.
    parser_ = nullptr;
    url_.setScheme("mms");
    StopTypefindAsync(true);
  }

  state_ = WaitingForData;

  if (!IsPipelinePlaying()) {
    EndOfStreamReached();
  }

}
#endif

#ifdef HAVE_GSTREAMER
bool SongLoader::IsPipelinePlaying() {

  GstState state = GST_STATE_NULL;
  GstState pending_state = GST_STATE_NULL;
  GstStateChangeReturn ret = gst_element_get_state(pipeline_.get(), &state, &pending_state, GST_SECOND);

  if (ret == GST_STATE_CHANGE_ASYNC && pending_state == GST_STATE_PLAYING) {
    // We're still on the way to playing
    return true;
  }
  return state == GST_STATE_PLAYING;

}
#endif

#ifdef HAVE_GSTREAMER
void SongLoader::StopTypefindAsync(bool success) {

  state_ = Finished;
  success_ = success;

  metaObject()->invokeMethod(this, "StopTypefind", Qt::QueuedConnection);

}
#endif
