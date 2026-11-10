/*
 * Strawberry Music Player
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

#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>

#include "core/mimedata.h"
#include "actiondialog.h"
#include "longpresshandler.h"
#include "playlistactionhandler.h"

PlaylistActionHandler::PlaylistActionHandler(QAbstractItemView *view, const QString &section, const Labels &labels, Eligible eligible, CreateMimeData create_mimedata)
    : QObject(view),
      view_(view),
      section_(section),
      labels_(labels),
      eligible_(std::move(eligible)),
      create_mimedata_(std::move(create_mimedata)) {

  long_press_ = new LongPressHandler(LongPressHandler::PlatformInterval(), LongPressHandler::PlatformDragDistance(view_->devicePixelRatioF()), this);
  view_->installEventFilter(this);
  view_->viewport()->installEventFilter(this);
  QObject::connect(long_press_, &LongPressHandler::LongPressed, this, &PlaylistActionHandler::ShowActions);
  QObject::connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &PlaylistActionHandler::Cancel);
  QObject::connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, &PlaylistActionHandler::Cancel);
  QObject::connect(qApp, &QApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive) Cancel();
  });

}

void PlaylistActionHandler::AddAction(const QString &text, Eligible available, Trigger trigger) {

  extra_actions_ << ExtraAction{text, std::move(available), std::move(trigger)};

}

bool PlaylistActionHandler::eventFilter(QObject *object, QEvent *event) {

  if (event->type() == QEvent::Hide) Cancel();
  if (event->type() == QEvent::TouchCancel || event->type() == QEvent::UngrabMouse) long_press_->Cancel();
  if (object != view_->viewport()) return false;

  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      long_press_->Release();
      Cancel();
      const auto *mouse = static_cast<QMouseEvent *>(event);
      if (mouse->button() != Qt::LeftButton) break;
      const QModelIndex index = view_->indexAt(mouse->pos());
      if (!index.isValid() || !eligible_(index)) break;
      index_ = index;
      long_press_->Press(mouse->pos());
      break;
    }
    case QEvent::MouseMove:
      return long_press_->Move(static_cast<QMouseEvent *>(event)->pos());
    case QEvent::MouseButtonRelease:
      return long_press_->Release();
    case QEvent::ContextMenu:
      return true;
    default:
      break;
  }
  return false;

}

void PlaylistActionHandler::Cancel() {

  long_press_->Cancel();
  if (dialog_) dialog_->reject();
  index_ = QPersistentModelIndex();

}

void PlaylistActionHandler::ShowActions() {

  if (!view_->isVisible() || !index_.isValid() || index_.model() != view_->model() || !eligible_(index_)) return;
  const QPersistentModelIndex index = index_;
  ActionDialog *dialog = new ActionDialog(view_, tr("Playlist actions"), tr("%1 > %2").arg(section_, index.sibling(index.row(), 0).data().toString()), ActionDialog::TitlePosition::Top);
  dialog_ = dialog;
  QPushButton *add = dialog->AddButton(labels_.add);
  QPushButton *enqueue = dialog->AddButton(labels_.enqueue);
  QPushButton *enqueue_next = dialog->AddButton(labels_.enqueue_next);
  QPushButton *create = dialog->AddButton(tr("Create saved playlist"));
  QPushButton *play = dialog->AddButton(tr("Create and play"));
  for (const ExtraAction &action : std::as_const(extra_actions_)) {
    if (!action.available(index)) continue;
    QPushButton *button = dialog->AddButton(action.text);
    const Trigger trigger = action.trigger;
    QObject::connect(button, &QPushButton::clicked, dialog, [this, dialog, index, trigger]() {
      dialog_.clear();
      dialog->accept();
      if (index.isValid() && index.model() == view_->model() && eligible_(index)) trigger(index);
    });
  }
  dialog->AddCancelButton(tr("Back"));
  QObject::connect(view_->model(), &QAbstractItemModel::modelAboutToBeReset, dialog, &QDialog::reject);
  QObject::connect(view_->model(), &QAbstractItemModel::rowsAboutToBeRemoved, dialog, &QDialog::reject);
  QObject::connect(view_->model(), &QObject::destroyed, dialog, &QDialog::reject);
  const auto perform = [this, dialog, index](const bool open_in_new_playlist, const bool play_now, const bool enqueue_now, const bool enqueue_next_now) {
    dialog_.clear();
    dialog->accept();
    if (!index.isValid() || index.model() != view_->model() || !eligible_(index)) return;
    MimeData *data = create_mimedata_(index);
    if (!data) return;
    data->override_user_settings_ = true;
    data->open_in_new_playlist_ = open_in_new_playlist;
    data->play_now_ = play_now;
    data->enqueue_now_ = enqueue_now;
    data->enqueue_next_now_ = enqueue_next_now;
    Q_EMIT AddToPlaylist(data);
  };
  QObject::connect(add, &QPushButton::clicked, dialog, [perform]() { perform(false, false, false, false); });
  QObject::connect(enqueue, &QPushButton::clicked, dialog, [perform]() { perform(false, false, true, false); });
  QObject::connect(enqueue_next, &QPushButton::clicked, dialog, [perform]() { perform(false, false, false, true); });
  QObject::connect(create, &QPushButton::clicked, dialog, [perform]() { perform(true, false, false, false); });
  QObject::connect(play, &QPushButton::clicked, dialog, [perform]() { perform(true, true, false, false); });
  dialog->Open();

}
