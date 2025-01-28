/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QDesktopServices>
#include <QMessageBox>

#include "filemanagerutils.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
void OpenInFileManager(const QString &path, const QUrl &url);
void OpenInFileManager(const QString &path, const QUrl &url) {

  if (!url.isLocalFile()) return;

  QProcess proc;
  proc.startCommand(u"xdg-mime query default inode/directory"_s);
  proc.waitForFinished();
  QString desktop_file = QString::fromUtf8(proc.readLine()).simplified();
  QString xdg_data_dirs = QString::fromUtf8(qgetenv("XDG_DATA_DIRS"));
  if (xdg_data_dirs.isEmpty()) {
    xdg_data_dirs = "/usr/local/share/:/usr/share/"_L1;
  }
  const QStringList data_dirs = xdg_data_dirs.split(u':');

  QString command;
  QStringList command_params;
  for (const QString &data_dir : data_dirs) {
    QString desktop_file_path = QStringLiteral("%1/applications/%2").arg(data_dir, desktop_file);
    if (!QFile::exists(desktop_file_path)) continue;
    QSettings setting(desktop_file_path, QSettings::IniFormat);
    setting.beginGroup(u"Desktop Entry"_s);
    if (setting.contains("Exec"_L1)) {
      QString cmd = setting.value(u"Exec"_s).toString();
      if (cmd.isEmpty()) break;
      static const QRegularExpression regex(u"[%][a-zA-Z]*( |$)"_s, QRegularExpression::CaseInsensitiveOption);
      cmd = cmd.remove(regex);
      command_params = cmd.split(u' ', Qt::SkipEmptyParts);
      command = command_params.first();
      command_params.removeFirst();
    }
    setting.endGroup();
    if (!command.isEmpty()) break;
  }

  if (command.startsWith("/usr/bin/"_L1)) {
    command = command.split(u'/').last();
  }

  if (command.isEmpty() || command == "exo-open"_L1) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
  else if (command.startsWith("nautilus"_L1) ||
           command.startsWith("dolphin"_L1) ||
           command.startsWith("konqueror"_L1) ||
           command.startsWith("kfmclient"_L1)) {
    proc.startDetached(command, QStringList() << command_params << u"--select"_s << url.toLocalFile());
  }
  else if (command.startsWith("caja"_L1)) {
    proc.startDetached(command, QStringList() << command_params << u"--no-desktop"_s << path);
  }
  else if (command.startsWith("pcmanfm"_L1) || command.startsWith("thunar"_L1) || command.startsWith("spacefm"_L1)) {
    proc.startDetached(command, QStringList() << command_params << path);
  }
  else {
    proc.startDetached(command, QStringList() << command_params << url.toLocalFile());
  }

}
#endif

#ifdef Q_OS_MACOS
// Better than openUrl(dirname(path)) - also highlights file at path
void RevealFileInFinder(const QString &path) {
  QProcess::execute(u"/usr/bin/open"_s, QStringList() << u"-R"_s << path);
}
#endif  // Q_OS_MACOS

#ifdef Q_OS_WIN32
void ShowFileInExplorer(const QString &path);
void ShowFileInExplorer(const QString &path) {
  QProcess::execute(u"explorer.exe"_s, QStringList() << u"/select,"_s << QDir::toNativeSeparators(path));
}
#endif

void OpenInFileBrowser(const QList<QUrl> &urls) {

  QMap<QString, QUrl> dirs;

  for (const QUrl &url : urls) {
    if (!url.isLocalFile()) {
      continue;
    }
    QString path = url.toLocalFile();
    if (!QFile::exists(path)) continue;

    const QString directory = QFileInfo(path).dir().path();
    if (dirs.contains(directory)) continue;
    dirs.insert(directory, url);
  }

  if (dirs.count() > 50) {
    QMessageBox messagebox(QMessageBox::Critical, QObject::tr("Show in file browser"), QObject::tr("Too many songs selected."));
    messagebox.exec();
    return;
  }

  if (dirs.count() > 5) {
    QMessageBox messagebox(QMessageBox::Information, QObject::tr("Show in file browser"), QObject::tr("%1 songs in %2 different directories selected, are you sure you want to open them all?").arg(urls.count()).arg(dirs.count()), QMessageBox::Open|QMessageBox::Cancel);
    messagebox.setTextFormat(Qt::RichText);
    int result = messagebox.exec();
    switch (result) {
      case QMessageBox::Open:
        break;
      case QMessageBox::Cancel:
      default:
        return;
    }
  }

  QMap<QString, QUrl>::iterator i;
  for (i = dirs.begin(); i != dirs.end(); ++i) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    OpenInFileManager(i.key(), i.value());
#elif defined(Q_OS_MACOS)
    // Revealing multiple files in the finder only opens one window, so it also makes sense to reveal at most one per directory
    RevealFileInFinder(i.value().toLocalFile());
#elif defined(Q_OS_WIN32)
    ShowFileInExplorer(i.value().toLocalFile());
#endif
  }

}

}  // namespace Utilities
