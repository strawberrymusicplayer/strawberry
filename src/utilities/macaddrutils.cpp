/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QString>
#include <QNetworkInterface>

#include "macaddrutils.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

QString MacAddress() {

  QString ret;

  for (QNetworkInterface &netif : QNetworkInterface::allInterfaces()) {
    if (
        (netif.hardwareAddress() == "00:00:00:00:00:00"_L1) ||
        (netif.flags() & QNetworkInterface::IsLoopBack) ||
        !(netif.flags() & QNetworkInterface::IsUp) ||
        !(netif.flags() & QNetworkInterface::IsRunning)
        ) { continue; }
    if (ret.isEmpty() || netif.type() == QNetworkInterface::Ethernet || netif.type() == QNetworkInterface::Wifi) {
      ret = netif.hardwareAddress();
    }
  }

  if (ret.isEmpty()) ret = "00:00:00:00:00:00"_L1;

  return ret;

}

}  // namespace Utilities
