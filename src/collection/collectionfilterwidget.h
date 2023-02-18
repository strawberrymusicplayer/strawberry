/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QObject>
#include <QHash>
#include <QString>

#include "collectionquery.h"
#include "collectionqueryoptions.h"
#include "collectionmodel.h"

class QTimer;
class QMenu;
class QAction;
class QActionGroup;
class QKeyEvent;

class GroupByDialog;
class SavedGroupingManager;
class Ui_CollectionFilterWidget;

class CollectionFilterWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CollectionFilterWidget(QWidget *parent = nullptr);
  ~CollectionFilterWidget() override;

  static const int kFilterDelay = 500;  // msec

  enum class DelayBehaviour {
    AlwaysInstant,
    DelayedOnLargeLibraries,
    AlwaysDelayed,
  };

  void Init(CollectionModel *model);

  static QActionGroup *CreateGroupByActions(const QString &saved_groupings_settings_group, QObject *parent);

  void SetFilterHint(const QString &hint);
  void SetApplyFilterToCollection(bool filter_applies_to_model) { filter_applies_to_model_ = filter_applies_to_model; }
  void SetDelayBehaviour(DelayBehaviour behaviour) { delay_behaviour_ = behaviour; }
  void SetAgeFilterEnabled(bool enabled);
  void SetGroupByEnabled(bool enabled);
  void ShowInCollection(const QString &search);

  QMenu *menu() const { return collection_menu_; }
  void AddMenuAction(QAction *action);

  void SetSettingsGroup(const QString &group);
  void SetSettingsPrefix(const QString &prefix);

  QString group_by_version() const;
  QString group_by_key() const;
  QString group_by_key(const int number) const;
  QString separate_albums_by_grouping_key() const;

  void ReloadSettings();

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

 public slots:
  void UpdateGroupByActions();
  void SetFilterMode(CollectionFilterOptions::FilterMode filter_mode);
  void FocusOnFilter(QKeyEvent *e);

 signals:
  void UpPressed();
  void DownPressed();
  void ReturnPressed();
  void Filter(QString text);

 protected:
  void keyReleaseEvent(QKeyEvent *e) override;

 private slots:
  void GroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping);
  void GroupByClicked(QAction *action);
  void SaveGroupBy();
  void ShowGroupingManager();

  void FilterTextChanged(const QString &text);
  void FilterDelayTimeout();

 private:
  static QAction *CreateGroupByAction(const QString &text, QObject *parent, const CollectionModel::Grouping grouping);
  void CheckCurrentGrouping(const CollectionModel::Grouping g);

 private:
  Ui_CollectionFilterWidget *ui_;
  CollectionModel *model_;

  GroupByDialog *group_by_dialog_;
  SavedGroupingManager *groupings_manager_;

  QMenu *filter_age_menu_;
  QMenu *group_by_menu_;
  QMenu *collection_menu_;
  QActionGroup *group_by_group_;
  QHash<QAction*, int> filter_ages_;

  QTimer *filter_delay_;

  bool filter_applies_to_model_;
  DelayBehaviour delay_behaviour_;

  QString settings_group_;
  QString saved_groupings_settings_group_;
  QString settings_prefix_;

};

#endif  // COLLECTIONFILTERWIDGET_H

