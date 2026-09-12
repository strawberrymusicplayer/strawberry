/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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

#ifndef PLEXURLHANDLER_H
#define PLEXURLHANDLER_H

#include "config.h"

#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "plex/plexservice.h"

class PlexUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit PlexUrlHandler(PlexService *service);

  QString scheme() const override { return service_->url_scheme(); }
  QUrl server_url() const { return service_->server_url(); }
  QString token() const { return service_->server_token(); }

  LoadResult StartLoading(const QUrl &url) override;

 private:
  PlexService *service_;
};

#endif  // PLEXURLHANDLER_H
