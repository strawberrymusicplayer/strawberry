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

#include "TagList.h"

#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/iconloader.h"

#include "gpoddertoptagsmodel.h"
#include "gpoddertoptagspage.h"

const int GPodderTopTagsPage::kMaxTagCount = 100;

GPodderTopTagsPage::GPodderTopTagsPage(Application *app, QWidget *parent)
    : AddPodcastPage(app, parent),
      network_(new NetworkAccessManager(this)),
      api_(new mygpo::ApiRequest(network_)),
      done_initial_load_(false) {

  setWindowTitle(tr("gpodder.net directory"));
  setWindowIcon(IconLoader::Load("mygpo"));

  SetModel(new GPodderTopTagsModel(api_, app, this));

}

GPodderTopTagsPage::~GPodderTopTagsPage() { delete api_; }

void GPodderTopTagsPage::Show() {

  if (!done_initial_load_) {
    // Start the request for list of top-level tags
    emit Busy(true);
    done_initial_load_ = true;

    mygpo::TagListPtr tag_list(api_->topTags(kMaxTagCount));
    QObject::connect(tag_list.get(), &mygpo::TagList::finished, this, [this, tag_list]() { TagListLoaded(tag_list); });
    QObject::connect(tag_list.get(), &mygpo::TagList::parseError, this, [this]() { TagListFailed(); });
    QObject::connect(tag_list.get(), &mygpo::TagList::requestError, this, [this]() { TagListFailed(); });
  }

}

void GPodderTopTagsPage::TagListLoaded(mygpo::TagListPtr tag_list) {

  emit Busy(false);

  for (mygpo::TagPtr tag : tag_list->list()) {
    model()->appendRow(model()->CreateFolder(tag->tag()));
  }

}

void GPodderTopTagsPage::TagListFailed() {

  emit Busy(false);
  done_initial_load_ = false;

  if (QMessageBox::warning(
        nullptr, tr("Failed to fetch directory"),
        tr("There was a problem communicating with gpodder.net"),
        QMessageBox::Retry | QMessageBox::Close,
        QMessageBox::Retry) != QMessageBox::Retry) {
    return;
  }

  // Try doing the search again.
  Show();

}
