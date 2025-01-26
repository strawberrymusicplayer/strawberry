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

#ifndef JSONCOVERPROVIDER_H
#define JSONCOVERPROVIDER_H

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "coverprovider.h"

class NetworkAccessManager;

class JsonCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit JsonCoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool batch, const bool allow_missing_album, const SharedPtr<NetworkAccessManager> network, QObject *parent);

 protected:
  QJsonObject ExtractJsonObject(const QByteArray &data);
};

#endif  // JSONCOVERPROVIDER_H
