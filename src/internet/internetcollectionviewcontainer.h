/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETCOLLECTIONVIEWCONTAINER_H
#define INTERNETCOLLECTIONVIEWCONTAINER_H

#include "config.h"

#include <QWidget>

#include "ui_internetcollectionviewcontainer.h"

class QStackedWidget;
class QPushButton;
class QLabel;
class QProgressBar;
class Application;
class InternetCollectionView;
class CollectionFilterWidget;
class InternetService;
class Ui_InternetCollectionViewContainer;

class InternetCollectionViewContainer : public QWidget {
  Q_OBJECT

 public:
  InternetCollectionViewContainer(QWidget *parent = nullptr);
  ~InternetCollectionViewContainer();

  QStackedWidget *stacked() const { return ui_->stacked; }
  QWidget *help_page() const { return ui_->help_page; }
  QWidget *internetcollection_page() const { return ui_->internetcollection_page; }
  InternetCollectionView *view() const { return ui_->view; }
  CollectionFilterWidget *filter() const { return ui_->filter; }
  QPushButton *button_refresh() const { return ui_->refresh; }
  QPushButton *button_close() const { return ui_->close; }
  QPushButton *button_abort() const { return ui_->abort; }
  QLabel *status() const { return ui_->status; }
  QProgressBar *progressbar() const { return ui_->progressbar; }
  void ReloadSettings() { view()->ReloadSettings(); }

 private slots:
  void contextMenuEvent(QContextMenuEvent *e);

 private:
  Ui_InternetCollectionViewContainer *ui_;
  Application *app_;
  InternetService *service_;

};

#endif  // INTERNETCOLLECTIONVIEWCONTAINER_H
