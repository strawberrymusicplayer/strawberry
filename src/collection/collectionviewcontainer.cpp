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

#include "config.h"

#include <QWidget>
#include <QKeyEvent>

#include "collectionfilterwidget.h"
#include "collectionview.h"
#include "collectionviewcontainer.h"
#include "ui_collectionviewcontainer.h"

CollectionViewContainer::CollectionViewContainer(QWidget *parent) : QWidget(parent), ui_(new Ui_CollectionViewContainer) {

  ui_->setupUi(this);
  view()->SetFilter(filter());

  QObject::connect(filter(), &CollectionFilterWidget::UpPressed, view(), &CollectionView::UpAndFocus);
  QObject::connect(filter(), &CollectionFilterWidget::DownPressed, view(), &CollectionView::DownAndFocus);
  QObject::connect(filter(), &CollectionFilterWidget::ReturnPressed, view(), &CollectionView::FilterReturnPressed);
  QObject::connect(view(), &CollectionView::FocusOnFilterSignal, filter(), &CollectionFilterWidget::FocusOnFilter);

  ReloadSettings();

}

CollectionViewContainer::~CollectionViewContainer() { delete ui_; }
CollectionView *CollectionViewContainer::view() const { return ui_->view; }
CollectionFilterWidget *CollectionViewContainer::filter() const { return ui_->filter; }
void CollectionViewContainer::ReloadSettings() {
  filter()->ReloadSettings();
  view()->ReloadSettings();
}
