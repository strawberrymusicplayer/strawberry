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

#include "config.h"

#include <algorithm>
#include <utility>

#include <QObject>
#include <QIODevice>
#include <QDataStream>
#include <QBuffer>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QMimeData>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>

#include "utilities/timeutils.h"
#include "playlist/playlist.h"
#include "queue.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kRowsMimetype[] = "application/x-strawberry-queue-rows";
}

Queue::Queue(Playlist *playlist, QObject *parent) : QAbstractProxyModel(parent), playlist_(playlist), total_length_ns_(0) {

  signal_item_count_changed_ = QObject::connect(this, &Queue::ItemCountChanged, this, &Queue::UpdateTotalLength);
  QObject::connect(this, &Queue::TotalLengthChanged, this, &Queue::UpdateSummaryText);

  UpdateSummaryText();

}

QModelIndex Queue::mapFromSource(const QModelIndex &source_index) const {

  if (!source_index.isValid()) return QModelIndex();

  const int source_row = source_index.row();
  for (int i = 0; i < source_indexes_.count(); ++i) {
    if (source_indexes_[i].row() == source_row) {
      return index(i, source_index.column());
    }
  }
  return QModelIndex();

}

bool Queue::ContainsSourceRow(const int source_row) const {

  for (int i = 0; i < source_indexes_.count(); ++i) {
    if (source_indexes_[i].row() == source_row) return true;
  }
  return false;

}

QModelIndex Queue::mapToSource(const QModelIndex &proxy_index) const {

  if (!proxy_index.isValid()) return QModelIndex();

  return source_indexes_[proxy_index.row()];

}

void Queue::setSourceModel(QAbstractItemModel *source_model) {

  if (sourceModel()) {
    QObject::disconnect(sourceModel(), &QAbstractItemModel::dataChanged, this, &Queue::SourceDataChanged);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &Queue::SourceLayoutChanged);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &Queue::SourceLayoutChanged);
  }

  QAbstractProxyModel::setSourceModel(source_model);

  QObject::connect(sourceModel(), &QAbstractItemModel::dataChanged, this, &Queue::SourceDataChanged);
  QObject::connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &Queue::SourceLayoutChanged);
  QObject::connect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &Queue::SourceLayoutChanged);

}

void Queue::SourceDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right) {

  for (int row = top_left.row(); row <= bottom_right.row(); ++row) {
    QModelIndex proxy_index = mapFromSource(sourceModel()->index(row, 0));
    if (!proxy_index.isValid()) continue;

    Q_EMIT dataChanged(proxy_index, proxy_index);
  }
  Q_EMIT ItemCountChanged(ItemCount());

}

void Queue::SourceLayoutChanged() {

  QObject::disconnect(signal_item_count_changed_);

  for (int i = 0; i < source_indexes_.count(); ++i) {
    if (!source_indexes_[i].isValid()) {
      beginRemoveRows(QModelIndex(), i, i);
      source_indexes_.removeAt(i);
      endRemoveRows();
      --i;
    }
  }

  signal_item_count_changed_ = QObject::connect(this, &Queue::ItemCountChanged, this, &Queue::UpdateTotalLength);

  Q_EMIT ItemCountChanged(ItemCount());

}

QModelIndex Queue::index(int row, int column, const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return createIndex(row, column);
}

QModelIndex Queue::parent(const QModelIndex &child) const {
  Q_UNUSED(child);
  return QModelIndex();
}

int Queue::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) return 0;
  return static_cast<int>(source_indexes_.count());
}

int Queue::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return 1;
}

QVariant Queue::data(const QModelIndex &proxy_index, int role) const {

  QModelIndex source_index = source_indexes_[proxy_index.row()];

  switch (role) {
    case Playlist::Role_QueuePosition:
      return proxy_index.row();

    case Qt::DisplayRole:{
      const QString artist = source_index.sibling(source_index.row(), static_cast<int>(Playlist::Column::Artist)).data().toString();
      const QString title = source_index.sibling(source_index.row(), static_cast<int>(Playlist::Column::Title)).data().toString();

      if (artist.isEmpty()) return title;
      return QStringLiteral("%1 - %2").arg(artist, title);
    }

    default:
      return QVariant();
  }

}

void Queue::ToggleTracks(const QModelIndexList &source_indexes) {

  for (const QModelIndex &source_index : source_indexes) {
    QModelIndex proxy_index = mapFromSource(source_index);
    if (proxy_index.isValid()) {
      // Dequeue the track
      const int row = proxy_index.row();
      beginRemoveRows(QModelIndex(), row, row);
      source_indexes_.removeAt(row);
      endRemoveRows();
    }
    else {
      // Enqueue the track
      const int row = static_cast<int>(source_indexes_.count());
      beginInsertRows(QModelIndex(), row, row);
      source_indexes_ << QPersistentModelIndex(source_index);
      endInsertRows();
    }
  }

}

void Queue::InsertFirst(const QModelIndexList &source_indexes) {

  for (const QModelIndex &source_index : source_indexes) {
    QModelIndex proxy_index = mapFromSource(source_index);
    if (proxy_index.isValid()) {
      // Already in the queue, so remove it to be reinserted later
      const int row = proxy_index.row();
      beginRemoveRows(QModelIndex(), row, row);
      source_indexes_.removeAt(row);
      endRemoveRows();
    }
  }

  const int rows = static_cast<int>(source_indexes.count());
  // Enqueue the tracks at the beginning
  beginInsertRows(QModelIndex(), 0, rows - 1);
  int offset = 0;
  for (const QModelIndex &source_index : source_indexes) {
    source_indexes_.insert(offset, QPersistentModelIndex(source_index));
    offset++;
  }
  endInsertRows();

}

int Queue::PositionOf(const QModelIndex &source_index) const {
  return mapFromSource(source_index).row();
}

bool Queue::is_empty() const { return source_indexes_.isEmpty(); }

int Queue::ItemCount() const { return static_cast<int>(source_indexes_.length()); }

quint64 Queue::GetTotalLength() const { return total_length_ns_; }

void Queue::UpdateTotalLength() {

  quint64 total = 0;

  for (const QPersistentModelIndex &row : std::as_const(source_indexes_)) {
    int id = row.row();

    Q_ASSERT(playlist_->has_item_at(id));

    const qint64 length = playlist_->item_at(id)->EffectiveMetadata().length_nanosec();
    if (length > 0) total += static_cast<quint64>(length);
  }

  total_length_ns_ = total;

  Q_EMIT TotalLengthChanged(total);

}

void Queue::UpdateSummaryText() {

  QString summary;
  int tracks = ItemCount();
  quint64 nanoseconds = GetTotalLength();

  summary += tr("%n track(s)", "", tracks);

  if (nanoseconds > 0) {
    summary += " - [ "_L1 + Utilities::WordyTimeNanosec(nanoseconds) + " ]"_L1;
  }

  Q_EMIT SummaryTextChanged(summary);

}

void Queue::Clear() {

  if (source_indexes_.isEmpty()) return;

  beginRemoveRows(QModelIndex(), 0, static_cast<int>(source_indexes_.count() - 1));
  source_indexes_.clear();
  endRemoveRows();

}

void Queue::Move(const QList<int> &proxy_rows, int pos) {

  Q_EMIT layoutAboutToBeChanged();
  QList<QPersistentModelIndex> moved_items;

  // Take the items out of the list first, keeping track of whether the insertion point changes
  int offset = 0;
  moved_items.reserve(proxy_rows.count());
  for (const int row : proxy_rows) {
    moved_items << source_indexes_.takeAt(row - offset);
    if (pos != -1 && pos >= row) --pos;
    ++offset;
  }

  // Put the items back in
  const int start = pos == -1 ? static_cast<int>(source_indexes_.count()) : pos;
  for (int i = start; i < start + moved_items.count(); ++i) {
    source_indexes_.insert(i, moved_items[i - start]);
  }

  // Update persistent indexes
  const QModelIndexList pindexes = persistentIndexList();
  for (const QModelIndex &pidx : pindexes) {
    const int dest_offset = static_cast<int>(proxy_rows.indexOf(pidx.row()));
    if (dest_offset != -1) {
      // This index was moved
      changePersistentIndex(pidx, index(start + dest_offset, pidx.column(), QModelIndex()));
    }
    else {
      int d = 0;
      for (int row : proxy_rows) {
        if (pidx.row() > row) d--;
      }
      if (pidx.row() + d >= start) d += static_cast<int>(proxy_rows.count());

      changePersistentIndex(pidx, index(pidx.row() + d, pidx.column(), QModelIndex()));
    }
  }

  Q_EMIT layoutChanged();

}

void Queue::MoveUp(int row) {
  Move(QList<int>() << row, row - 1);
}

void Queue::MoveDown(int row) {
  Move(QList<int>() << row, row + 2);
}

QStringList Queue::mimeTypes() const {
  return QStringList() << QLatin1String(kRowsMimetype) << QLatin1String(Playlist::kRowsMimetype);
}

Qt::DropActions Queue::supportedDropActions() const {
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
}

QMimeData *Queue::mimeData(const QModelIndexList &indexes) const {

  QMimeData *data = new QMimeData;

  QList<int> rows;
  for (const QModelIndex &idx : indexes) {
    if (idx.column() != 0) continue;

    rows << idx.row();
  }

  QBuffer buf;
  if (buf.open(QIODevice::WriteOnly)) {
    QDataStream stream(&buf);
    stream << rows;
    buf.close();
    data->setData(QLatin1String(kRowsMimetype), buf.data());
  }

  return data;

}

bool Queue::dropMimeData(const QMimeData *data, Qt::DropAction action, const int row, const int column, const QModelIndex &parent_index) {

  Q_UNUSED(column)
  Q_UNUSED(parent_index)

  if (action == Qt::IgnoreAction)
    return false;

  if (data->hasFormat(QLatin1String(kRowsMimetype))) {
    // Dragged from the queue

    QList<int> proxy_rows;
    QDataStream stream(data->data(QLatin1String(kRowsMimetype)));
    stream >> proxy_rows;

    // Make sure we take them in order
    std::stable_sort(proxy_rows.begin(), proxy_rows.end());

    Move(proxy_rows, row);
  }
  else if (data->hasFormat(QLatin1String(Playlist::kRowsMimetype))) {
    // Dragged from the playlist

    Playlist *playlist = nullptr;
    QList<int> source_rows;
    QDataStream stream(data->data(QLatin1String(Playlist::kRowsMimetype)));
    stream.readRawData(reinterpret_cast<char*>(&playlist), sizeof(&playlist));
    stream >> source_rows;

    QModelIndexList source_indexes;
    for (int source_row : std::as_const(source_rows)) {
      const QModelIndex source_index = sourceModel()->index(source_row, 0);
      const QModelIndex proxy_index = mapFromSource(source_index);
      if (proxy_index.isValid()) {
        // This row was already in the queue, so no need to add it again
        continue;
      }

      source_indexes << source_index;
    }

    if (!source_indexes.isEmpty()) {
      const int insert_point = row == -1 ? static_cast<int>(source_indexes_.count()) : row;
      beginInsertRows(QModelIndex(), insert_point, insert_point + static_cast<int>(source_indexes.count() - 1));
      for (int i = 0; i < source_indexes.count(); ++i) {
        source_indexes_.insert(insert_point + i, source_indexes[i]);
      }
      endInsertRows();
    }
  }

  return true;

}

Qt::ItemFlags Queue::flags(const QModelIndex &idx) const {

  Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  if (idx.isValid()) {
    flags |= Qt::ItemIsDragEnabled;
  }
  else {
    flags |= Qt::ItemIsDropEnabled;
  }

  return flags;

}

int Queue::PeekNext() const {

  if (source_indexes_.isEmpty()) return -1;
  return source_indexes_.first().row();

}

int Queue::TakeNext() {

  if (source_indexes_.isEmpty()) return -1;

  beginRemoveRows(QModelIndex(), 0, 0);
  int ret = source_indexes_.takeFirst().row();
  endRemoveRows();

  return ret;

}

QVariant Queue::headerData(int section, Qt::Orientation orientation, int role) const {
  Q_UNUSED(section);
  Q_UNUSED(orientation);
  Q_UNUSED(role);
  return QVariant();
}

void Queue::Remove(QList<int> &proxy_rows) {

  // Order the rows
  std::stable_sort(proxy_rows.begin(), proxy_rows.end());

  // Reflects immediately changes in the playlist
  Q_EMIT layoutAboutToBeChanged();

  int removed_rows = 0;
  for (int row : proxy_rows) {
    // After the first row, the row number needs to be updated
    const int real_row = row - removed_rows;
    beginRemoveRows(QModelIndex(), real_row, real_row);
    source_indexes_.removeAt(real_row);
    endRemoveRows();
    removed_rows++;
  }

  Q_EMIT layoutChanged();

}
