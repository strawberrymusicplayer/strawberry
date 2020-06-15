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

#ifndef GROUPBYDIALOG_H
#define GROUPBYDIALOG_H

#include "config.h"

#include <memory>

#include <QDialog>
#include <QObject>
#include <QString>

#include "collectionmodel.h"
#include "ui_groupbydialog.h"

class QWidget;

class GroupByDialogPrivate;

class GroupByDialog : public QDialog {
  Q_OBJECT

 public:
  explicit GroupByDialog(QWidget *parent = nullptr);
  ~GroupByDialog() override;

 public slots:
  void CollectionGroupingChanged(const CollectionModel::Grouping &g);
  void accept() override;

 signals:
  void Accepted(const CollectionModel::Grouping &g);

 private slots:
  void Reset();

 private:
  std::unique_ptr<Ui_GroupByDialog> ui_;
  std::unique_ptr<GroupByDialogPrivate> p_;
};

#endif  // GROUPBYDIALOG_H
