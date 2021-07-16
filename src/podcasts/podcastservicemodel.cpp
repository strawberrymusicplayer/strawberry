/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QVariant>
#include <QUrl>
#include <QMimeData>

#include "podcastservicemodel.h"

#include "podcastservice.h"
#include "playlist/songmimedata.h"

PodcastServiceModel::PodcastServiceModel(QObject* parent) : QStandardItemModel(parent) {}

QMimeData* PodcastServiceModel::mimeData(const QModelIndexList &indexes) const {
  SongMimeData *data = new SongMimeData;
  QList<QUrl> urls;
#if 0
  for (const QModelIndex& index : indexes) {
    switch (index.data(InternetModel::Role_Type).toInt()) {
      case PodcastService::Type_Episode:
        MimeDataForEpisode(index, data, &urls);
        break;

      case PodcastService::Type_Podcast:
        MimeDataForPodcast(index, data, &urls);
        break;
    }
  }
#endif

  data->setUrls(urls);
  return data;

}

void PodcastServiceModel::MimeDataForEpisode(const QModelIndex &idx, SongMimeData *data, QList<QUrl>* urls) const {

  QVariant episode_variant = idx.data(PodcastService::Role_Episode);
  if (!episode_variant.isValid()) return;

  PodcastEpisode episode(episode_variant.value<PodcastEpisode>());

  // Get the podcast from the index's parent
  Podcast podcast;
  QVariant podcast_variant = idx.parent().data(PodcastService::Role_Podcast);
  if (podcast_variant.isValid()) {
    podcast = podcast_variant.value<Podcast>();
  }

  Song song = episode.ToSong(podcast);

  data->songs << song;
  *urls << song.url();

}

void PodcastServiceModel::MimeDataForPodcast(const QModelIndex &idx, SongMimeData *data, QList<QUrl> *urls) const {

  // Get the podcast
  Podcast podcast;
  QVariant podcast_variant = idx.data(PodcastService::Role_Podcast);
  if (podcast_variant.isValid()) {
    podcast = podcast_variant.value<Podcast>();
  }

  // Add each child episode
  const int children = idx.model()->rowCount(idx);
  for (int i = 0; i < children; ++i) {
    QVariant episode_variant = idx.model()->index(i, 0, idx).data(PodcastService::Role_Episode);
    if (!episode_variant.isValid()) continue;

    PodcastEpisode episode(episode_variant.value<PodcastEpisode>());
    Song song = episode.ToSong(podcast);

    data->songs << song;
    *urls << song.url();
  }

}
