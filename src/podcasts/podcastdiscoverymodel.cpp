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

#include "podcastdiscoverymodel.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QIcon>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/standarditemiconloader.h"
#include "opmlcontainer.h"
#include "podcast.h"

PodcastDiscoveryModel::PodcastDiscoveryModel(Application *app, QObject *parent)
    : QStandardItemModel(parent),
      app_(app),
      icon_loader_(new StandardItemIconLoader(app->album_cover_loader(), this)),
      default_icon_(IconLoader::Load("podcast")) {

  icon_loader_->SetModel(this);

}

QVariant PodcastDiscoveryModel::data(const QModelIndex &idx, int role) const {

  if (idx.isValid() && role == Qt::DecorationRole && !QStandardItemModel::data(idx, Role_StartedLoadingImage).toBool()) {
    const QUrl image_url = QStandardItemModel::data(idx, Role_ImageUrl).toUrl();
    if (image_url.isValid()) {
      const_cast<PodcastDiscoveryModel*>(this)->LazyLoadImage(image_url, idx);
    }
  }

  return QStandardItemModel::data(idx, role);

}

QStandardItem *PodcastDiscoveryModel::CreatePodcastItem(const Podcast &podcast) {

  QStandardItem *item = new QStandardItem;
  item->setIcon(default_icon_);
  item->setText(podcast.title());
  item->setData(QVariant::fromValue(podcast), Role_Podcast);
  item->setData(Type_Podcast, Role_Type);
  item->setData(podcast.ImageUrlSmall(), Role_ImageUrl);
  return item;

}

QStandardItem *PodcastDiscoveryModel::CreateFolder(const QString &name) {

  if (folder_icon_.isNull()) {
    folder_icon_ = IconLoader::Load("folder");
  }

  QStandardItem *item = new QStandardItem;
  item->setIcon(folder_icon_);
  item->setText(name);
  item->setData(Type_Folder, Role_Type);
  return item;

}

QStandardItem *PodcastDiscoveryModel::CreateOpmlContainerItem(const OpmlContainer &container) {

  QStandardItem *item = CreateFolder(container.name);
  CreateOpmlContainerItems(container, item);
  return item;

}

void PodcastDiscoveryModel::CreateOpmlContainerItems(const OpmlContainer &container, QStandardItem *parent) {

  for (const OpmlContainer &child : container.containers) {
    QStandardItem *child_item = CreateOpmlContainerItem(child);
    parent->appendRow(child_item);
  }

  for (const Podcast &child : container.feeds) {
    QStandardItem *child_item = CreatePodcastItem(child);
    parent->appendRow(child_item);
  }

}

void PodcastDiscoveryModel::LazyLoadImage(const QUrl &url, const QModelIndex &idx) {

  QStandardItem *item = itemFromIndex(idx);
  item->setData(true, Role_StartedLoadingImage);
  icon_loader_->LoadIcon(url, QUrl(), item);

}

QStandardItem *PodcastDiscoveryModel::CreateLoadingIndicator() {

  QStandardItem *item = new QStandardItem;
  item->setText(tr("Loading..."));
  item->setData(Type_LoadingIndicator, Role_Type);
  return item;

}
