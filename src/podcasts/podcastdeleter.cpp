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

#include <QObject>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QSettings>

#include "core/application.h"
#include "core/logging.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "collection/collectiondirectorymodel.h"
#include "collection/collectionmodel.h"
#include "podcastbackend.h"
#include "podcastdeleter.h"

const char *PodcastDeleter::kSettingsGroup = "Podcasts";
const int PodcastDeleter::kAutoDeleteCheckIntervalMsec = 60 * 6 * 60 * kMsecPerSec;

PodcastDeleter::PodcastDeleter(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      backend_(app_->podcast_backend()),
      delete_after_secs_(0),
      auto_delete_timer_(new QTimer(this)) {

  ReloadSettings();
  auto_delete_timer_->setSingleShot(true);
  AutoDelete();
  QObject::connect(auto_delete_timer_, &QTimer::timeout, this, &PodcastDeleter::AutoDelete);
  QObject::connect(app_, &Application::SettingsChanged, this, &PodcastDeleter::ReloadSettings);

}

void PodcastDeleter::DeleteEpisode(const PodcastEpisode &episode) {

  // Delete the local file
  if (!QFile::remove(episode.local_url().toLocalFile())) {
    qLog(Warning) << "The local file" << episode.local_url().toLocalFile() << "could not be removed";
  }

  // Update the episode in the DB
  PodcastEpisode episode_copy(episode);
  episode_copy.set_downloaded(false);
  episode_copy.set_local_url(QUrl());
  episode_copy.set_listened_date(QDateTime());
  backend_->UpdateEpisodes(PodcastEpisodeList() << episode_copy);

}

void PodcastDeleter::ReloadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  delete_after_secs_ = s.value("delete_after", 0).toInt();
  s.endGroup();

  AutoDelete();

}

void PodcastDeleter::AutoDelete() {

  if (delete_after_secs_ <= 0) {
    return;
  }
  auto_delete_timer_->stop();
  QDateTime max_date = QDateTime::currentDateTime();
  qint64 timeout_ms;
  PodcastEpisode oldest_episode;
  QDateTime oldest_episode_time;
  max_date = max_date.addSecs(-delete_after_secs_);

  PodcastEpisodeList old_episodes = backend_->GetOldDownloadedEpisodes(max_date);

  qLog(Info) << "Deleting" << old_episodes.count()
             << "episodes because they were last listened to"
             << (delete_after_secs_ / kSecsPerDay) << "days ago";

  for (const PodcastEpisode& episode : old_episodes) {
    DeleteEpisode(episode);
  }

  oldest_episode = backend_->GetOldestDownloadedListenedEpisode();
  if (!oldest_episode.listened_date().isValid()) {
    oldest_episode_time = QDateTime::currentDateTime();
  }
  else {
    oldest_episode_time = oldest_episode.listened_date();
  }

  timeout_ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
  timeout_ms -= oldest_episode_time.toMSecsSinceEpoch();
  timeout_ms = (delete_after_secs_ * kMsecPerSec) - timeout_ms;
  qLog(Info) << "Timeout for autodelete set to:" << timeout_ms << "ms";
  if (timeout_ms >= 0) {
    auto_delete_timer_->setInterval(timeout_ms);
  }
  else {
    auto_delete_timer_->setInterval(kAutoDeleteCheckIntervalMsec);
  }
  auto_delete_timer_->start();

}
