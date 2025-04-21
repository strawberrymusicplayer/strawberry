/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QString>

#include "scrobblerservice.h"
#include "scrobblersettingsservice.h"

#include "core/song.h"

ScrobblerService::ScrobblerService(const QString &name, const SharedPtr<NetworkAccessManager> network, const SharedPtr<ScrobblerSettingsService> settings, QObject *parent) : JsonBaseRequest(network, parent), name_(name), settings_(settings) {}

QString ScrobblerService::StripAlbum(const QString &album) const {

  if (settings_->strip_remastered()) {
    return Song::AlbumRemoveDiscMisc(album);
  }

  return Song::AlbumRemoveDisc(album);;

}

QString ScrobblerService::StripTitle(const QString &title) const {

  if (settings_->strip_remastered()) {
    return Song::TitleRemoveMisc(title);
  }

  return title;

}
