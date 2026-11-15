// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectionselection_android.h"

#include <utility>

#include <QApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "collectionmodel.h"
#include "core/logging.h"
#include "core/mimedata.h"
#include "widgets/actiondialog.h"
#include "widgets/longpresshandler.h"

namespace {

constexpr int kCheckboxWidth = 40;

// Keep the collection's existing album art and letter-divider painting.
class SelectionDelegate : public QStyledItemDelegate {
 public:
  SelectionDelegate(QTreeView *view, CollectionSelection *selection)
      : QStyledItemDelegate(selection), view_(view), selection_(selection), original_(view->itemDelegate()) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    QStyleOptionViewItem content(option);
    if (selection_->active() && index.flags().testFlag(Qt::ItemIsSelectable)) {
      QStyleOptionButton check;
      check.rect = QRect(option.rect.left(), option.rect.top(), kCheckboxWidth, option.rect.height());
      check.state = QStyle::State_Enabled | (view_->selectionModel()->isSelected(index) ? QStyle::State_On : QStyle::State_Off);
      const QRect indicator = view_->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &check, view_);
      check.rect = QStyle::alignedRect(option.direction, Qt::AlignCenter, indicator.size(), check.rect);
      view_->style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, painter, view_);
      if (option.direction == Qt::RightToLeft) content.rect.adjust(0, 0, -kCheckboxWidth, 0);
      else content.rect.adjust(kCheckboxWidth, 0, 0, 0);
    }
    original_->paint(painter, content, index);
  }

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    QSize size = original_->sizeHint(option, index);
    if (selection_->active() && index.flags().testFlag(Qt::ItemIsSelectable)) {
      size.rwidth() += kCheckboxWidth;
      size.setHeight(qMax(size.height(), 48));
    }
    return size;
  }

 private:
  QTreeView *view_;
  CollectionSelection *selection_;
  QAbstractItemDelegate *original_;
};

}  // namespace

CollectionSelection::CollectionSelection(QTreeView *view, QWidget *toolbar_parent)
    : QObject(view),
      view_(view),
      toolbar_(new QWidget(toolbar_parent)),
      count_(new QLabel(toolbar_)),
      actions_(new QPushButton(tr("Actions"), toolbar_)),
      drag_distance_(LongPressHandler::PlatformDragDistance(view->devicePixelRatioF())),
      long_press_(new LongPressHandler(LongPressHandler::PlatformInterval(), drag_distance_, this)) {

  toolbar_->setObjectName(QStringLiteral("collection_selection_toolbar"));
  auto *layout = new QHBoxLayout(toolbar_);
  layout->setContentsMargins(4, 0, 4, 0);
  count_->setWordWrap(true);
  layout->addWidget(count_, 1);
  layout->addWidget(actions_);
  auto *done = new QPushButton(tr("Done"), toolbar_);
  layout->addWidget(done);
  actions_->setMinimumHeight(48);
  done->setMinimumHeight(48);
  toolbar_->hide();
  view_->setSelectionMode(QAbstractItemView::NoSelection);
  view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view_->setItemDelegate(new SelectionDelegate(view_, this));
  view_->installEventFilter(this);
  view_->viewport()->installEventFilter(this);
  QObject::connect(done, &QPushButton::clicked, this, &CollectionSelection::Exit);
  QObject::connect(actions_, &QPushButton::clicked, this, &CollectionSelection::ShowActions);
  QObject::connect(long_press_, &LongPressHandler::LongPressed, this, &CollectionSelection::Enter);
  QObject::connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &CollectionSelection::CancelPress);
  QObject::connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, &CollectionSelection::CancelPress);
  QObject::connect(qApp, &QApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive) CancelPress();
  });
  BindModel();

}

bool CollectionSelection::Selectable(const QModelIndex &index) const {
  return index.isValid() && index.flags().testFlag(Qt::ItemIsEnabled) && index.flags().testFlag(Qt::ItemIsSelectable) &&
         !index.data(CollectionModel::Role_IsDivider).toBool();
}

void CollectionSelection::BindModel() {

  if (model_ == view_->model()) return;
  Exit();
  for (const auto &connection : std::as_const(connections_)) QObject::disconnect(connection);
  connections_.clear();
  model_ = view_->model();
  if (!model_) return;
  view_->selectionModel()->clearSelection();
  connections_ << QObject::connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CollectionSelection::UpdateSelection);
  connections_ << QObject::connect(model_, &QAbstractItemModel::modelAboutToBeReset, this, &CollectionSelection::Exit);
  connections_ << QObject::connect(model_, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this]() {
    CancelPress();
    if (dialog_) dialog_->reject();
  });
  connections_ << QObject::connect(model_, &QObject::destroyed, this, &CollectionSelection::Exit);

}

void CollectionSelection::CancelPress() {
  long_press_->Cancel();
  pressed_ = QPersistentModelIndex();
  moved_ = true;
}

void CollectionSelection::Enter() {

  if (!Selectable(pressed_) || !view_->isVisible()) return;
  view_->setFocus(Qt::OtherFocusReason);
  active_ = true;
  view_->selectionModel()->select(pressed_, QItemSelectionModel::Select | QItemSelectionModel::Rows);
  CancelPress();
  toolbar_->show();
  view_->doItemsLayout();
  UpdateSelection();

}

bool CollectionSelection::Exit() {

  CancelPress();
  if (!active_) return false;
  active_ = false;
  if (dialog_) dialog_->reject();
  if (view_->selectionModel()) view_->selectionModel()->clearSelection();
  toolbar_->hide();
  view_->setFocus(Qt::OtherFocusReason);
  view_->doItemsLayout();
  view_->viewport()->update();
  return true;

}

bool CollectionSelection::Back() {

  if (dialog_ && dialog_->isVisible()) {
    dialog_->reject();
    view_->setFocus(Qt::OtherFocusReason);
    return true;
  }
  return Exit();

}

void CollectionSelection::UpdateSelection() {

  const int count = view_->selectionModel()->selectedRows().count();
  count_->setText(tr("%n item(s) selected", nullptr, count));
  count_->setToolTip(tr("Selected artists and albums include their songs."));
  actions_->setEnabled(count > 0);
  view_->viewport()->update();

}

QModelIndexList CollectionSelection::SelectedIndexes() const {

  QModelIndexList indexes;
  // An explicitly selected descendant is redundant when its artist or album is selected.
  for (const QModelIndex &index : view_->selectionModel()->selectedRows()) {
    if (!Selectable(index)) continue;
    QModelIndex parent = index.parent();
    while (parent.isValid() && !view_->selectionModel()->isSelected(parent)) parent = parent.parent();
    if (!parent.isValid()) indexes << index;
  }
  return indexes;

}

bool CollectionSelection::eventFilter(QObject *object, QEvent *event) {

  if (object == view_) {
    if (event->type() == QEvent::Show) BindModel();
    if (event->type() == QEvent::Hide) CancelPress();
    if (event->type() == QEvent::KeyPress) {
      const int key = static_cast<QKeyEvent*>(event)->key();
      if ((key == Qt::Key_Back || key == Qt::Key_Escape) && Back()) return true;
    }
    return false;
  }
  if (object != view_->viewport()) return false;
  if (!view_->model() || view_->model()->rowCount() == 0) return false;
  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      const auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() != Qt::LeftButton) return false;
      BindModel();
      CancelPress();
      long_press_->Release();
      press_position_ = mouse->pos();
      pressed_ = view_->indexAt(press_position_);
      moved_ = false;
      if (pressed_.isValid()) long_press_->Press(press_position_);
      // Do not deliver a speculative press to QTreeView: it selects before Qt resolves scrolling.
      return true;
    }
    case QEvent::MouseMove: {
      const QPoint position = static_cast<QMouseEvent*>(event)->pos();
      if ((position - press_position_).manhattanLength() > drag_distance_) CancelPress();
      return true;
    }
    case QEvent::MouseButtonRelease: {
      const auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() != Qt::LeftButton) return false;
      const QPersistentModelIndex index = pressed_;
      const bool held = long_press_->Release();
      const bool tap = !moved_ && !held && index.isValid() && index == view_->indexAt(mouse->pos());
      CancelPress();
      if (!tap) return true;
      if (!index.flags().testFlag(Qt::ItemIsEnabled)) return true;
      const QRect rect = view_->visualRect(index);
      const bool checkbox = rect.contains(mouse->pos()) && (view_->layoutDirection() == Qt::RightToLeft ? mouse->pos().x() >= rect.right() - kCheckboxWidth : mouse->pos().x() < rect.left() + kCheckboxWidth);
      view_->setFocus(Qt::OtherFocusReason);
      view_->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
      if (active_ && Selectable(index) && (checkbox || !view_->model()->hasChildren(index))) {
        view_->selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
      }
      else {
        Q_EMIT view_->clicked(index);
      }
      return true;
    }
    case QEvent::MouseButtonDblClick:
    case QEvent::ContextMenu:
      return true;
    case QEvent::TouchCancel:
    case QEvent::UngrabMouse:
    case QEvent::Hide:
      CancelPress();
      break;
    default:
      break;
  }
  return false;

}

void CollectionSelection::ShowActions() {

  if (!active_ || SelectedIndexes().isEmpty()) return;
  auto *dialog = new ActionDialog(view_, tr("Collection"), tr("Collection > Selected music\nSelected artists and albums include their songs."), ActionDialog::TitlePosition::Top);
  dialog_ = dialog;
  QObject::connect(dialog, &QDialog::finished, view_, [this]() { view_->setFocus(Qt::OtherFocusReason); });
  const auto add = [this, dialog](const QString &text, const bool create, const bool play, const bool enqueue, const bool next) {
    QObject::connect(dialog->AddButton(text), &QPushButton::clicked, dialog, [this, dialog, create, play, enqueue, next]() {
      dialog->accept();
      PerformAction(create, play, enqueue, next);
    });
  };
  add(tr("Add to current playlist"), false, false, false, false);
  add(tr("Queue"), false, false, true, false);
  add(tr("Queue to play next"), false, false, false, true);
  add(tr("Create saved playlist"), true, false, false, false);
  add(tr("Create and play"), true, true, false, false);
  const QModelIndexList indexes = SelectedIndexes();
  bool editable = true;
  for (const QModelIndex &index : indexes) editable = editable && index.data(CollectionModel::Role_Editable).toBool();
  if (editable) {
    QObject::connect(dialog->AddButton(tr("Rescan selected music")), &QPushButton::clicked, dialog, [this, dialog]() {
      dialog->accept();
      Q_EMIT RescanSongs(SelectedIndexes());
    });
  }
  dialog->AddCancelButton(tr("Back"));
  dialog->Open();

}

void CollectionSelection::PerformAction(const bool create, const bool play, const bool enqueue, const bool next) {

  const QModelIndexList indexes = SelectedIndexes();
  if (indexes.isEmpty()) {
    Q_EMIT Error(tr("No music is selected."));
    return;
  }
  QMimeData *data = view_->model()->mimeData(indexes);
  auto *songs = qobject_cast<MimeData*>(data);
  if (!songs) {
    delete data;
    qLog(Error) << "Cannot create playlist from selection: collection returned invalid MIME data";
    Q_EMIT Error(tr("Could not read the selected music."));
    return;
  }
  songs->override_user_settings_ = true;
  songs->open_in_new_playlist_ = create;
  songs->play_now_ = play;
  songs->enqueue_now_ = enqueue;
  songs->enqueue_next_now_ = next;
  Q_EMIT AddToPlaylist(songs);

}
