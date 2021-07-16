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

#include "podcastupdater.h"

#include <QObject>
#include <QSet>
#include <QList>
#include <QUrl>
#include <QDateTime>
#include <QTimer>
#include <QSettings>

#include "core/application.h"
#include "core/logging.h"
#include "core/timeconstants.h"
#include "podcastbackend.h"
#include "podcasturlloader.h"

const char *PodcastUpdater::kSettingsGroup = "Podcasts";

PodcastUpdater::PodcastUpdater(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      update_interval_secs_(0),
      update_timer_(new QTimer(this)),
      loader_(new PodcastUrlLoader(this)),
      pending_replies_(0) {

  update_timer_->setSingleShot(true);

  QObject::connect(app_, &Application::SettingsChanged, this, &PodcastUpdater::ReloadSettings);
  QObject::connect(update_timer_, &QTimer::timeout, this, &PodcastUpdater::UpdateAllPodcastsNow);
  QObject::connect(app_->podcast_backend(), &PodcastBackend::SubscriptionAdded, this, &PodcastUpdater::SubscriptionAdded);

  ReloadSettings();

}

void PodcastUpdater::ReloadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  last_full_update_ = s.value("last_full_update").toDateTime();
  update_interval_secs_ = s.value("update_interval_secs").toInt();

  s.endGroup();

  RestartTimer();

}

void PodcastUpdater::SaveSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("last_full_update", last_full_update_);
  s.endGroup();

}

void PodcastUpdater::RestartTimer() {

  // Stop any existing timer
  update_timer_->stop();

  if (pending_replies_ > 0) {
    // We're still waiting for replies from the last update - don't do anything.
    return;
  }

  if (update_interval_secs_ > 0) {
    if (!last_full_update_.isValid()) {
      // Updates are enabled and we've never updated before.  Do it now.
      qLog(Info) << "Updating podcasts for the first time";
      UpdateAllPodcastsNow();
    }
    else {
      const QDateTime next_update = last_full_update_.addSecs(update_interval_secs_);
      const int secs_until_next_update = QDateTime::currentDateTime().secsTo(next_update);

      if (secs_until_next_update < 0) {
        qLog(Info) << "Updating podcasts" << (-secs_until_next_update) << "seconds late";
        UpdateAllPodcastsNow();
      }
      else {
        qLog(Info) << "Updating podcasts at" << next_update << "(in" << secs_until_next_update << "seconds)";
        update_timer_->start(secs_until_next_update * kMsecPerSec);
      }
    }
  }

}

void PodcastUpdater::SubscriptionAdded(const Podcast& podcast) {

  // Only update a new podcast immediately if it doesn't have an episode list.
  // We assume that the episode list has already been fetched recently otherwise.
  if (podcast.episodes().isEmpty()) {
    UpdatePodcastNow(podcast);
  }

}

void PodcastUpdater::UpdatePodcastNow(const Podcast& podcast) {

  PodcastUrlLoaderReply *reply = loader_->Load(podcast.url());
  QObject::connect(reply, &PodcastUrlLoaderReply::Finished, this, [this, reply, podcast]() { PodcastLoaded(reply, podcast, false); });

}

void PodcastUpdater::UpdateAllPodcastsNow() {

  PodcastList podcasts = app_->podcast_backend()->GetAllSubscriptions();
  for (const Podcast &podcast : podcasts) {
    PodcastUrlLoaderReply *reply = loader_->Load(podcast.url());
    QObject::connect(reply, &PodcastUrlLoaderReply::Finished, this, [this, reply, podcast]() { PodcastLoaded(reply, podcast, true); });

    ++pending_replies_;
  }

}

void PodcastUpdater::PodcastLoaded(PodcastUrlLoaderReply *reply, const Podcast& podcast, bool one_of_many) {

  reply->deleteLater();

  if (one_of_many) {
    --pending_replies_;
    if (pending_replies_ == 0) {
      // This was the last reply we were waiting for.  Save this time as being
      // the last successful update and restart the timer.
      last_full_update_ = QDateTime::currentDateTime();
      SaveSettings();
      RestartTimer();
    }
  }

  if (!reply->is_success()) {
    qLog(Warning) << "Error fetching podcast at" << podcast.url() << ":" << reply->error_text();
    return;
  }

  if (reply->result_type() != PodcastUrlLoaderReply::Type_Podcast) {
    qLog(Warning) << "The URL" << podcast.url() << "no longer contains a podcast";
    return;
  }

  // Get the episode URLs we had for this podcast already.
  QSet<QUrl> existing_urls;
  for (const PodcastEpisode &episode :
    app_->podcast_backend()->GetEpisodes(podcast.database_id())) {
    existing_urls.insert(episode.url());
  }

  // Add any new episodes
  PodcastEpisodeList new_episodes;
  PodcastList reply_podcasts = reply->podcast_results();
  for (const Podcast &reply_podcast : reply_podcasts) {
    PodcastEpisodeList episodes = reply_podcast.episodes();
    for (const PodcastEpisode &episode : episodes) {
      if (!existing_urls.contains(episode.url())) {
        PodcastEpisode episode_copy(episode);
        episode_copy.set_podcast_database_id(podcast.database_id());
        new_episodes.append(episode_copy);
      }
    }
  }

  app_->podcast_backend()->AddEpisodes(&new_episodes);
  qLog(Info) << "Added" << new_episodes.count() << "new episodes for" << podcast.url();

}
