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

#include <QMessageBox>
#include <QPushButton>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/networkaccessmanager.h"
#include "podcast.h"
#include "podcastdiscoverymodel.h"

#include "gpoddersearchpage.h"
#include "ui_gpoddersearchpage.h"

GPodderSearchPage::GPodderSearchPage(Application *app, QWidget *parent)
    : AddPodcastPage(app, parent),
      ui_(new Ui_GPodderSearchPage),
      network_(new NetworkAccessManager(this)),
      api_(new mygpo::ApiRequest(network_)) {

  ui_->setupUi(this);
  QObject::connect(ui_->search, &QPushButton::clicked, this, &GPodderSearchPage::SearchClicked);
  setWindowIcon(IconLoader::Load("mygpo"));

}

GPodderSearchPage::~GPodderSearchPage() {

  delete ui_;
  delete api_;

}

void GPodderSearchPage::SearchClicked() {

  emit Busy(true);

  mygpo::PodcastListPtr list(api_->search(ui_->query->text()));
  QObject::connect(list.data(), &mygpo::PodcastList::finished, this, [this, list]() { SearchFinished(list); });
  QObject::connect(list.data(), &mygpo::PodcastList::parseError, this, [this, list]() { SearchFailed(list); });
  QObject::connect(list.data(), &mygpo::PodcastList::requestError, this, [this, list]() { SearchFailed(list); });

}

void GPodderSearchPage::SearchFinished(mygpo::PodcastListPtr list) {

  emit Busy(false);

  model()->clear();

  for (mygpo::PodcastPtr gpo_podcast : list->list()) {
    Podcast podcast;
    podcast.InitFromGpo(gpo_podcast.data());

    model()->appendRow(model()->CreatePodcastItem(podcast));
  }

}

void GPodderSearchPage::SearchFailed(mygpo::PodcastListPtr list) {

  emit Busy(false);

  model()->clear();

  if (QMessageBox::warning(
        nullptr, tr("Failed to fetch podcasts"),
        tr("There was a problem communicating with gpodder.net"),
        QMessageBox::Retry | QMessageBox::Close,
        QMessageBox::Retry) != QMessageBox::Retry) {
    return;
  }

  // Try doing the search again.
  SearchClicked();

}

void GPodderSearchPage::Show() { ui_->query->setFocus(); }
