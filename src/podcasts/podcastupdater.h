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

#ifndef PODCASTUPDATER_H
#define PODCASTUPDATER_H

#include <QObject>
#include <QDateTime>

class Application;
class Podcast;
class PodcastUrlLoader;
class PodcastUrlLoaderReply;

class QTimer;

// Responsible for updating podcasts when they're first subscribed to, and then updating them at regular intervals afterwards.
class PodcastUpdater : public QObject {
  Q_OBJECT

 public:
  explicit PodcastUpdater(Application *app, QObject *parent = nullptr);

 public slots:
  void UpdateAllPodcastsNow();
  void UpdatePodcastNow(const Podcast &podcast);

 private slots:
  void ReloadSettings();

  void SubscriptionAdded(const Podcast &podcast);
  void PodcastLoaded(PodcastUrlLoaderReply *reply, const Podcast &podcast, const bool one_of_many);

 private:
  void RestartTimer();
  void SaveSettings();

 private:
  static const char *kSettingsGroup;

  Application *app_;

  QDateTime last_full_update_;
  int update_interval_secs_;

  QTimer *update_timer_;
  PodcastUrlLoader *loader_;
  int pending_replies_;
};

#endif  // PODCASTUPDATER_H
