/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
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

#ifndef ITUNESSEARCHPAGE_H
#define ITUNESSEARCHPAGE_H

#include "addpodcastpage.h"

class Ui_ITunesSearchPage;

class QNetworkReply;

class NetworkAccessManager;

class ITunesSearchPage : public AddPodcastPage {
  Q_OBJECT

 public:
  ITunesSearchPage(Application *app, QWidget *parent);
  ~ITunesSearchPage();

  void Show();

 private slots:
  void SearchClicked();
  void SearchFinished(QNetworkReply *reply);

 private:
  static const char *kUrlBase;

  Ui_ITunesSearchPage *ui_;

  NetworkAccessManager *network_;
};

#endif  // ITUNESSEARCHPAGE_H
