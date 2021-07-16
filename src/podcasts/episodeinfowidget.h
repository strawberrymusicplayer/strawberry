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

#ifndef EPISODEINFOWIDGET_H
#define EPISODEINFOWIDGET_H

#include <QWidget>

#include "podcastepisode.h"

class Application;
class Ui_EpisodeInfoWidget;

class EpisodeInfoWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EpisodeInfoWidget(QWidget *parent = nullptr);
  ~EpisodeInfoWidget();

  void SetApplication(Application *app);

  void SetEpisode(const PodcastEpisode &episode);

 private:
  Ui_EpisodeInfoWidget *ui_;

  Application *app_;
  PodcastEpisode episode_;
};

#endif  // EPISODEINFOWIDGET_H
