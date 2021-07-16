/*
 * Strawberry Music Player
 * This file was part of Clementine.
   Copyright 2018, Jim Broadus <jbroadus@gmail.com>
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

#include <QDialog>

#include "core/application.h"
#include "podcastepisode.h"
#include "podcastinfodialog.h"
#include "ui_podcastinfodialog.h"

PodcastInfoDialog::PodcastInfoDialog(Application *app, QWidget *parent)
    : QDialog(parent), app_(app), ui_(new Ui_PodcastInfoDialog) {

  ui_->setupUi(this);
  ui_->podcast_details->SetApplication(app);
  ui_->episode_details->SetApplication(app);

}

PodcastInfoDialog::~PodcastInfoDialog() { delete ui_; }

void PodcastInfoDialog::ShowPodcast(const Podcast &podcast) {

  ui_->episode_info_scroll_area->hide();
  ui_->podcast_url->setText(podcast.url().toString());
  ui_->podcast_url->setReadOnly(true);
  ui_->podcast_details->SetPodcast(podcast);
  show();

}

void PodcastInfoDialog::ShowEpisode(const PodcastEpisode &episode, const Podcast &podcast) {

  ui_->episode_info_scroll_area->show();
  ui_->podcast_url->setText(episode.url().toString());
  ui_->podcast_url->setReadOnly(true);
  ui_->podcast_details->SetPodcast(podcast);
  ui_->episode_details->SetEpisode(episode);
  show();

}
