/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>

#include "core/logging.h"

#include "urlhandlers.h"
#include "urlhandler.h"

UrlHandlers::UrlHandlers(QObject *parent) : QObject(parent) {}

void UrlHandlers::Register(UrlHandler *url_handler) {

  const QString scheme = url_handler->scheme();

  if (url_handlers_.contains(scheme)) {
    qLog(Warning) << "Tried to register a URL handler for" << scheme << "but one was already registered";
    return;
  }

  qLog(Info) << "Registered URL handler for" << scheme;
  url_handlers_.insert(scheme, url_handler);
  QObject::connect(url_handler, &UrlHandler::destroyed, this, &UrlHandlers::Destroyed);

  Q_EMIT Registered(url_handler);

}

void UrlHandlers::Unregister(UrlHandler *url_handler) {

  const QString scheme = url_handlers_.key(url_handler);
  if (scheme.isEmpty()) {
    qLog(Warning) << "Tried to unregister a URL handler for" << url_handler->scheme() << "that wasn't registered";
    return;
  }

  qLog(Info) << "Unregistered URL handler for" << scheme;
  url_handlers_.remove(scheme);
  QObject::disconnect(url_handler, &UrlHandler::destroyed, this, &UrlHandlers::Destroyed);
  QObject::disconnect(url_handler, &UrlHandler::AsyncLoadComplete, nullptr, nullptr);

}

void UrlHandlers::Destroyed(QObject *object) {

  UrlHandler *handler = static_cast<UrlHandler*>(object);
  const QString scheme = url_handlers_.key(handler);
  if (!scheme.isEmpty()) {
    url_handlers_.remove(scheme);
  }

}

bool UrlHandlers::CanHandle(const QString &scheme) const {

  return url_handlers_.contains(scheme);

}

bool UrlHandlers::CanHandle(const QUrl &url) const {

  return url_handlers_.contains(url.scheme());

}

UrlHandler *UrlHandlers::GetUrlHandler(const QString &scheme) const {

  if (!CanHandle(scheme)) return nullptr;

  return url_handlers_.value(scheme);

}

UrlHandler *UrlHandlers::GetUrlHandler(const QUrl &url) const {

  return GetUrlHandler(url.scheme());

}
