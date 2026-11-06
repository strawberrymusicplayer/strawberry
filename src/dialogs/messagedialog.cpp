/*
 * Strawberry Music Player
 * Copyright 2020-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QDialog>
#include <QDialogButtonBox>
#include <QString>
#include <QPixmap>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>
#include <QCheckBox>
#include <QSettings>
#ifdef Q_OS_ANDROID
#  include <QScrollArea>
#endif

#include "core/settings.h"
#include "utilities/screenutils.h"
#include "messagedialog.h"
#include "ui_messagedialog.h"

MessageDialog::MessageDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_MessageDialog), parent_(parent) {

  ui_->setupUi(this);

#ifdef Q_OS_ANDROID
  ui_->horizontalLayout->setDirection(QBoxLayout::TopToBottom);
  ui_->label_logo->setAlignment(Qt::AlignCenter);
  ui_->label_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  ui_->label_text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  QScrollArea *scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameShape(QFrame::NoFrame);
  ui_->layout_right->removeWidget(ui_->label_text);
  scroll_area->setWidget(ui_->label_text);
  ui_->layout_right->insertWidget(0, scroll_area, 1);
  ui_->verticalLayout->addWidget(ui_->checkbox_do_not_show_message_again);
  ui_->verticalLayout->addWidget(ui_->buttonBox);
#endif

  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

  ui_->buttonBox->button(QDialogButtonBox::Ok)->setShortcut(QKeySequence::Close);

  QObject::connect(ui_->checkbox_do_not_show_message_again, &QCheckBox::toggled, this, &MessageDialog::DoNotShowMessageAgain);

}

MessageDialog::~MessageDialog() { delete ui_; }

void MessageDialog::ShowMessage(const QString &title, const QString &message, const QIcon &icon) {

  setWindowTitle(title);

  if (!icon.isNull()) {
    const QPixmap pixmap = icon.pixmap(QSize(64, 64), devicePixelRatioF());
    ui_->label_logo->setPixmap(pixmap);
  }

  ui_->label_text->setText(message);
#ifdef Q_OS_ANDROID
  showMaximized();
#else
  ui_->label_text->adjustSize();
  adjustSize();
  setMinimumWidth(520);

  if (parent_) {
    Utilities::CenterWidgetOnScreen(Utilities::GetScreen(parent_), this);
  }

  show();
#endif

}

void MessageDialog::DoNotShowMessageAgain() {

  if (!settings_group_.isEmpty() && !do_not_show_message_again_.isEmpty()) {
    Settings s;
    s.beginGroup(settings_group_);
    s.setValue(do_not_show_message_again_, ui_->checkbox_do_not_show_message_again->isChecked());
    s.endGroup();
  }

}
