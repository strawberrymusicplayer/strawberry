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

#include "config.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QString>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>
#include <QSettings>

#include "snapdialog.h"
#include "ui_snapdialog.h"

#include "core/mainwindow.h"

SnapDialog::SnapDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_SnapDialog) {

  ui_->setupUi(this);
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("Strawberry is running as a Snap"));

  QString text;
  text += QString("<p>");
  text += tr("It is detected that Strawberry is running as a Snap");
  text += QString("</p>");

  text += QString("<p>");
  text += tr("Strawberry is slower, and has restrictions when running as a Snap. Accessing the root filesystem (/) will not work. There also might be other restrictions such as accessing certain devices or network shares.");
  text += QString("</p>");

  text += QString("<p>");
  text += QString("Strawberry is available natively in the official package repositories for Fedora, openSUSE, Mageia, Arch, Manjaro, MX Linux and most other popular Linux distributions.");
  text += QString("</p>");

  text += QString("<p>");
  text += tr("For Ubuntu there is an official PPA repository available at %1.").arg(QString("<a style=\"color:%1;\" href=\"https://launchpad.net/~jonaski/+archive/ubuntu/strawberry\">https://launchpad.net/~jonaski/+archive/ubuntu/strawberry</a>").arg(palette().text().color().name()));
  text += QString("</p>");

  text += QString("<p>");
  text += tr("Official releases are available for Debian and Ubuntu which also work on most of their derivatives. See %1 for more information.").arg(QString("<a style=\"color:%1;\" href=\"https://www.strawberrymusicplayer.org/\">https://www.strawberrymusicplayer.org/</a>").arg(palette().text().color().name()));
  text += QString("</p>");

  text += QString("<p>");
  text += tr("For a better experience please consider the other options above.");
  text += QString("</p>");

  text += QString("<p>");
  text += tr("Copy your strawberry.conf and strawberry.db from your ~/snap directory to avoid losing configuration before you uninstall the snap:");
  text += QString("<br />");
  text += QString("cp ~/snap/strawberry/current/.config/strawberry/strawberry.conf ~/.config/strawberry/strawberry.conf<br />");
  text += QString("cp ~/snap/strawberry/current/.local/share/strawberry/strawberry/strawberry.db ~/.local/share/strawberry/strawberry/strawberry.db<br />");
  text += QString("</p>");
  text += QString("<p>");
  text += tr("Uninstall the snap with:");
  text += QString("<br />");
  text += QString("snap remove strawberry");
  text += QString("</p>");
  text += QString("<p>");
  text += tr("Install strawberry through PPA:");
  text += QString("<br />");
  text += QString("sudo add-apt-repository ppa:jonaski/strawberry<br />");
  text += QString("sudo apt-get update<br />");
  text += QString("sudo apt install strawberry");
  text += QString("</p>");
  text += QString("<p></p>");

  ui_->label_text->setText(text);
  ui_->label_text->adjustSize();
  adjustSize();

  ui_->buttonBox->button(QDialogButtonBox::Ok)->setShortcut(QKeySequence::Close);

  QObject::connect(ui_->checkbox_do_not_show_message_again, &QCheckBox::toggled, this, &SnapDialog::DoNotShowMessageAgain);

}

SnapDialog::~SnapDialog() { delete ui_; }

void SnapDialog::DoNotShowMessageAgain() {

  QSettings s;
  s.beginGroup(MainWindow::kSettingsGroup);
  s.setValue("ignore_snap", ui_->checkbox_do_not_show_message_again->isChecked());
  s.endGroup();

}
