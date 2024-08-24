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

#include <memory>

#include <cstddef>
#include <glib.h>
#include <glib/gtypes.h>
#include <glib-object.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include <cdio/cdio.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#include "cddasongloader.h"
#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/timeconstants.h"

using std::make_shared;

CddaSongLoader::CddaSongLoader(const QUrl &url, QObject *parent)
    : QObject(parent),
      url_(url),
      network_(make_shared<NetworkAccessManager>()),
      cdda_(nullptr),
      cdio_(nullptr) {}

CddaSongLoader::~CddaSongLoader() {
  if (cdio_) cdio_destroy(cdio_);
}

QUrl CddaSongLoader::GetUrlFromTrack(int track_number) const {

  if (url_.isEmpty()) {
    return QUrl(QStringLiteral("cdda://%1a").arg(track_number));
  }

  return QUrl(QStringLiteral("cdda://%1/%2").arg(url_.path()).arg(track_number));

}

void CddaSongLoader::LoadSongs() {

  QMutexLocker locker(&mutex_load_);
  cdio_ = cdio_open(url_.path().toLocal8Bit().constData(), DRIVER_DEVICE);
  if (cdio_ == nullptr) {
    Error(QStringLiteral("Unable to open CDIO device."));
    return;
  }

  // Create gstreamer cdda element
  GError *error = nullptr;
  cdda_ = gst_element_make_from_uri(GST_URI_SRC, "cdda://", nullptr, &error);
  if (error) {
    Error(QStringLiteral("%1: %2").arg(error->code).arg(QString::fromUtf8(error->message)));
  }
  if (!cdda_) return;

  if (!url_.isEmpty()) {
    g_object_set(cdda_, "device", g_strdup(url_.path().toLocal8Bit().constData()), nullptr);
  }
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(cdda_), "paranoia-mode")) {
    g_object_set(cdda_, "paranoia-mode", 0, nullptr);
  }

  // Change the element's state to ready and paused, to be able to query it
  if (gst_element_set_state(cdda_, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while setting CDDA device to ready state."));
    return;
  }

  if (gst_element_set_state(cdda_, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while setting CDDA device to pause state."));
    return;
  }

  // Get number of tracks
  GstFormat fmt = gst_format_get_by_nick("track");
  GstFormat out_fmt = fmt;
  gint64 num_tracks = 0;
  if (!gst_element_query_duration(cdda_, out_fmt, &num_tracks)) {
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while querying CDDA tracks."));
    return;
  }

  if (out_fmt != fmt) {
    qLog(Error) << "Error while querying cdda GstElement (2).";
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while querying CDDA tracks."));
    return;
  }

  SongList songs;
  songs.reserve(num_tracks);
  for (int track_number = 1; track_number <= num_tracks; ++track_number) {
    // Init song
    Song song(Song::Source::CDDA);
    song.set_id(track_number);
    song.set_valid(true);
    song.set_filetype(Song::FileType::CDDA);
    song.set_url(GetUrlFromTrack(track_number));
    song.set_title(QStringLiteral("Track %1").arg(track_number));
    song.set_track(track_number);
    songs << song;
  }
  Q_EMIT SongsLoaded(songs);


  gst_tag_register_musicbrainz_tags();

  GstElement *pipeline = gst_pipeline_new("pipeline");
  GstElement *sink = gst_element_factory_make("fakesink", nullptr);
  gst_bin_add_many(GST_BIN(pipeline), cdda_, sink, nullptr);
  gst_element_link(cdda_, sink);
  gst_element_set_state(pipeline, GST_STATE_READY);
  gst_element_set_state(pipeline, GST_STATE_PAUSED);

  // Get TOC and TAG messages
  GstMessage *msg = nullptr;
  GstMessage *msg_toc = nullptr;
  GstMessage *msg_tag = nullptr;
  while ((msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS(pipeline), GST_SECOND, static_cast<GstMessageType>(GST_MESSAGE_TOC | GST_MESSAGE_TAG)))) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TOC) {
      if (msg_toc) gst_message_unref(msg_toc);  // Shouldn't happen, but just in case
      msg_toc = msg;
    }
    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG) {
      if (msg_tag) gst_message_unref(msg_tag);
      msg_tag = msg;
    }
  }

  // Handle TOC message: get tracks duration
  if (msg_toc) {
    GstToc *toc = nullptr;
    gst_message_parse_toc(msg_toc, &toc, nullptr);
    if (toc) {
      GList *entries = gst_toc_get_entries(toc);
      if (entries && static_cast<guint>(songs.size()) <= g_list_length(entries)) {
        int i = 0;
        for (GList *node = entries; node != nullptr; node = node->next) {
          GstTocEntry *entry = static_cast<GstTocEntry*>(node->data);
          qint64 duration = 0;
          gint64 start = 0, stop = 0;
          if (gst_toc_entry_get_start_stop_times(entry, &start, &stop)) duration = stop - start;
          songs[i++].set_length_nanosec(duration);
        }
      }
    }
    gst_message_unref(msg_toc);
  }
  Q_EMIT SongsDurationLoaded(songs);

#ifdef HAVE_MUSICBRAINZ
  // Handle TAG message: generate MusicBrainz DiscId
  if (msg_tag) {
    GstTagList *tags = nullptr;
    gst_message_parse_tag(msg_tag, &tags);
    char *string_mb = nullptr;
    if (gst_tag_list_get_string(tags, GST_TAG_CDDA_MUSICBRAINZ_DISCID, &string_mb)) {
      QString musicbrainz_discid = QString::fromUtf8(string_mb);
      qLog(Info) << "MusicBrainz discid: " << musicbrainz_discid;

      MusicBrainzClient *musicbrainz_client = new MusicBrainzClient(network_);
      QObject::connect(musicbrainz_client, &MusicBrainzClient::DiscIdFinished, this, &CddaSongLoader::AudioCDTagsLoaded);
      musicbrainz_client->StartDiscIdRequest(musicbrainz_discid);
      g_free(string_mb);
      gst_message_unref(msg_tag);
      gst_tag_list_unref(tags);
    }
  }
#endif

  gst_element_set_state(pipeline, GST_STATE_NULL);
  // This will also cause cdda_ to be unref'd.
  gst_object_unref(pipeline);

}

#ifdef HAVE_MUSICBRAINZ
void CddaSongLoader::AudioCDTagsLoaded(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &results) {

  MusicBrainzClient *musicbrainz_client = qobject_cast<MusicBrainzClient*>(sender());
  musicbrainz_client->deleteLater();
  if (results.empty()) return;
  SongList songs;
  songs.reserve(results.count());
  int track_number = 1;
  for (const MusicBrainzClient::Result &ret : results) {
    Song song(Song::Source::CDDA);
    song.set_artist(artist);
    song.set_album(album);
    song.set_title(ret.title_);
    song.set_length_nanosec(ret.duration_msec_ * kNsecPerMsec);
    song.set_track(track_number);
    song.set_year(ret.year_);
    song.set_id(track_number);
    song.set_filetype(Song::FileType::CDDA);
    song.set_valid(true);
    // We need to set url: that's how playlist will find the correct item to update
    song.set_url(GetUrlFromTrack(track_number++));
    songs << song;
  }
  Q_EMIT SongsMetadataLoaded(songs);

}
#endif

bool CddaSongLoader::HasChanged() {

  if (cdio_ && cdio_get_media_changed(cdio_) != 1) {
    return false;
  }
  // Check if mutex is already token (i.e. init is already taking place)
  if (!mutex_load_.tryLock()) {
    return false;
  }
  mutex_load_.unlock();

  return true;

}

void CddaSongLoader::Error(const QString &error) {

  qLog(Error) << error;
  Q_EMIT SongsDurationLoaded(SongList(), error);

}
