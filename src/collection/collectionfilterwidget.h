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

#ifndef COLLECTIONFILTERWIDGET_H
#define COLLECTIONFILTERWIDGET_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QWidget>
#include <QObject>
#include <QString>
#include <QMenu>
#include <QSignalMapper>
#include <QTimer>
#include <QAction>
#include <QActionGroup>
#include <QtEvents>

#include "collectionquery.h"
#include "collectionmodel.h"

class QKeyEvent;

class GroupByDialog;
class SavedGroupingManager;
class SettingsDialog;
class Ui_CollectionFilterWidget;
struct QueryOptions;

class CollectionFilterWidget : public QWidget {
  Q_OBJECT

 public:
  CollectionFilterWidget(QWidget *parent = nullptr);
  ~CollectionFilterWidget();

  static const int kFilterDelay = 500;  // msec

  enum DelayBehaviour {
    AlwaysInstant,
    DelayedOnLargeLibraries,
    AlwaysDelayed,
  };

  static QActionGroup *CreateGroupByActions(QObject *parent);

  void UpdateGroupByActions();
  void SetFilterHint(const QString &hint);
  void SetApplyFilterToCollection(bool filter_applies_to_model) { filter_applies_to_model_ = filter_applies_to_model; }
  void SetDelayBehaviour(DelayBehaviour behaviour) { delay_behaviour_ = behaviour; }
  void SetAgeFilterEnabled(bool enabled);
  void SetGroupByEnabled(bool enabled);
  void ShowInCollection(const QString &search);

  QMenu *menu() const { return collection_menu_; }
  void AddMenuAction(QAction *action);

  void SetSettingsGroup(const QString &group) { settings_group_ = group; }
  void SetSettingsPrefix(const QString &prefix) { settings_prefix_ = prefix; }
  void SetCollectionModel(CollectionModel *model);

  QString group_by();
  QString group_by(const int number);

 public slots:
  void SetQueryMode(QueryOptions::QueryMode view);
  void FocusOnFilter(QKeyEvent *e);

signals:
  void UpPressed();
  void DownPressed();
  void ReturnPressed();
  void Filter(const QString &text);

 protected:
  void keyReleaseEvent(QKeyEvent *e);

 private slots:
  void GroupingChanged(const CollectionModel::Grouping &g);
  void GroupByClicked(QAction *action);
  void SaveGroupBy();
  void ShowGroupingManager();

  void FilterTextChanged(const QString &text);
  void FilterDelayTimeout();

 private:
  static QAction *CreateGroupByAction(const QString &text, QObject *parent, const CollectionModel::Grouping &grouping);
  void CheckCurrentGrouping(const CollectionModel::Grouping &g);

 private:
  Ui_CollectionFilterWidget *ui_;
  CollectionModel *model_;

  std::unique_ptr<GroupByDialog> group_by_dialog_;
  std::unique_ptr<SavedGroupingManager> groupings_manager_;
  SettingsDialog *settings_dialog_;

  QMenu *filter_age_menu_;
  QMenu *group_by_menu_;
  QMenu *collection_menu_;
  QActionGroup *group_by_group_;
  QSignalMapper *filter_age_mapper_;

  QTimer *filter_delay_;

  bool filter_applies_to_model_;
  DelayBehaviour delay_behaviour_;

  QString settings_group_;
  QString settings_prefix_;
};

#endif  // COLLECTIONFILTERWIDGET_H

