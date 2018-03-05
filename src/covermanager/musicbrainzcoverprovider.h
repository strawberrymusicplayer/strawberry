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

#ifndef MUSICBRAINZCOVERPROVIDER_H
#define MUSICBRAINZCOVERPROVIDER_H

#include "config.h"

#include <QMultiMap>

#include "coverprovider.h"

class QNetworkAccessManager;
class QNetworkReply;

class MusicbrainzCoverProvider : public CoverProvider {
  Q_OBJECT
 public:
  explicit MusicbrainzCoverProvider(QObject *parent = nullptr);

  // CoverProvider
  virtual bool StartSearch(const QString &artist, const QString &album, int id);
  virtual void CancelSearch(int id);

 private slots:
  void ReleaseSearchFinished(QNetworkReply *reply, int id);
  void ImageCheckFinished(int id);

 private:
  QNetworkAccessManager *network_;
  QMultiMap<int, QNetworkReply *> image_checks_;
  QMap<int, QString> cover_names_;

};

#endif  // MUSICBRAINZCOVERPROVIDER_H
