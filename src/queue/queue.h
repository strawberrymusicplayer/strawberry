/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef QUEUE_H
#define QUEUE_H

#include "config.h"

#include <QObject>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>

class QMimeData;
class Playlist;

class Queue : public QAbstractProxyModel {
  Q_OBJECT

 public:
  explicit Queue(Playlist *playlist, QObject *parent = nullptr);

  // Query the queue
  bool is_empty() const;
  int PositionOf(const QModelIndex &source_index) const;
  bool ContainsSourceRow(const int source_row) const;
  int PeekNext() const;
  int ItemCount() const;
  quint64 GetTotalLength() const;

  // Modify the queue
  int TakeNext();
  void ToggleTracks(const QModelIndexList &source_indexes);
  void InsertFirst(const QModelIndexList &source_indexes);
  void Clear();
  void Move(const QList<int> &proxy_rows, int pos);
  void MoveUp(const int row);
  void MoveDown(const int row);
  void Remove(QList<int> &proxy_rows);

  // QAbstractProxyModel
  void setSourceModel(QAbstractItemModel *source_model) override;
  QModelIndex mapFromSource(const QModelIndex &source_index) const override;
  QModelIndex mapToSource(const QModelIndex &proxy_index) const override;

  // QAbstractItemModel
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &child) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &proxy_index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  QStringList mimeTypes() const override;
  Qt::DropActions supportedDropActions() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, const int row, const int column, const QModelIndex &parent_index) override;
  Qt::ItemFlags flags(const QModelIndex &idx) const override;

 public Q_SLOTS:
  void UpdateSummaryText();

 Q_SIGNALS:
  void TotalLengthChanged(const quint64 length);
  void ItemCountChanged(const int count);
  void SummaryTextChanged(const QString &message);

 private Q_SLOTS:
  void SourceDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right);
  void SourceLayoutChanged();
  void UpdateTotalLength();

 private:
  QList<QPersistentModelIndex> source_indexes_;
  const Playlist *playlist_;
  quint64 total_length_ns_;
  QMetaObject::Connection signal_item_count_changed_;
};

#endif  // QUEUE_H
