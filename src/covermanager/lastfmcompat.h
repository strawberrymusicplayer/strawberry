/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef LASTFMCOMPAT_H
#define LASTFMCOMPAT_H

#include "config.h"

#ifdef HAVE_LIBLASTFM1
#include <lastfm5/misc.h>
#include <lastfm5/User.h>
#include <lastfm5/ws.h>
#include <lastfm5/XmlQuery.h>
#else
#include <lastfm5/misc.h>
#include <lastfm5/User>
#include <lastfm5/ws.h>
#include <lastfm5/XmlQuery>
#endif

namespace lastfm {
namespace compat {

lastfm::XmlQuery EmptyXmlQuery();
bool ParseQuery(const QByteArray &data, lastfm::XmlQuery *query, bool *connection_problems = nullptr);
bool ParseUserList(QNetworkReply *reply, QList<lastfm::User> *users);

#ifdef HAVE_LIBLASTFM1
typedef lastfm::User AuthenticatedUser;
#else
typedef lastfm::AuthenticatedUser AuthenticatedUser;
#endif
}
}

#endif  // LASTFMCOMPAT_H

