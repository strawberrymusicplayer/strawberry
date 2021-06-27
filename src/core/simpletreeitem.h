/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef SIMPLETREEITEM_H
#define SIMPLETREEITEM_H

#include "config.h"

#include <QList>
#include <QString>
#include <QAbstractItemModel>

#include "simpletreemodel.h"

template<typename T>
class SimpleTreeItem {
 public:
  explicit SimpleTreeItem(SimpleTreeModel<T> *_model);  // For the root item
  explicit SimpleTreeItem(const QString &_key, T *_parent = nullptr);
  explicit SimpleTreeItem(T *_parent = nullptr);
  virtual ~SimpleTreeItem();

  void InsertNotify(T *_parent);
  void DeleteNotify(int child_row);
  void ClearNotify();
  void ChangedNotify();

  void Delete(int child_row);
  T *ChildByKey(const QString &key) const;

  QString DisplayText() const { return display_text; }
  QString SortText() const { return sort_text; }

  QString container_key;
  QString sort_text;
  QString display_text;

  int row;

  T *parent;
  QList<T*> children;
  QAbstractItemModel *child_model;

  SimpleTreeModel<T> *model;
};

template<typename T>
SimpleTreeItem<T>::SimpleTreeItem(SimpleTreeModel<T> *_model)
    : row(0),
      parent(nullptr),
      child_model(nullptr),
      model(_model) {}

template<typename T>
SimpleTreeItem<T>::SimpleTreeItem(const QString &_container_key, T *_parent)
    : container_key(_container_key),
      parent(_parent),
      child_model(nullptr),
      model(_parent ? _parent->model : nullptr) {
  if (parent) {
    row = parent->children.count();
    parent->children << static_cast<T*>(this);
  }
}

template<typename T>
SimpleTreeItem<T>::SimpleTreeItem(T *_parent)
    : parent(_parent),
      child_model(nullptr),
      model(_parent ? _parent->model : nullptr) {
  if (parent) {
    row = parent->children.count();
    parent->children << static_cast<T*>(this);
  }
}

template<typename T>
void SimpleTreeItem<T>::InsertNotify(T *_parent) {
  parent = _parent;
  model = parent->model;
  row = parent->children.count();

  model->BeginInsert(parent, row);
  parent->children << static_cast<T*>(this);
  model->EndInsert();
}

template<typename T>
void SimpleTreeItem<T>::DeleteNotify(const int child_row) {
  model->BeginDelete(static_cast<T*>(this), child_row);
  delete children.takeAt(child_row);

  // Adjust row numbers of those below it :(
  for (int i = child_row; i < children.count(); ++i) children[i]->row--;
  model->EndDelete();
}

template<typename T>
void SimpleTreeItem<T>::ClearNotify() {
  if (children.count()) {
    model->BeginDelete(static_cast<T*>(this), 0, children.count() - 1);

    qDeleteAll(children);
    children.clear();

    model->EndDelete();
  }
}

template<typename T>
void SimpleTreeItem<T>::ChangedNotify() {
  model->EmitDataChanged(static_cast<T*>(this));
}

template<typename T>
SimpleTreeItem<T>::~SimpleTreeItem() {
  qDeleteAll(children);
}

template<typename T>
void SimpleTreeItem<T>::Delete(int child_row) {
  delete children.takeAt(child_row);

  // Adjust row numbers of those below it :(
  for (int i = child_row; i < children.count(); ++i) children[i]->row--;
}

template<typename T>
T *SimpleTreeItem<T>::ChildByKey(const QString &_key) const {
  for (T *child : children) {
    if (child->key == _key) return child;
  }
  return nullptr;
}

#endif  // SIMPLETREEITEM_H
