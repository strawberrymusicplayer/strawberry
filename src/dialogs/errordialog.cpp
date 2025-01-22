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

#include "config.h"

#include <utility>

#include <QWidget>
#include <QString>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QLabel>
#include <QTextEdit>
#include <QCloseEvent>

#include "errordialog.h"
#include "ui_errordialog.h"

using namespace Qt::Literals::StringLiterals;

ErrorDialog::ErrorDialog(QWidget *parent)
    : QDialog(parent),
      parent_(parent),
      ui_(new Ui_ErrorDialog) {

  ui_->setupUi(this);

  QIcon warning_icon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
  QPixmap warning_pixmap(warning_icon.pixmap(48));

  QPalette messages_palette(ui_->messages->palette());
  messages_palette.setColor(QPalette::Base, messages_palette.color(QPalette::Window));

  ui_->messages->setPalette(messages_palette);
  ui_->icon->setPixmap(warning_pixmap);

}

ErrorDialog::~ErrorDialog() {
  delete ui_;
}

void ErrorDialog::ShowMessage(const QString &message) {

  if (message.isEmpty()) return;

  current_messages_ << message;
  UpdateContent();

  show();

  if (parent_ && parent_->isMaximized()) {
    raise();
    activateWindow();
  }

}

void ErrorDialog::closeEvent(QCloseEvent *e) {

  current_messages_.clear();
  UpdateContent();

  QDialog::closeEvent(e);

}

void ErrorDialog::UpdateContent() {

  QString html;
  for (const QString &message : std::as_const(current_messages_)) {
    if (!html.isEmpty()) {
      html += "<hr/>"_L1;
    }
    html += message.toHtmlEscaped();
  }
  ui_->messages->setHtml(html);

}
