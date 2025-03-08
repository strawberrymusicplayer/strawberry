/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QUrl>
#include <QUrlQuery>

#include "subsonicservice.h"
#include "subsonicbaserequest.h"
#include "subsonicurlhandler.h"

using namespace Qt::Literals::StringLiterals;

SubsonicUrlHandler::SubsonicUrlHandler(SubsonicService *service) : UrlHandler(service), service_(service) {}

UrlHandler::LoadResult SubsonicUrlHandler::StartLoading(const QUrl &url) {

  if (!server_url().isValid()) {
    return LoadResult(url, LoadResult::Type::Error, tr("Subsonic server URL is invalid."));
  }

  if (username().isEmpty() || password().isEmpty()) {
    return LoadResult(url, LoadResult::Type::Error, tr("Missing Subsonic username or password."));
  }

  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;
  const QUrl stream_url = SubsonicBaseRequest::CreateUrl(server_url(), auth_method(), username(), password(), u"stream"_s, ParamList() << Param(u"id"_s, url.path()));

  return LoadResult(url, LoadResult::Type::TrackAvailable, stream_url);

}
