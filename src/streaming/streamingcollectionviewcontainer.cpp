/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QKeyEvent>
#include <QContextMenuEvent>

#include "core/iconloader.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "collection/collectionfilterwidget.h"
#include "streamingcollectionview.h"
#include "streamingcollectionviewcontainer.h"
#include "ui_streamingcollectionviewcontainer.h"

using namespace Qt::Literals::StringLiterals;

StreamingCollectionViewContainer::StreamingCollectionViewContainer(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_StreamingCollectionViewContainer),
      refresh_(new QToolButton(this)) {

  ui_->setupUi(this);

  refresh_->setIcon(IconLoader::Load(u"view-refresh"_s));
  refresh_->setToolTip(tr("Refresh catalogue"));
  ui_->filter_widget->AddButton(refresh_);
  ui_->view->SetFilterWidget(ui_->filter_widget);

  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::UpPressed, ui_->view, &StreamingCollectionView::UpAndFocus);
  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::DownPressed, ui_->view, &StreamingCollectionView::DownAndFocus);
  QObject::connect(ui_->filter_widget, &CollectionFilterWidget::ReturnPressed, ui_->view, &StreamingCollectionView::FilterReturnPressed);
  QObject::connect(ui_->view, &StreamingCollectionView::FocusOnFilterSignal, ui_->filter_widget, &CollectionFilterWidget::FocusOnFilter);

  ui_->progressbar->hide();

  ReloadSettings();

}

StreamingCollectionViewContainer::~StreamingCollectionViewContainer() { delete ui_; }

void StreamingCollectionViewContainer::Init(CollectionModel *collection_model, CollectionFilter *collection_filter, const QString &settings_group, const QString &settings_prefix, const bool favorite) {

  ui_->stacked->setCurrentWidget(ui_->streamingcollection_page);
  ui_->view->Init(collection_model, favorite);
  ui_->view->setModel(collection_filter);
  ui_->filter_widget->SetSettingsGroup(settings_group);
  if (!settings_prefix.isEmpty()) {
    ui_->filter_widget->SetSettingsPrefix(settings_prefix);
  }
  ui_->filter_widget->Init(collection_model, collection_filter);

  QObject::connect(collection_model, &CollectionModel::TotalSongCountUpdated, ui_->view, &StreamingCollectionView::TotalSongCountUpdated);
  QObject::connect(collection_model, &CollectionModel::modelAboutToBeReset, ui_->view, &StreamingCollectionView::SaveFocus);
  QObject::connect(collection_model, &CollectionModel::modelReset, ui_->view, &StreamingCollectionView::RestoreFocus);

}

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

void StreamingCollectionViewContainer::ShowProgress() {

  ui_->status->clear();
  ui_->progressbar->show();
  ui_->abort->show();
  ui_->close->hide();
  ui_->stacked->setCurrentWidget(ui_->help_page);

}

void StreamingCollectionViewContainer::ShowCollection() {

  ui_->progressbar->setValue(0);
  ui_->status->clear();
  ui_->stacked->setCurrentWidget(ui_->streamingcollection_page);

}

void StreamingCollectionViewContainer::ShowError(const QString &error) {

  ui_->status->setText(error);
  ui_->progressbar->setValue(0);
  ui_->progressbar->hide();
  ui_->abort->hide();
  ui_->close->show();

}

void StreamingCollectionViewContainer::contextMenuEvent(QContextMenuEvent *e) { Q_UNUSED(e); }
