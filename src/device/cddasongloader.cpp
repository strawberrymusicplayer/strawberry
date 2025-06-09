/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>

#include <memory>

#include <glib.h>
#include <glib/gtypes.h>
#include <glib-object.h>

#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QtConcurrentRun>

#include "cddasongloader.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "constants/timeconstants.h"

using std::make_shared;

using namespace Qt::Literals::StringLiterals;

CddaSongLoader::CddaSongLoader(const QUrl &url, QObject *parent)
    : QObject(parent),
      url_(url),
      network_(make_shared<NetworkAccessManager>()),
      cdda_(nullptr) {

  QObject::connect(this, &CddaSongLoader::MusicBrainzDiscIdLoaded, this, &CddaSongLoader::LoadMusicBrainzCDTags);

}

CddaSongLoader::~CddaSongLoader() {
  loading_future_.waitForFinished();
}

QUrl CddaSongLoader::GetUrlFromTrack(int track_number) const {

  if (url_.isEmpty()) {
    return QUrl(QStringLiteral("cdda://%1a").arg(track_number));
  }

  return QUrl(QStringLiteral("cdda://%1/%2").arg(url_.path()).arg(track_number));

}

void CddaSongLoader::LoadSongs() {

  if (IsActive()) {
    return;
  }

  loading_future_ = QtConcurrent::run(&CddaSongLoader::LoadSongsFromCDDA, this);

}

void CddaSongLoader::LoadSongsFromCDDA() {

  QMutexLocker l(&mutex_load_);

  GError *error = nullptr;
  cdda_ = gst_element_make_from_uri(GST_URI_SRC, "cdda://", nullptr, &error);
  if (error) {
    Error(QStringLiteral("%1: %2").arg(error->code).arg(QString::fromUtf8(error->message)));
  }
  if (!cdda_) {
    Q_EMIT SongLoadingFinished();
    return;
  }

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
    Q_EMIT SongLoadingFinished();
    return;
  }

  if (gst_element_set_state(cdda_, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while setting CDDA device to pause state."));
    Q_EMIT SongLoadingFinished();
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
    Q_EMIT SongLoadingFinished();
    return;
  }

  if (out_fmt != fmt) {
    qLog(Error) << "Error while querying cdda GstElement (2).";
    gst_element_set_state(cdda_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda_));
    cdda_ = nullptr;
    Error(tr("Error while querying CDDA tracks."));
    Q_EMIT SongLoadingFinished();
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
  while ((!msg_toc || !msg_tag) && (msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS(pipeline), GST_SECOND * 5, static_cast<GstMessageType>(GST_MESSAGE_TOC | GST_MESSAGE_TAG)))) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TOC) {
      if (msg_toc) gst_message_unref(msg_toc);
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
      if (entries) {
        if (static_cast<guint>(songs.size()) <= g_list_length(entries)) {
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
      gst_toc_unref(toc);
    }
    gst_message_unref(msg_toc);
  }
  Q_EMIT SongsDurationLoaded(songs);

  QString musicbrainz_discid;
#ifdef HAVE_MUSICBRAINZ
  // Handle TAG message: generate MusicBrainz DiscId
  if (msg_tag) {
    GstTagList *tags = nullptr;
    gst_message_parse_tag(msg_tag, &tags);
    if (tags) {
      char *string_mb = nullptr;
      if (gst_tag_list_get_string(tags, GST_TAG_CDDA_MUSICBRAINZ_DISCID, &string_mb)) {
        musicbrainz_discid = QString::fromUtf8(string_mb);
        g_free(string_mb);
      }
      gst_tag_list_free(tags);
    }
    gst_message_unref(msg_tag);
  }
#endif

  gst_element_set_state(pipeline, GST_STATE_NULL);
  // This will also cause cdda_ to be unref'd.
  gst_object_unref(pipeline);

  if (musicbrainz_discid.isEmpty()) {
    Q_EMIT SongLoadingFinished();
  }
  else {
    qLog(Info) << "MusicBrainz Disc ID:" << musicbrainz_discid;
    Q_EMIT MusicBrainzDiscIdLoaded(musicbrainz_discid);
  }

}

#ifdef HAVE_MUSICBRAINZ

void CddaSongLoader::LoadMusicBrainzCDTags(const QString &musicbrainz_discid) const {

  MusicBrainzClient *musicbrainz_client = new MusicBrainzClient(network_);
  QObject::connect(musicbrainz_client, &MusicBrainzClient::DiscIdFinished, this, &CddaSongLoader::MusicBrainzCDTagsLoaded);
  musicbrainz_client->StartDiscIdRequest(musicbrainz_discid);

}

void CddaSongLoader::MusicBrainzCDTagsLoaded(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &results) {

  MusicBrainzClient *musicbrainz_client = qobject_cast<MusicBrainzClient*>(sender());
  musicbrainz_client->deleteLater();

  if (results.empty()) {
    Q_EMIT SongLoadingFinished();
    return;
  }

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
    // We need to set URL, that's how playlist will find the correct item to update
    song.set_url(GetUrlFromTrack(track_number++));
    songs << song;
  }

  Q_EMIT SongsMetadataLoaded(songs);
  Q_EMIT SongLoadingFinished();

}

#endif  // HAVE_MUSICBRAINZ

void CddaSongLoader::Error(const QString &error) {

  qLog(Error) << error;
  Q_EMIT SongsDurationLoaded(SongList(), error);

}
