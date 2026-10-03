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

#ifndef STREAMINGCOLLECTIONVIEWCONTAINER_H
#define STREAMINGCOLLECTIONVIEWCONTAINER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "streamingcollectionview.h"

class QPushButton;
class QToolButton;
class QLabel;
class QProgressBar;
class QContextMenuEvent;
class CollectionModel;
class CollectionFilter;
class CollectionFilterWidget;

#include "ui_streamingcollectionviewcontainer.h"

class StreamingCollectionViewContainer : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingCollectionViewContainer(QWidget *parent = nullptr);
  ~StreamingCollectionViewContainer() override;

  // Sets up the view and the filter for the collection model.
  void Init(CollectionModel *collection_model, CollectionFilter *collection_filter, const QString &settings_group, const QString &settings_prefix, const bool favorite);

  void ReloadSettings() const;
  bool SearchFieldHasFocus() const;
  void FocusSearchField();

  // Shows the progress of retrieving the collection.
  void ShowProgress();
  // Shows the collection after retrieving it, or after aborting.
  void ShowCollection();
  // Shows an error from retrieving the collection.
  void ShowError(const QString &error);

  StreamingCollectionView *view() const { return ui_->view; }
  CollectionFilterWidget *filter_widget() const { return ui_->filter_widget; }
  QToolButton *button_refresh() const { return refresh_; }
  QPushButton *button_close() const { return ui_->close; }
  QPushButton *button_abort() const { return ui_->abort; }
  QLabel *status() const { return ui_->status; }
  QProgressBar *progressbar() const { return ui_->progressbar; }

 private Q_SLOTS:
  void contextMenuEvent(QContextMenuEvent *e) override;

 private:
  Ui_StreamingCollectionViewContainer *ui_;
  QToolButton *refresh_;
};

#endif  // STREAMINGCOLLECTIONVIEWCONTAINER_H
