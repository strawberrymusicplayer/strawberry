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

#ifndef PODCASTINFODIALOG_H
#define PODCASTINFODIALOG_H

#include <QDialog>

class Application;
class Podcast;
class PodcastEpisode;
class Ui_PodcastInfoDialog;

class PodcastInfoDialog : public QDialog {
  Q_OBJECT

 public:
  explicit PodcastInfoDialog(Application *app, QWidget *parent = nullptr);
  ~PodcastInfoDialog();

  void ShowPodcast(const Podcast &podcast);
  void ShowEpisode(const PodcastEpisode &episode, const Podcast &podcast);

 private:
  Application *app_;

  Ui_PodcastInfoDialog *ui_;
};

#endif  // PODCASTINFODIALOG_H
