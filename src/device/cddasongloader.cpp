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
#include <QScopeGuard>

#include "cddasongloader.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "constants/timeconstants.h"

using std::make_shared;

using namespace Qt::Literals::StringLiterals;

CDDASongLoader::CDDASongLoader(const QUrl &url, QObject *parent)
    : QObject(parent),
      url_(url),
      network_(make_shared<NetworkAccessManager>()) {

#ifdef HAVE_MUSICBRAINZ
  QObject::connect(this, &CDDASongLoader::LoadTagsFromMusicBrainz, this, &CDDASongLoader::LoadTagsFromMusicBrainzSlot);
#endif  // HAVE_MUSICBRAINZ
}

CDDASongLoader::~CDDASongLoader() {
  loading_future_.waitForFinished();
}

QUrl CDDASongLoader::GetUrlFromTrack(int track_number) const {

  if (url_.isEmpty()) {
    return QUrl(QStringLiteral("cdda://%1a").arg(track_number));
  }

  return QUrl(QStringLiteral("cdda://%1/%2").arg(url_.path()).arg(track_number));

}

void CDDASongLoader::LoadSongs() {

  if (IsActive()) {
    return;
  }

  loading_future_ = QtConcurrent::run(&CDDASongLoader::LoadSongsFromCDDA, this);

}

void CDDASongLoader::LoadSongsFromCDDA() {

  QMutexLocker l(&mutex_load_);

  GError *error = nullptr;
  GstElement *cdda = gst_element_factory_make("cdiocddasrc", nullptr);
  if (error) {
    Error(QStringLiteral("%1: %2").arg(error->code).arg(QString::fromUtf8(error->message)));
  }
  if (!cdda) {
    Error(tr("Could not create cdiocddasrc"));
    return;
  }

  if (!url_.isEmpty()) {
    g_object_set(cdda, "device", g_strdup(url_.path().toLocal8Bit().constData()), nullptr);
  }
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(cdda), "paranoia-mode")) {
    g_object_set(cdda, "paranoia-mode", 0, nullptr);
  }

  // Change the element's state to ready and paused, to be able to query it
  if (gst_element_set_state(cdda, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(cdda, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda));
    cdda = nullptr;
    Error(tr("Error while setting CDDA device to ready state."));
    return;
  }

  if (gst_element_set_state(cdda, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(cdda, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda));
    cdda = nullptr;
    Error(tr("Error while setting CDDA device to pause state."));
    return;
  }

  // Get number of tracks
  GstFormat format_track = gst_format_get_by_nick("track");
  GstFormat format_duration = format_track;
  gint64 total_tracks = 0;
  if (!gst_element_query_duration(cdda, format_duration, &total_tracks)) {
    gst_element_set_state(cdda, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda));
    cdda = nullptr;
    Error(tr("Error while querying CDDA tracks."));
    return;
  }

  if (format_duration != format_track) {
    qLog(Error) << "Error while querying CDDA GstElement (2).";
    gst_element_set_state(cdda, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(cdda));
    cdda = nullptr;
    Error(tr("Error while querying CDDA tracks."));
    return;
  }

  QMap<int, Song> songs;
  for (int track_number = 1; track_number <= total_tracks; ++track_number) {
    Song song(Song::Source::CDDA);
    song.set_id(track_number);
    song.set_valid(true);
    song.set_filetype(Song::FileType::CDDA);
    song.set_url(GetUrlFromTrack(track_number));
    song.set_title(QStringLiteral("Track %1").arg(track_number));
    song.set_track(track_number);
    songs.insert(track_number, song);
  }

  Q_EMIT SongsLoaded(songs.values());

#ifdef HAVE_MUSICBRAINZ
  gst_tag_register_musicbrainz_tags();
#endif  // HAVE_MUSICBRAINZ

  GstElement *pipeline = gst_pipeline_new("pipeline");
  GstElement *sink = gst_element_factory_make("fakesink", nullptr);
  gst_bin_add_many(GST_BIN(pipeline), cdda, sink, nullptr);
  gst_element_link(cdda, sink);
  gst_element_set_state(pipeline, GST_STATE_READY);
  gst_element_set_state(pipeline, GST_STATE_PAUSED);

  GstMessage *msg = nullptr;
  int track_artist_tags = 0;
  int track_album_tags = 0;
  int track_title_tags = 0;
#ifdef HAVE_MUSICBRAINZ
  QString musicbrainz_discid;
#endif  // HAVE_MUSICBRAINZ
  GstMessageType msg_filter = static_cast<GstMessageType>(GST_MESSAGE_TOC|GST_MESSAGE_TAG);
  while (msg_filter != 0 && (msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS(pipeline), GST_SECOND * 5, msg_filter))) {

    const QScopeGuard scopeguard_msg = qScopeGuard([msg]() {
      gst_message_unref(msg);
    });

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TOC) {
      GstToc *toc = nullptr;
      gst_message_parse_toc(msg, &toc, nullptr);
      const QScopeGuard scopeguard_toc = qScopeGuard([toc]() {
        gst_toc_unref(toc);
      });
      GList *entries = gst_toc_get_entries(toc);
      int track_number = 0;
      for (GList *entry_node = entries; entry_node != nullptr; entry_node = entry_node->next) {
        ++track_number;
        if (songs.contains(track_number)) {
          Song &song = songs[track_number];
          GstTocEntry *entry = static_cast<GstTocEntry*>(entry_node->data);
          gint64 start = 0, stop = 0;
          if (gst_toc_entry_get_start_stop_times(entry, &start, &stop)) {
            song.set_length_nanosec(static_cast<qint64>(stop - start));
          }
        }
        msg_filter = static_cast<GstMessageType>(static_cast<int>(msg_filter) ^ GST_MESSAGE_TOC);
      }
    }

    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG) {

      GstTagList *tags = nullptr;
      gst_message_parse_tag(msg, &tags);
      const QScopeGuard scopeguard_tags = qScopeGuard([tags]() {
        gst_tag_list_free(tags);
      });

      gint64 track_index = 0;
      gst_element_query_position(cdda, format_track, &track_index);

      char *tag = nullptr;

#ifdef HAVE_MUSICBRAINZ
      if (musicbrainz_discid.isEmpty()) {
        if (gst_tag_list_get_string(tags, GST_TAG_CDDA_MUSICBRAINZ_DISCID, &tag)) {
          musicbrainz_discid = QString::fromUtf8(tag);
          g_free(tag);
          tag = nullptr;
        }
      }
#endif

      guint track_number = 0;
      if (!gst_tag_list_get_uint(tags, GST_TAG_TRACK_NUMBER, &track_number)) {
        qLog(Error) << "Could not get track number";
        msg_filter = static_cast<GstMessageType>(static_cast<int>(msg_filter) ^GST_MESSAGE_TAG);
        continue;
      }

      if (!songs.contains(track_number)) {
        qLog(Error) << "Got invalid track number" << track_number;
        msg_filter = static_cast<GstMessageType>(static_cast<int>(msg_filter) ^GST_MESSAGE_TAG);
        continue;
      }

      Song &song = songs[track_number];
      guint64 duration = 0;
      if (gst_tag_list_get_uint64(tags, GST_TAG_DURATION, &duration)) {
        song.set_length_nanosec(static_cast<qint64>(duration));
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ALBUM_ARTIST, &tag)) {
        song.set_albumartist(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ALBUM_ARTIST_SORTNAME, &tag)) {
        song.set_albumartistsort(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &tag)) {
        song.set_artist(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
        ++track_artist_tags;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ARTIST_SORTNAME, &tag)) {
        song.set_artistsort(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ALBUM, &tag)) {
        song.set_album(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
        ++track_album_tags;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_ALBUM_SORTNAME, &tag)) {
        song.set_albumsort(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &tag)) {
        song.set_title(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
        ++track_title_tags;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_TITLE_SORTNAME, &tag)) {
        song.set_titlesort(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_GENRE, &tag)) {
        song.set_genre(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_COMPOSER, &tag)) {
        song.set_composer(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_COMPOSER_SORTNAME, &tag)) {
        song.set_composersort(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_PERFORMER, &tag)) {
        song.set_performer(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      if (gst_tag_list_get_string(tags, GST_TAG_COMMENT, &tag)) {
        song.set_comment(QString::fromUtf8(tag));
        g_free(tag);
        tag = nullptr;
      }
      guint bitrate = 0;
      if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &bitrate)) {
        song.set_bitrate(static_cast<int>(bitrate));
      }

      if (track_number >= total_tracks) {
        msg_filter = static_cast<GstMessageType>(static_cast<int>(msg_filter) ^GST_MESSAGE_TAG);
        continue;
      }

      const gint64 next_track_index = track_index + 1;
      if (!gst_element_seek_simple(pipeline, format_track, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE), next_track_index)) {
        qLog(Error) << "Failed to seek to next track index" << next_track_index;
        msg_filter = static_cast<GstMessageType>(static_cast<int>(msg_filter) ^GST_MESSAGE_TAG);
      }

    }
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
  // This will also cause cdda to be unref'd.
  gst_object_unref(pipeline);

  if ((track_artist_tags >= total_tracks && track_album_tags >= total_tracks && track_title_tags >= total_tracks)) {
    qLog(Info) << "Songs loaded from CD-Text";
    Q_EMIT SongsUpdated(songs.values());
    Q_EMIT LoadingFinished();
  }
  else {
#ifdef HAVE_MUSICBRAINZ
    if (musicbrainz_discid.isEmpty()) {
      qLog(Info) << "CD is missing tags";
      Q_EMIT LoadingFinished();
    }
    else {
      qLog(Info) << "MusicBrainz Disc ID:" << musicbrainz_discid;
      Q_EMIT LoadTagsFromMusicBrainz(musicbrainz_discid);
    }
#else
    Q_EMIT LoadingFinished();
#endif  // HAVE_MUSICBRAINZ
  }

}

#ifdef HAVE_MUSICBRAINZ

void CDDASongLoader::LoadTagsFromMusicBrainzSlot(const QString &musicbrainz_discid) const {

  MusicBrainzClient *musicbrainz_client = new MusicBrainzClient(network_);
  QObject::connect(musicbrainz_client, &MusicBrainzClient::DiscIdFinished, this, &CDDASongLoader::LoadTagsFromMusicBrainzFinished);
  musicbrainz_client->StartDiscIdRequest(musicbrainz_discid);

}

void CDDASongLoader::LoadTagsFromMusicBrainzFinished(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &results, const QString &error) {

  MusicBrainzClient *musicbrainz_client = qobject_cast<MusicBrainzClient*>(sender());
  musicbrainz_client->deleteLater();

  if (!error.isEmpty()) {
    Error(error);
    return;
  }

  if (results.empty()) {
    Q_EMIT LoadingFinished();
    return;
  }

  SongList songs;
  songs.reserve(results.count());
  int track_number = 0;
  for (const MusicBrainzClient::Result &result : results) {
    ++track_number;
    Song song(Song::Source::CDDA);
    song.set_artist(artist);
    song.set_album(album);
    song.set_title(result.title_);
    song.set_length_nanosec(result.duration_msec_ * kNsecPerMsec);
    song.set_track(track_number);
    song.set_year(result.year_);
    song.set_id(track_number);
    song.set_filetype(Song::FileType::CDDA);
    song.set_valid(true);
    // We need to set URL, that's how playlist will find the correct item to update
    song.set_url(GetUrlFromTrack(track_number));
    songs << song;
  }

  Q_EMIT SongsUpdated(songs);
  Q_EMIT LoadingFinished();

}

#endif  // HAVE_MUSICBRAINZ

void CDDASongLoader::Error(const QString &error) {

  qLog(Error) << error;

  Q_EMIT LoadError(error);
  Q_EMIT LoadingFinished();

}
