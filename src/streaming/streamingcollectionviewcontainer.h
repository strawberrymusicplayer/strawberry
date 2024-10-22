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

#ifndef STREAMINGCOLLECTIONVIEWCONTAINER_H
#define STREAMINGCOLLECTIONVIEWCONTAINER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "streamingcollectionview.h"

class QStackedWidget;
class QPushButton;
class QLabel;
class QProgressBar;
class QContextMenuEvent;
class CollectionFilterWidget;

#include "ui_streamingcollectionviewcontainer.h"

class StreamingCollectionViewContainer : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingCollectionViewContainer(QWidget *parent = nullptr);
  ~StreamingCollectionViewContainer() override;

  void ReloadSettings() const;
  bool SearchFieldHasFocus() const;
  void FocusSearchField();

  QStackedWidget *stacked() const { return ui_->stacked; }
  QWidget *help_page() const { return ui_->help_page; }
  QWidget *streamingcollection_page() const { return ui_->streamingcollection_page; }
  StreamingCollectionView *view() const { return ui_->view; }
  CollectionFilterWidget *filter_widget() const { return ui_->filter_widget; }
  QPushButton *button_refresh() const { return ui_->refresh; }
  QPushButton *button_close() const { return ui_->close; }
  QPushButton *button_abort() const { return ui_->abort; }
  QLabel *status() const { return ui_->status; }
  QProgressBar *progressbar() const { return ui_->progressbar; }

 private Q_SLOTS:
  void contextMenuEvent(QContextMenuEvent *e) override;

 private:
  Ui_StreamingCollectionViewContainer *ui_;

};

#endif  // STREAMINGCOLLECTIONVIEWCONTAINER_H
