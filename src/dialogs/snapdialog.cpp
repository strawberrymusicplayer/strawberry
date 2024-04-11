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

#include "core/logging.h"
#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "utilities/screenutils.h"

#include "snapdialog.h"
#include "ui_messagedialog.h"

SnapDialog::SnapDialog(QWidget *parent) : MessageDialog(parent) {

  setWindowTitle(tr("Strawberry is running as a Snap"));

  const QIcon icon = IconLoader::Load(QStringLiteral("dialog-warning"));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  const QPixmap pixmap = icon.pixmap(QSize(64, 64), devicePixelRatioF());
#else
  const QPixmap pixmap = icon.pixmap(QSize(64, 64));
#endif
  ui_->label_logo->setPixmap(pixmap);

  QString text;
  text += QStringLiteral("<p>");
  text += tr("It is detected that Strawberry is running as a Snap");
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += tr("Strawberry is slower, and has restrictions when running as a Snap. Accessing the root filesystem (/) will not work. There also might be other restrictions such as accessing certain devices or network shares.");
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += QStringLiteral("Strawberry is available natively in the official package repositories for Fedora, openSUSE, Mageia, Arch, Manjaro, MX Linux and most other popular Linux distributions.");
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += tr("For Ubuntu there is an official PPA repository available at %1.").arg(QStringLiteral("<a style=\"color:%1;\" href=\"https://launchpad.net/~jonaski/+archive/ubuntu/strawberry\">https://launchpad.net/~jonaski/+archive/ubuntu/strawberry</a>").arg(palette().text().color().name()));
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += tr("Official releases are available for Debian and Ubuntu which also work on most of their derivatives. See %1 for more information.").arg(QStringLiteral("<a style=\"color:%1;\" href=\"https://www.strawberrymusicplayer.org/\">https://www.strawberrymusicplayer.org/</a>").arg(palette().text().color().name()));
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += tr("For a better experience please consider the other options above.");
  text += QStringLiteral("</p>");

  text += QStringLiteral("<p>");
  text += tr("Copy your strawberry.conf and strawberry.db from your ~/snap directory to avoid losing configuration before you uninstall the snap:");
  text += QStringLiteral("<br />");
  text += QStringLiteral("cp ~/snap/strawberry/current/.config/strawberry/strawberry.conf ~/.config/strawberry/strawberry.conf<br />");
  text += QStringLiteral("cp ~/snap/strawberry/current/.local/share/strawberry/strawberry/strawberry.db ~/.local/share/strawberry/strawberry/strawberry.db<br />");
  text += QStringLiteral("</p>");
  text += QStringLiteral("<p>");
  text += tr("Uninstall the snap with:");
  text += QStringLiteral("<br />");
  text += QStringLiteral("snap remove strawberry");
  text += QStringLiteral("</p>");
  text += QStringLiteral("<p>");
  text += tr("Install strawberry through PPA:");
  text += QStringLiteral("<br />");
  text += QStringLiteral("sudo add-apt-repository ppa:jonaski/strawberry<br />");
  text += QStringLiteral("sudo apt-get update<br />");
  text += QStringLiteral("sudo apt install strawberry");
  text += QStringLiteral("</p>");
  text += QStringLiteral("<p></p>");

  ui_->label_text->setText(text);
  ui_->label_text->adjustSize();
  adjustSize();

  settings_group_ = QLatin1String(MainWindow::kSettingsGroup);
  do_not_show_message_again_ = QStringLiteral("ignore_snap");

  if (parent) {
    Utilities::CenterWidgetOnScreen(Utilities::GetScreen(parent), this);
  }

}
