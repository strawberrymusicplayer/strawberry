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

#ifndef PODCASTEPISODE_H
#define PODCASTEPISODE_H

#include <QSharedDataPointer>
#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDateTime>
#include <QSqlQuery>

#include "core/song.h"

class Podcast;

class PodcastEpisode {
 public:
  PodcastEpisode();
  PodcastEpisode(const PodcastEpisode &other);
  ~PodcastEpisode();

  static const QStringList kColumns;
  static const QString kColumnSpec;
  static const QString kJoinSpec;
  static const QString kBindSpec;
  static const QString kUpdateSpec;

  void InitFromQuery(const QSqlQuery &query);
  void BindToQuery(QSqlQuery *query) const;

  Song ToSong(const Podcast &podcast) const;

  bool is_valid() const { return database_id() != -1; }

  int database_id() const;
  int podcast_database_id() const;
  const QString &title() const;
  const QString &description() const;
  const QString &author() const;
  const QDateTime &publication_date() const;
  int duration_secs() const;
  const QUrl &url() const;
  bool listened() const;
  const QDateTime &listened_date() const;
  bool downloaded() const;
  const QUrl &local_url() const;
  const QVariantMap &extra() const;
  QVariant extra(const QString &key) const;

  void set_database_id(const int v);
  void set_podcast_database_id(int v);
  void set_title(const QString &v);
  void set_description(const QString &v);
  void set_author(const QString &v);
  void set_publication_date(const QDateTime &v);
  void set_duration_secs(int v);
  void set_url(const QUrl &v);
  void set_listened(const bool v);
  void set_listened_date(const QDateTime &v);
  void set_downloaded(const bool v);
  void set_local_url(const QUrl &v);
  void set_extra(const QVariantMap &v);
  void set_extra(const QString &key, const QVariant &value);

  PodcastEpisode &operator=(const PodcastEpisode &other);

 private:
  struct Private;
  QSharedDataPointer<Private> d;
};
Q_DECLARE_METATYPE(PodcastEpisode)

typedef QList<PodcastEpisode> PodcastEpisodeList;
Q_DECLARE_METATYPE(QList<PodcastEpisode>)

#endif  // PODCASTEPISODE_H
