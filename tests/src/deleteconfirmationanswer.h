/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DELETECONFIRMATIONANSWER_H
#define DELETECONFIRMATIONANSWER_H

#include <algorithm>
#include <functional>

#include <QApplication>
#include <QObject>
#include <QTimer>
#include <QList>
#include <QStringList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>

#include "dialogs/deleteconfirmationdialog.h"

// Answers each delete confirmation dialog shown while it exists with the buttons in order, the last button answers the rest, and keeps the files the dialogs listed.
// The dialog is modal and shown with exec(), so it's answered from a timer which runs in the event loop of the dialog.
// The function is called before answering, like something which happens while the dialog is open.
class DeleteConfirmationAnswer {
 public:
  explicit DeleteConfirmationAnswer(const QDialogButtonBox::StandardButton button, const std::function<void()> &while_shown = std::function<void()>()) : DeleteConfirmationAnswer(QList<QDialogButtonBox::StandardButton>() << button, while_shown) {}
  explicit DeleteConfirmationAnswer(const QList<QDialogButtonBox::StandardButton> &buttons, const std::function<void()> &while_shown = std::function<void()>()) : shown_(0) {
    timer_.setInterval(10);
    QObject::connect(&timer_, &QTimer::timeout, &timer_, [this, buttons, while_shown]() {
      DeleteConfirmationDialog *dialog = qobject_cast<DeleteConfirmationDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      const QDialogButtonBox::StandardButton button = buttons.value(std::min(static_cast<qsizetype>(shown_), buttons.count() - 1));
      ++shown_;
      const QListWidget *list = dialog->findChild<QListWidget*>();
      if (list) {
        for (int row = 0; row < list->count(); ++row) {
          files_ << list->item(row)->text();
        }
      }
      if (while_shown) while_shown();
      dialog->done(button);
    });
    timer_.start();
  }

  int shown() const { return shown_; }
  QStringList files() const { return files_; }

 private:
  QTimer timer_;
  int shown_;
  QStringList files_;
};

#endif  // DELETECONFIRMATIONANSWER_H
