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
#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QVBoxLayout>

#include "core/mimedata.h"
#include "longpresshandler.h"
#include "playlistactionhandler.h"

PlaylistActionHandler::PlaylistActionHandler(QAbstractItemView *view, const QString &section, Eligible eligible, CreateMimeData create_mimedata)
    : QObject(view),
      view_(view),
      section_(section),
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
  QDialog *dialog = new QDialog(view_);
  dialog_ = dialog;
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("Playlist actions"));
  // Android positioned native dialogs receive offset touch coordinates.
  dialog->setWindowState(Qt::WindowFullScreen);
  QVBoxLayout *layout = new QVBoxLayout(dialog);
  QLabel *title = new QLabel(tr("%1 > %2").arg(section_, index.data().toString()), dialog);
  title->setTextFormat(Qt::PlainText);
  title->setWordWrap(true);
  layout->addWidget(title);
  layout->addStretch();
  QPushButton *create = new QPushButton(tr("Create saved playlist"), dialog);
  QPushButton *play = new QPushButton(tr("Create and play"), dialog);
  QPushButton *back = new QPushButton(tr("Back"), dialog);
  for (QPushButton *button : {create, play, back}) {
    button->setMinimumHeight(48);
    button->setAutoDefault(false);
    layout->addWidget(button);
  }
  layout->addStretch();
  QShortcut *shortcut = new QShortcut(dialog);
  shortcut->setKeys({QKeySequence(Qt::Key_Back), QKeySequence(Qt::Key_Escape)});
  QObject::connect(shortcut, &QShortcut::activated, dialog, &QDialog::reject);
  QObject::connect(back, &QPushButton::clicked, dialog, &QDialog::reject);
  QObject::connect(view_->model(), &QAbstractItemModel::modelAboutToBeReset, dialog, &QDialog::reject);
  QObject::connect(view_->model(), &QAbstractItemModel::rowsAboutToBeRemoved, dialog, &QDialog::reject);
  QObject::connect(view_->model(), &QObject::destroyed, dialog, &QDialog::reject);
  const auto perform = [this, dialog, index](const bool play_now) {
    dialog_.clear();
    dialog->accept();
    if (!index.isValid() || index.model() != view_->model() || !eligible_(index)) return;
    MimeData *data = create_mimedata_(index);
    if (!data) return;
    data->override_user_settings_ = true;
    data->open_in_new_playlist_ = true;
    data->play_now_ = play_now;
    Q_EMIT AddToPlaylist(data);
  };
  QObject::connect(create, &QPushButton::clicked, dialog, [perform]() { perform(false); });
  QObject::connect(play, &QPushButton::clicked, dialog, [perform]() { perform(true); });
  dialog->open();
  dialog->activateWindow();
  dialog->setFocus(Qt::OtherFocusReason);

}
