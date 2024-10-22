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

#ifndef RADIOSERVICES_H
#define RADIOSERVICES_H

#include <QObject>
#include <QMap>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "radiochannel.h"

class QSortFilterProxyModel;

class TaskManager;
class Database;
class NetworkAccessManager;
class AlbumCoverLoader;
class RadioBackend;
class RadioModel;
class RadioService;

class RadioServices : public QObject {
  Q_OBJECT

 public:
  explicit RadioServices(const SharedPtr<TaskManager> task_manager,
                         const SharedPtr<NetworkAccessManager> network,
                         const SharedPtr<Database> database,
                         const SharedPtr<AlbumCoverLoader> albumcover_loader,
                         QObject *parent = nullptr);

  void AddService(RadioService *service);
  void RemoveService(RadioService *service);

  RadioService *ServiceBySource(const Song::Source source) const;

  template<typename T>
  T *Service() {
    return static_cast<T*>(ServiceBySource(T::source));
  }

  void ReloadSettings();

  SharedPtr<RadioBackend> radio_backend() const { return backend_; }
  QSortFilterProxyModel *sort_model() const { return sort_model_; }

 private Q_SLOTS:
  void ServiceDeleted();
  void GotChannelsFromBackend(const RadioChannelList &channels);
  void GotChannelsFromService(const RadioChannelList &channels);

 public Q_SLOTS:
  void GetChannels();
  void RefreshChannels();

 private:
  const SharedPtr<NetworkAccessManager> network_;
  SharedPtr<RadioBackend> backend_;
  RadioModel *model_;
  QSortFilterProxyModel *sort_model_;
  QMap<Song::Source, RadioService*> services_;
  bool channels_refresh_;
};

#endif  // RADIOSERVICES_H
