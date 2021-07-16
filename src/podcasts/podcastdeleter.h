/*
 * Strawberry Music Player
 * This file was part of Clementine.
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

#ifndef PODCASTDELETER_H
#define PODCASTDELETER_H

#include <QObject>

#include "podcast.h"
#include "podcastepisode.h"

class QTimer;

class Application;
class PodcastBackend;

class PodcastDeleter : public QObject {
  Q_OBJECT

 public:
  explicit PodcastDeleter(Application *app, QObject *parent = nullptr);
  static const char *kSettingsGroup;
  static const int kAutoDeleteCheckIntervalMsec;

 public slots:
  // Deletes downloaded data for this episode
  void DeleteEpisode(const PodcastEpisode &episode);
  void AutoDelete();
  void ReloadSettings();

 private:
  Application *app_;
  PodcastBackend *backend_;
  int delete_after_secs_;
  QTimer *auto_delete_timer_;
};

#endif  // PODCASTDELETER_H
