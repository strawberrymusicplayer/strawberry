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
#include "streamingcollectionview.h"
#include "streamingcollectionviewcontainer.h"
#include "ui_streamingcollectionviewcontainer.h"

StreamingCollectionViewContainer::StreamingCollectionViewContainer(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_StreamingCollectionViewContainer) {

  ui_->setupUi(this);
  ui_->view->SetFilter(ui_->filter_widget);

  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::UpPressed, ui_->view, &StreamingCollectionView::UpAndFocus);
  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::DownPressed, ui_->view, &StreamingCollectionView::DownAndFocus);
  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::ReturnPressed, ui_->view, &StreamingCollectionView::FilterReturnPressed);
  QObject::connect(ui_->view, &StreamingCollectionView::FocusOnFilterSignal, ui_->filter_widget, &CollectionFilterWidget::FocusOnFilter);

  ui_->progressbar->hide();

  ReloadSettings();

}

StreamingCollectionViewContainer::~StreamingCollectionViewContainer() { delete ui_; }

void StreamingCollectionViewContainer::ReloadSettings() const {

  ui_->filter_widget->ReloadSettings();
  ui_->view->ReloadSettings();

}

bool StreamingCollectionViewContainer::SearchFieldHasFocus() const {
  return ui_->filter_widget->SearchFieldHasFocus();
}

void StreamingCollectionViewContainer::FocusSearchField() {
  ui_->filter_widget->FocusSearchField();
}

void StreamingCollectionViewContainer::contextMenuEvent(QContextMenuEvent *e) { Q_UNUSED(e); }
