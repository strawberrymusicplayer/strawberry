/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef ARTISTBIOPROVIDER_H
#define ARTISTBIOPROVIDER_H

#include <QObject>
#include <QUrl>

#include "widgets/collapsibleinfopane.h"
#include "core/song.h"

class ArtistBioProvider : public QObject {
  Q_OBJECT

 public:
  explicit ArtistBioProvider();

  virtual void Start(const int id, const Song &song) = 0;
  virtual void Cancel(const int) {}

  virtual QString name() const;

  bool is_enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

 signals:
  void ImageReady(int, QUrl);
  void InfoReady(int, CollapsibleInfoPane::Data);
  void Finished(int);

 private:
  bool enabled_;
};

#endif  // ARTISTBIOPROVIDER_H
