/*
 * Strawberry Music Player
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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QProgressBar>
#include <QKeyEvent>
#include <QContextMenuEvent>

#include "collection/collectionfilterwidget.h"
#include "internetcollectionview.h"
#include "internetcollectionviewcontainer.h"
#include "ui_internetcollectionviewcontainer.h"

InternetCollectionViewContainer::InternetCollectionViewContainer(QWidget *parent) :
  QWidget(parent),
  ui_(new Ui_InternetCollectionViewContainer) {

  ui_->setupUi(this);
  view()->SetFilter(filter_widget());

  QObject::connect(filter_widget(), &CollectionFilterWidget::UpPressed, view(), &InternetCollectionView::UpAndFocus);
  QObject::connect(filter_widget(), &CollectionFilterWidget::DownPressed, view(), &InternetCollectionView::DownAndFocus);
  QObject::connect(filter_widget(), &CollectionFilterWidget::ReturnPressed, view(), &InternetCollectionView::FilterReturnPressed);
  QObject::connect(view(), &InternetCollectionView::FocusOnFilterSignal, filter_widget(), &CollectionFilterWidget::FocusOnFilter);

  ui_->progressbar->hide();

  ReloadSettings();

}

InternetCollectionViewContainer::~InternetCollectionViewContainer() { delete ui_; }

void InternetCollectionViewContainer::ReloadSettings() const {
  filter_widget()->ReloadSettings();
  view()->ReloadSettings();
}

void InternetCollectionViewContainer::contextMenuEvent(QContextMenuEvent *e) { Q_UNUSED(e); }
