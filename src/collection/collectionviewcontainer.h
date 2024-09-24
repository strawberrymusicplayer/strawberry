/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONVIEWCONTAINER_H
#define COLLECTIONVIEWCONTAINER_H

#include "config.h"

#include <QWidget>

class CollectionFilterWidget;
class CollectionView;
class Ui_CollectionViewContainer;

class CollectionViewContainer : public QWidget {
  Q_OBJECT

 public:
  explicit CollectionViewContainer(QWidget *parent = nullptr);
  ~CollectionViewContainer() override;

  CollectionFilterWidget *filter_widget() const;
  CollectionView *view() const;

  void ReloadSettings() const;

 private:
  Ui_CollectionViewContainer *ui_;
};

#endif  // COLLECTIONVIEWCONTAINER_H
