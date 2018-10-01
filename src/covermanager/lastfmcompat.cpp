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

#include "config.h"

#include <lastfm5/ws.h>

#include <QByteArray>
#include <QList>
#include <QNetworkReply>

#include "lastfmcompat.h"

namespace lastfm {
namespace compat {

#ifdef HAVE_LIBLASTFM1

XmlQuery EmptyXmlQuery() { return XmlQuery(); }

bool ParseQuery(const QByteArray &data, XmlQuery *query, bool *connection_problems) {

  const bool ret = query->parse(data);

  if (connection_problems) {
    *connection_problems = !ret && query->parseError().enumValue() == lastfm::ws::MalformedResponse;
  }

  return ret;
}

bool ParseUserList(QNetworkReply *reply, QList<User> *users) {

  lastfm::XmlQuery lfm;
  if (!lfm.parse(reply->readAll())) {
    return false;
  }

  *users = lastfm::UserList(lfm).users();
  return true;

}

#else  // HAVE_LIBLASTFM1

XmlQuery EmptyXmlQuery() {

  QByteArray dummy;
  return XmlQuery(dummy);

}

bool ParseQuery(const QByteArray &data, XmlQuery *query, bool *connection_problems) {

  try {
    *query = lastfm::XmlQuery(data);
#ifdef Q_OS_WIN32
    if (lastfm::ws::last_parse_error != lastfm::ws::NoError) {
      return false;
    }
#endif  // Q_OS_WIN32
  }
  catch (lastfm::ws::ParseError e) {
    qLog(Error) << "Last.fm parse error: " << e.enumValue();
    if (connection_problems) {
      *connection_problems = e.enumValue() == lastfm::ws::MalformedResponse;
    }
    return false;
  }
  catch (std::runtime_error &e) {
    qLog(Error) << __FUNCTION__ << e.what();
    return false;
  }

  if (connection_problems) {
    *connection_problems = false;
  }

  // Check for app errors.
  if (QDomElement(*query).attribute("status") == "failed") {
    return false;
  }

  return true;

}

bool ParseUserList(QNetworkReply *reply, QList<User> *users) {

  try {
    *users = lastfm::User::list(reply);
#ifdef Q_OS_WIN32
    if (lastfm::ws::last_parse_error != lastfm::ws::NoError) {
      return false;
    }
#endif  // Q_OS_WIN32
  }
  catch (std::runtime_error &e) {
    qLog(Error) << __FUNCTION__ << e.what();
    return false;
  }
  return true;

}

#endif  // HAVE_LIBLASTFM1
}
}

