/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QStandardPaths>
#include <QProcess>
#include <QDesktopServices>
#include <QMessageBox>

#include "filemanagerutils.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
namespace {

constexpr int kXdgMimeTimeoutMs = 5000;

struct FileManagerCommand {
  QString program;
  QStringList params;
};

// Returns the program and parameters of the default file manager from its desktop file, or an empty program if it is not found.
FileManagerCommand DefaultFileManagerCommand() {

  QProcess proc;
  proc.startCommand(u"xdg-mime query default inode/directory"_s);
  if (!proc.waitForFinished(kXdgMimeTimeoutMs)) {
    qLog(Error) << "Could not get the default file manager with xdg-mime" << proc.errorString();
    proc.kill();
    proc.waitForFinished();
    return FileManagerCommand();
  }
  const QString desktop_file = QString::fromUtf8(proc.readLine()).simplified();
  if (desktop_file.isEmpty()) return FileManagerCommand();

  const QStringList data_dirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (const QString &data_dir : data_dirs) {
    const QString desktop_file_path = QStringLiteral("%1/applications/%2").arg(data_dir, desktop_file);
    if (!QFile::exists(desktop_file_path)) continue;
    QSettings setting(desktop_file_path, QSettings::IniFormat);
    setting.beginGroup(u"Desktop Entry"_s);
    if (!setting.contains("Exec"_L1)) continue;
    // Field codes such as %U are removed, the file or directory to show is added depending on the file manager.
    static const QRegularExpression regex(u"[%][a-zA-Z]*( |$)"_s, QRegularExpression::CaseInsensitiveOption);
    QStringList params = setting.value(u"Exec"_s).toString().remove(regex).split(u' ', Qt::SkipEmptyParts);
    // An Exec with only field codes or spaces has no program.
    if (params.isEmpty()) return FileManagerCommand();
    const QString program = params.takeFirst();
    return FileManagerCommand{ program, params };
  }

  return FileManagerCommand();

}

void OpenDirectoryWithDesktopServices(const QString &path) {

  QDesktopServices::openUrl(QUrl::fromLocalFile(path));

}

void OpenInFileManager(const FileManagerCommand &file_manager, const QString &path, const QUrl &url) {

  if (!url.isLocalFile()) return;

  // The file manager is recognized by the file name of the program, so it is also recognized when the program is not in /usr/bin.
  const QString program_name = QFileInfo(file_manager.program).fileName();

  QStringList arguments = file_manager.params;
  if (program_name.startsWith("nautilus"_L1) ||
      program_name.startsWith("dolphin"_L1) ||
      program_name.startsWith("konqueror"_L1)) {
    arguments << u"--select"_s << url.toLocalFile();
  }
  else if (program_name.startsWith("caja"_L1)) {
    arguments << u"--no-desktop"_s << path;
  }
  else if (program_name.startsWith("pcmanfm"_L1) || program_name.startsWith("thunar"_L1) || program_name.startsWith("spacefm"_L1)) {
    arguments << path;
  }
  else {
    // Without a file manager, or with a file manager which is not known, like exo-open, open the directory with the default application for directories.
    // kfmclient also opens the directory this way, since it has no option to select a file, and exits when given an unknown option.
    OpenDirectoryWithDesktopServices(path);
    return;
  }

  if (!QProcess::startDetached(file_manager.program, arguments)) {
    qLog(Error) << "Could not start file manager" << file_manager.program;
    OpenDirectoryWithDesktopServices(path);
  }

}

}  // namespace
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

  if (dirs.isEmpty()) return;

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  // The default file manager is the same for all directories, and getting it starts a process, so get it only once.
  const FileManagerCommand file_manager = DefaultFileManagerCommand();
#endif

  QMap<QString, QUrl>::iterator i;
  for (i = dirs.begin(); i != dirs.end(); ++i) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    OpenInFileManager(file_manager, i.key(), i.value());
#elif defined(Q_OS_MACOS)
    // Revealing multiple files in the finder only opens one window, so it also makes sense to reveal at most one per directory
    RevealFileInFinder(i.value().toLocalFile());
#elif defined(Q_OS_WIN32)
    ShowFileInExplorer(i.value().toLocalFile());
#endif
  }

}

}  // namespace Utilities
