/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QMap>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QIODevice>
#include <QCryptographicHash>

#include "confirmedcoverfiles.h"

ConfirmedCoverFiles::ConfirmedCoverFiles(const QStringList &filenames) {

  for (const QString &filename : filenames) {
    files_.insert(filename, ContentHash(filename));
  }

}

bool ConfirmedCoverFiles::contains(const QString &filename) const {

  return files_.contains(filename) && contains(filename, ContentHash(filename));

}

bool ConfirmedCoverFiles::contains(const QString &filename, const QByteArray &content_hash) const {

  const QMap<QString, QByteArray>::const_iterator it = files_.constFind(filename);
  return it != files_.constEnd() && !it.value().isEmpty() && it.value() == content_hash;

}

QByteArray ConfirmedCoverFiles::ContentHash(const QString &filename) {

  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) return QByteArray();

  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) return QByteArray();

  return hash.result();

}
