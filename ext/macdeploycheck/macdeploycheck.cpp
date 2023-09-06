/* This file is part of Strawberry.
   Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include "core/logging.h"

int main(int argc, char **argv);
int main(int argc, char **argv) {

  QCoreApplication app(argc, argv);

  logging::Init();

  qLog(Info) << "Running macdeploycheck";

  if (argc < 1) {
    qLog(Error) << "Usage: macdeploycheck <bundledir>";
    return 1;
  }
  QString bundle_path = QString::fromLocal8Bit(argv[1]);

  bool success = true;

  QDirIterator iter(bundle_path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
  while (iter.hasNext()) {

    iter.next();

    QString filepath = iter.fileInfo().filePath();

    // Ignore these files.
    if (filepath.endsWith(".plist") ||
        filepath.endsWith(".icns") ||
        filepath.endsWith(".prl") ||
        filepath.endsWith(".conf") ||
        filepath.endsWith(".h") ||
        filepath.endsWith(".nib") ||
        filepath.endsWith(".strings") ||
        filepath.endsWith(".css") ||
        filepath.endsWith("CodeResources") ||
        filepath.endsWith("PkgInfo") ||
        filepath.endsWith(".modulemap")) {
      continue;
    }

    QProcess otool;
    otool.start("otool", QStringList() << "-L" << filepath);
    otool.waitForFinished();
    if (otool.exitStatus() != QProcess::NormalExit || otool.exitCode() != 0) {
      qLog(Error) << "otool failed for" << filepath << ":" << otool.readAllStandardError();
      success = false;
      continue;
    }
    QString output = otool.readAllStandardOutput();
    QStringList output_lines = output.split("\n", Qt::SkipEmptyParts);
    if (output_lines.size() < 2) {
      qLog(Error) << "Could not parse otool output:" << output;
      success = false;
      continue;
    }
    QString first_line = output_lines.first();
    if (first_line.endsWith(':')) first_line.chop(1);
    if (first_line == filepath) {
      output_lines.removeFirst();
    }
    else {
      qLog(Error) << "First line" << first_line << "does not match" << filepath;
      success = false;
    }
    QRegularExpression regexp(QStringLiteral("^\\t(.+) \\(compatibility version (\\d+\\.\\d+\\.\\d+), current version (\\d+\\.\\d+\\.\\d+)(, weak|, reexport)?\\)$"));
    for (const QString &output_line : output_lines) {

      //qDebug() << "Final check on" << filepath << output_line;

      QRegularExpressionMatch match = regexp.match(output_line);
      if (match.hasMatch()) {
        QString library = match.captured(1);
        if (QFileInfo(library).fileName() == QFileInfo(filepath).fileName()) {  // It's this.
          continue;
        }
        else if (library.startsWith("@executable_path")) {
          QString real_path = library;
          real_path = real_path.replace("@executable_path", bundle_path + "/Contents/MacOS");
          if (!QFile::exists(real_path)) {
            qLog(Error) << real_path << "does not exist for" << filepath;
            success = false;
          }
        }
        else if (library.startsWith("@rpath")) {
          QString real_path = library;
          real_path = real_path.replace("@rpath", bundle_path + "/Contents/Frameworks");
          if (!QFile::exists(real_path) && !real_path.endsWith("QtSvg")) {  // FIXME: Ignore broken svg image plugin.
            qLog(Error) << real_path << "does not exist for" << filepath;
            success = false;
          }
        }
        else if (library.startsWith("@loader_path")) {
          QString loader_path = QFileInfo(filepath).path();
          QString real_path = library;
          real_path = real_path.replace("@loader_path", loader_path);
          if (!QFile::exists(real_path)) {
            qLog(Error) << real_path << "does not exist for" << filepath;
            success = false;
          }
        }
        else if (library.startsWith("/System/Library/") || library.startsWith("/usr/lib/")) {  // System library
          continue;
        }
        else {
          qLog(Error) << "File" << filepath << "points to" << library;
          success = false;
        }
      }
      else {
        qLog(Error) << "Could not parse otool output line:" << output_line;
        success = false;
      }
    }
  }

  return success ? 0 : 1;

}
