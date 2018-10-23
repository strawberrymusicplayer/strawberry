/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETSERVICES_H
#define INTERNETSERVICES_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QStandardItemModel>
#include <QMap>
#include <QString>

#include "core/song.h"
#include "collection/collectionmodel.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"
#include "widgets/multiloadingindicator.h"

class Application;
class InternetService;

class InternetServices : public QObject {
  Q_OBJECT

 public:
  explicit InternetServices(QObject *parent = nullptr);
  ~InternetServices();

  enum Role {
    // Services can use this role to distinguish between different types of items that they add.
    // The root item's type is automatically set to Type_Service,
    // but apart from that Services are free to define their own values for this field (starting from TypeCount).
    Role_Type = Qt::UserRole + 1000,

    // If this is not set the item is not playable (ie. it can't be dragged to the playlist).
    // Otherwise it describes how this item is converted to playlist items.
    // See the PlayBehaviour enum for more details.
    Role_PlayBehaviour,

    // The URL of the media for this item.  This is required if the PlayBehaviour is set to PlayBehaviour_UseSongLoader.
    Role_Url,

    // The metadata used in the item that is added to the playlist if the PlayBehaviour is set to PlayBehaviour_SingleItem.  Ignored otherwise.
    Role_SongMetadata,

    // If this is set to true then the model will call the service's LazyPopulate method when this item is expanded.
    // Use this if your item's children have to be downloaded or fetched remotely.
    Role_CanLazyLoad,

    // This is automatically set on the root item for a service.  It contains a pointer to an InternetService.
    // Services should not set this field themselves.
    Role_Service,

    // Setting this to true means that the item can be changed by user action (e.g. changing remote playlists)
    Role_CanBeModified,
    RoleCount,
    Role_IsDivider = CollectionModel::Role_IsDivider,
  };

  enum Type {
    Type_Service = 1,
    Type_Track,
    Type_UserPlaylist,
    TypeCount
  };

  enum PlayBehaviour {
    // The item can't be played.  This is the default.
    PlayBehaviour_None = 0,

    // This item's URL is passed through the normal song loader.
    // This supports loading remote playlists, remote files and local files.
    // This is probably the most sensible behaviour to use if you're just returning normal radio stations.
    PlayBehaviour_UseSongLoader,

    // This item's URL, Title and Artist are used in the playlist.  No special behaviour occurs
    // The URL is just passed straight to gstreamer when the user starts playing.
    PlayBehaviour_SingleItem,

    // This item's children have PlayBehaviour_SingleItem set.
    // This is used when dragging a playlist item for instance, to have all the playlit's items info loaded in the mime data.
    PlayBehaviour_MultipleItems,

    // This item might not represent a song - the service's ItemDoubleClicked() slot will get called instead to do some custom action.
    PlayBehaviour_DoubleClickAction,
  };

  InternetService *ServiceBySource(const Song::Source &source);
  template <typename T>
  T *Service() {
    return static_cast<T*>(this->ServiceBySource(T::kSource));
  }

  // Add and remove services.  Ownership is not transferred and the service is not reparented.
  // If the service is deleted it will be automatically removed from the model.
  void AddService(InternetService *service);
  void RemoveService(InternetService *service);
  void ReloadSettings();

 private slots:
  void ServiceDeleted();

 private:
  QMap<Song::Source, InternetService*> services_;

};

#endif
