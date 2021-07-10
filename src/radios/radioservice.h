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

#ifndef RADIOSERVICE_H
#define RADIOSERVICE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QJsonObject>

#include "core/song.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"
#include "widgets/multiloadingindicator.h"
#include "radiochannel.h"

class QNetworkReply;
class QStandardItem;

class Application;
class NetworkAccessManager;
class RadioModel;

class RadioService : public QObject {
  Q_OBJECT

 public:
  explicit RadioService(const Song::Source source, const QString &name, const QIcon &icon, Application *app, NetworkAccessManager *network, QObject *parent = nullptr);

  Song::Source source() const { return source_; }
  QString name() const { return name_; }

  virtual void ReloadSettings() {}

  virtual QUrl Homepage() = 0;
  virtual QUrl Donate() = 0;

 signals:
  void StreamError(const QString& message);
  void StreamMetadataFound(const QUrl& original_url, const Song& song);

  void NewChannels(RadioChannelList channels = RadioChannelList());

 public slots:
  virtual void ShowConfig() {}
  virtual void GetChannels() = 0;

 private slots:

 protected:
  // Called once when context menu is created
  virtual void PopulateContextMenu(){};
  // Called every time context menu is shown
  virtual void UpdateContextMenu(){};

  // Returns all the playlist insertion related QActions (see below).
  QList<QAction*> GetPlaylistActions();

  // Returns the 'append to playlist' QAction.
  QAction *GetAppendToPlaylistAction();
  // Returns the 'replace playlist' QAction.
  QAction *GetReplacePlaylistAction();
  // Returns the 'open in new playlist' QAction.
  QAction *GetOpenInNewPlaylistAction();

  QByteArray ExtractData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  QJsonObject ExtractJsonObj(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant());

  // Describes how songs should be added to playlist.
  enum AddMode {
    // appends songs to the current playlist
    AddMode_Append,
    // clears the current playlist and then appends all songs to it
    AddMode_Replace,
    // creates a new, empty playlist and then adds all songs to it
    AddMode_OpenInNew
  };

  // Adds the 'index' element to playlist using the 'add_mode' mode.
  void AddItemToPlaylist(const QModelIndex& index, AddMode add_mode);
  // Adds the 'indexes' elements to playlist using the 'add_mode' mode.
  void AddItemsToPlaylist(const QModelIndexList& indexes, AddMode add_mode);

 protected:
  Application *app_;
  NetworkAccessManager *network_;
  RadioModel *model_;
  Song::Source source_;
  QString name_;
  QIcon icon_;

};

Q_DECLARE_METATYPE(RadioService*)

#endif  // RADIOSERVICE_H
