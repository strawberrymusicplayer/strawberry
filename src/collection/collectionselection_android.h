// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLLECTIONSELECTION_ANDROID_H
#define COLLECTIONSELECTION_ANDROID_H

#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPoint>
#include <QList>

class QAbstractItemModel;
class QTreeView;
class QLabel;
class QPushButton;
class QWidget;
class QMimeData;
class LongPressHandler;
class ActionDialog;

class CollectionSelection : public QObject {
  Q_OBJECT

 public:
  explicit CollectionSelection(QTreeView *view, QWidget *toolbar_parent);

  QWidget *toolbar() const { return toolbar_; }
  bool active() const { return active_; }
  bool Exit();
  bool Back();
  QModelIndexList SelectedIndexes() const;

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *data);
  void RescanSongs(const QModelIndexList &indexes);
  void Error(const QString &message);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  void BindModel();
  void CancelPress();
  void Enter();
  void UpdateSelection();
  void ShowActions();
  void PerformAction(const bool create, const bool play, const bool enqueue, const bool next);
  bool Selectable(const QModelIndex &index) const;

  QTreeView *view_;
  QWidget *toolbar_;
  QLabel *count_;
  QPushButton *actions_;
  const int drag_distance_;
  LongPressHandler *long_press_;
  QPointer<QAbstractItemModel> model_;
  QList<QMetaObject::Connection> connections_;
  QPointer<ActionDialog> dialog_;
  QPersistentModelIndex pressed_;
  QPoint press_position_;
  bool active_ = false;
  bool moved_ = false;
};

#endif  // COLLECTIONSELECTION_ANDROID_H
