#include "collectionlettergroups_android.h"

#include <utility>

#include <QAbstractItemModel>
#include <QEvent>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QTreeView>

#include "collectionitemdelegate.h"
#include "collectionmodel.h"

namespace {

class LetterGroupDelegate : public CollectionItemDelegate {
 public:
  explicit LetterGroupDelegate(CollectionLetterGroups *groups)
      : CollectionItemDelegate(groups), groups_(groups) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    CollectionItemDelegate::paint(painter, option, index);
    if (!groups_->enabled() || !index.data(CollectionModel::Role_IsDivider).toBool()) return;

    QStyleOption branch;
    branch.rect = QRect(option.rect.left() + 6, option.rect.center().y() - 9, 18, 18);
    branch.palette = option.palette;
    branch.state = QStyle::State_Enabled | QStyle::State_Children;
    if (!groups_->IsCollapsed(index)) branch.state |= QStyle::State_Open;
    option.widget->style()->drawPrimitive(QStyle::PE_IndicatorBranch, &branch, painter, option.widget);
  }

 private:
  CollectionLetterGroups *groups_;
};

}

CollectionLetterGroups::CollectionLetterGroups(QTreeView *view)
    : QObject(view), view_(view), update_timer_(new QTimer(this)) {
  view_->setItemDelegate(new LetterGroupDelegate(this));
  view_->installEventFilter(this);
  update_timer_->setSingleShot(true);
  update_timer_->setInterval(0);
  QObject::connect(update_timer_, &QTimer::timeout, this, &CollectionLetterGroups::ApplyRows);
  QObject::connect(view_, &QTreeView::clicked, this, [this](const QModelIndex &index) {
    if (!enabled_ || index.parent().isValid() || !index.data(CollectionModel::Role_IsDivider).toBool()) return;
    const QString key = index.data(CollectionModel::Role_DividerKey).toString();
    if (collapsed_.contains(key)) collapsed_.remove(key);
    else collapsed_.insert(key);
    // Move the current item before hiding it, so restoring focus cannot immediately reopen this group.
    view_->setCurrentIndex(index);
    ApplyRows();
  });
}

void CollectionLetterGroups::SetEnabled(const bool enabled) {
  enabled_ = enabled;
  if (!enabled_) collapsed_.clear();
  BindModel();
  ApplyRows();
}

bool CollectionLetterGroups::IsCollapsed(const QModelIndex &index) const {
  return collapsed_.contains(index.data(CollectionModel::Role_DividerKey).toString());
}

bool CollectionLetterGroups::eventFilter(QObject *object, QEvent *event) {
  if (object == view_ && event->type() == QEvent::Show) {
    BindModel();
    ApplyRows();
  }
  return QObject::eventFilter(object, event);
}

void CollectionLetterGroups::BindModel() {
  if (model_ == view_->model()) return;
  for (const QMetaObject::Connection &connection : std::as_const(connections_)) QObject::disconnect(connection);
  connections_.clear();
  collapsed_.clear();
  model_ = view_->model();
  if (!model_) return;

  const auto schedule = [this]() { update_timer_->start(); };
  connections_ << QObject::connect(model_, &QAbstractItemModel::rowsInserted, this, schedule);
  connections_ << QObject::connect(model_, &QAbstractItemModel::rowsRemoved, this, schedule);
  connections_ << QObject::connect(model_, &QAbstractItemModel::layoutChanged, this, schedule);
  connections_ << QObject::connect(model_, &QAbstractItemModel::dataChanged, this, schedule);
  connections_ << QObject::connect(model_, &QAbstractItemModel::modelReset, this, [this]() {
    collapsed_.clear();
    update_timer_->start();
  });
  connections_ << QObject::connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
      [this](const QModelIndex &current) { Reveal(current); });
}

void CollectionLetterGroups::ApplyRows() {
  if (!model_) return;
  QSet<QString> visible_dividers;
  for (int row = 0; row < model_->rowCount(); ++row) {
    const QModelIndex index = model_->index(row, 0);
    if (index.data(CollectionModel::Role_IsDivider).toBool()) {
      visible_dividers.insert(index.data(CollectionModel::Role_DividerKey).toString());
    }
  }
  for (int row = 0; row < model_->rowCount(); ++row) {
    const QModelIndex index = model_->index(row, 0);
    const QString key = index.data(CollectionModel::Role_DividerKey).toString();
    const bool hidden = enabled_ && !index.data(CollectionModel::Role_IsDivider).toBool() &&
                        visible_dividers.contains(key) && collapsed_.contains(key);
    view_->setRowHidden(row, QModelIndex(), hidden);
  }
  // Hidden selections must not be added to a playlist by a later action.
  const QModelIndexList selected = view_->selectionModel()->selectedIndexes();
  for (QModelIndex index : selected) {
    QModelIndex root = index;
    while (root.parent().isValid()) root = root.parent();
    if (view_->isRowHidden(root.row(), QModelIndex())) {
      view_->selectionModel()->select(index, QItemSelectionModel::Deselect);
    }
  }
  view_->viewport()->update();
}

void CollectionLetterGroups::Reveal(const QModelIndex &index) {
  if (!enabled_ || !index.isValid()) return;
  QModelIndex root = index;
  while (root.parent().isValid()) root = root.parent();
  if (!root.data(CollectionModel::Role_IsDivider).toBool() &&
      collapsed_.remove(root.data(CollectionModel::Role_DividerKey).toString())) {
    ApplyRows();
  }
}
