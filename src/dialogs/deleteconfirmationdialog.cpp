/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QStringList>
#include <QStyle>
#include <QFont>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QAbstractItemView>
#include <QListWidget>
#include <QLabel>

#include "utilities/screenutils.h"

#include "deleteconfirmationdialog.h"

DeleteConfirmationDialog::DeleteConfirmationDialog(const QStringList &files, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint),
      button_box_(new QDialogButtonBox(this)) {

  setModal(true);
  setWindowTitle(tr("Delete files"));
  setWindowIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this));

  QLabel *label_icon = new QLabel(this);
  label_icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this).pixmap(style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this), style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this)));
  label_icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QLabel *label_text_top = new QLabel(this);
  QFont label_text_top_font = label_text_top->font();
  label_text_top_font.setBold(true);
  label_text_top_font.setPointSize(label_text_top_font.pointSize() + 4);
  label_text_top->setTextInteractionFlags(Qt::TextInteractionFlags(style()->styleHint(QStyle::SH_MessageBox_TextInteractionFlags, nullptr, this)));
  label_text_top->setContentsMargins(0, 0, 0, 0);
  label_text_top->setFont(label_text_top_font);
  label_text_top->setText(tr("The following files will be deleted from disk:"));

  QListWidget *list = new QListWidget(this);
  list->setSelectionMode(QAbstractItemView::NoSelection);
  list->addItems(files);

  QLabel *label_text_bottom = new QLabel(this);
  QFont label_text_bottom_font = label_text_bottom->font();
  label_text_bottom_font.setBold(true);
  label_text_bottom_font.setPointSize(label_text_bottom_font.pointSize() + 4);
  label_text_bottom->setTextInteractionFlags(Qt::TextInteractionFlags(style()->styleHint(QStyle::SH_MessageBox_TextInteractionFlags, nullptr, this)));
  label_text_bottom->setContentsMargins(0, 0, 0, 0);
  label_text_bottom->setFont(label_text_bottom_font);
  label_text_bottom->setText(tr("Are you sure you want to continue?"));

  button_box_->setStandardButtons(QDialogButtonBox::Yes|QDialogButtonBox::Cancel);
  QObject::connect(button_box_, &QDialogButtonBox::clicked, this, &DeleteConfirmationDialog::ButtonClicked);

  // Add layout
  QGridLayout *grid = new QGridLayout(this);
  grid->addWidget(label_icon,        0, 0, 2, 1, Qt::AlignTop);
  grid->addWidget(label_text_top,    0, 1, 1, 1);
  grid->addWidget(list,              1, 1, 1, 2);
  grid->addWidget(label_text_bottom, 2, 1, 1, 2);
  grid->addWidget(button_box_,       3, 1, 1, 2, Qt::AlignRight);
  grid->setSizeConstraint(QLayout::SetNoConstraint);
  setLayout(grid);

  // Set size of dialog
  int max_width = 0;
  int max_height = 0;
  QScreen *screen = Utilities::GetScreen(this);
  if (screen) {
    max_width = static_cast<int>(screen->geometry().size().width() / 0.5);
    max_height = static_cast<int>(static_cast<float>(screen->geometry().size().height()) / static_cast<float>(1.5));
  }
  int min_width = std::min(list->sizeHintForColumn(0) + 100, max_width);
  int min_height = std::min((list->sizeHintForRow(0) * list->count()) + 160, max_height);
  setMinimumSize(min_width, min_height);
  adjustSize();
  setMinimumSize(0, 0);

}

void DeleteConfirmationDialog::ButtonClicked(QAbstractButton *button) {

  done(button_box_->standardButton(button));

}

QDialogButtonBox::StandardButton DeleteConfirmationDialog::warning(const QStringList &files, QWidget *parent) {

  DeleteConfirmationDialog box(files, parent);
  return static_cast<QDialogButtonBox::StandardButton>(box.exec());

}
