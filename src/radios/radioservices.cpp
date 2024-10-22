/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QObject>
#include <QSortFilterProxyModel>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "radioservices.h"
#include "radiobackend.h"
#include "radiomodel.h"
#include "radioservice.h"
#include "radiochannel.h"
#include "somafmservice.h"
#include "radioparadiseservice.h"

using std::make_shared;

RadioServices::RadioServices(const SharedPtr<TaskManager> task_manager,
                             const SharedPtr<NetworkAccessManager> network,
                             const SharedPtr<Database> database,
                             const SharedPtr<AlbumCoverLoader> albumcover_loader,
                             QObject *parent)
    : QObject(parent),
      network_(network),
      backend_(nullptr),
      model_(new RadioModel(albumcover_loader, SharedPtr<RadioServices>(this))),
      sort_model_(new QSortFilterProxyModel(this)),
      channels_refresh_(false) {

  backend_ = make_shared<RadioBackend>(database);
  backend_->moveToThread(database->thread());

  QObject::connect(&*backend_, &RadioBackend::NewChannels, this, &RadioServices::GotChannelsFromBackend);

  sort_model_->setSourceModel(model_);
  sort_model_->setSortRole(RadioModel::Role_SortText);
  sort_model_->setDynamicSortFilter(true);
  sort_model_->setSortLocaleAware(true);
  sort_model_->sort(0);

  AddService(new SomaFMService(task_manager, network_, this));
  AddService(new RadioParadiseService(task_manager, network_, this));

}

void RadioServices::AddService(RadioService *service) {

  qLog(Debug) << "Adding radio service:" << service->name();
  services_.insert(service->source(), service);

  QObject::connect(service, &RadioService::NewChannels, this, &RadioServices::GotChannelsFromService);
  QObject::connect(service, &RadioService::destroyed, this, &RadioServices::ServiceDeleted);

}

void RadioServices::RemoveService(RadioService *service) {

  if (!services_.contains(service->source())) return;

  services_.remove(service->source());
  QObject::disconnect(service, nullptr, this, nullptr);

}

void RadioServices::ServiceDeleted() {

  RadioService *service = qobject_cast<RadioService*>(sender());
  if (service) RemoveService(service);

}

RadioService *RadioServices::ServiceBySource(const Song::Source source) const {

  if (services_.contains(source)) return services_.value(source);
  return nullptr;

}

void RadioServices::ReloadSettings() {

  const QList<RadioService*> services = services_.values();
  for (RadioService *service : services) {
    service->ReloadSettings();
  }

}

void RadioServices::GetChannels() {

  model_->Reset();
  backend_->GetChannelsAsync();

}

void RadioServices::RefreshChannels() {

  channels_refresh_ = true;
  model_->Reset();
  backend_->DeleteChannelsAsync();

  const QList<RadioService*> services = services_.values();
  for (RadioService *service : services) {
    service->GetChannels();
  }

}

void RadioServices::GotChannelsFromBackend(const RadioChannelList &channels) {

  if (channels.isEmpty()) {
    if (!channels_refresh_) {
      RefreshChannels();
    }
  }
  else {
    model_->AddChannels(channels);
  }

}

void RadioServices::GotChannelsFromService(const RadioChannelList &channels) {

  RadioService *service = qobject_cast<RadioService*>(sender());
  if (!service) return;

  backend_->AddChannelsAsync(channels);

}
