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

#ifndef GROUPEDICONVIEW_H
#define GROUPEDICONVIEW_H

#include <QObject>
#include <QListView>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QList>
#include <QString>
#include <QFont>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QRegion>

class QWidget;
class QPainter;
class QModelIndex;
class QPaintEvent;
class QResizeEvent;
class MultiSortFilterProxy;

class GroupedIconView : public QListView {
  Q_OBJECT

  // Vertical space separating a header from the items above and below it.
  Q_PROPERTY(int header_spacing READ header_spacing WRITE set_header_spacing)

  // Horizontal space separating a header from the left and right edges of the widget.
  Q_PROPERTY(int header_indent READ header_indent WRITE set_header_indent)

  // Horizontal space separating an item from the left and right edges of the widget.
  Q_PROPERTY(int item_indent READ item_indent WRITE set_item_indent)

  // The text of each group's header.  Must contain "%1".
  Q_PROPERTY(QString header_text READ header_text WRITE set_header_text)

 public:
  explicit GroupedIconView(QWidget *parent = nullptr);

  enum Role {
    Role_Group = 1158300,
  };

  void AddSortSpec(const int role, const Qt::SortOrder order = Qt::AscendingOrder);

  int header_spacing() const { return header_spacing_; }
  int header_indent() const { return header_indent_; }
  int item_indent() const { return item_indent_; }
  const QString &header_text() const { return header_text_; }

  void set_header_spacing(const int value) { header_spacing_ = value; }
  void set_header_indent(const int value) { header_indent_ = value; }
  void set_item_indent(const int value) { item_indent_ = value; }
  void set_header_text(const QString &value) { header_text_ = value; }

  // QAbstractItemView
  QModelIndex moveCursor(CursorAction action, const Qt::KeyboardModifiers keyboard_modifiers) override;
  void setModel(QAbstractItemModel *model) override;

  static void DrawHeader(QPainter *painter, const QRect rect, const QFont &font, const QPalette &palette, const QString &text);

 protected:
  virtual int header_height() const;

  // QWidget
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

  // QAbstractItemView
  void dataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right, const QList<int> &roles = QList<int>()) override;
  QModelIndex indexAt(const QPoint &p) const override;
  void rowsInserted(const QModelIndex &parent, int start, int end) override;
  void setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) override;
  QRect visualRect(const QModelIndex &idx) const override;
  QRegion visualRegionForSelection(const QItemSelection &selection) const override;

 private Q_SLOTS:
  void LayoutItems();

 private:
  struct Header {
    int y;
    int first_row;
    QString text;
  };

  // Returns the items that are wholly or partially inside the rect.
  QList<QModelIndex> IntersectingItems(const QRect rect) const;

  // Returns the index of the item above (d=-1) or below (d=+1) the given item.
  int IndexAboveOrBelow(int index, const int d) const;

  MultiSortFilterProxy *proxy_model_;
  QList<QRect> visual_rects_;
  QList<Header> headers_;

  const int default_header_height_;
  int header_spacing_;
  int header_indent_;
  int item_indent_;
  QString header_text_;
};

#endif  // GROUPEDICONVIEW_H
