/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#ifndef JELLYFINURLHANDLER_H
#define JELLYFINURLHANDLER_H

#include "config.h"

#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "core/song.h"
#include "jellyfin/jellyfinservice.h"

class JellyfinUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit JellyfinUrlHandler(JellyfinService *service);

  QString scheme() const override { return service_->url_scheme(); }
  LoadResult StartLoading(const QUrl &url) override;

 private:
  JellyfinService *service_;
};

#endif  // JELLYFINURLHANDLER_H