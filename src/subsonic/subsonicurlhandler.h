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

#ifndef SUBSONICURLHANDLER_H
#define SUBSONICURLHANDLER_H

#include "config.h"

#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "subsonic/subsonicservice.h"
#include "constants/subsonicsettings.h"

class SubsonicUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit SubsonicUrlHandler(SubsonicService *service);

  QString scheme() const override { return service_->url_scheme(); }
  QUrl server_url() const { return service_->server_url(); }
  QString username() const { return service_->username(); }
  QString password() const { return service_->password(); }
  SubsonicSettings::AuthMethod auth_method() const { return service_->auth_method(); }

  LoadResult StartLoading(const QUrl &url) override;

 private:
  SubsonicService *service_;
};

#endif  // SUBSONICURLHANDLER_H
