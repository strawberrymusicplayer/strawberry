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

#include "test_utils.h"

#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QTemporaryFile>
#include <QNetworkRequest>
#include <QVariant>
#include <QString>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

std::ostream &operator<<(std::ostream &stream, const QString &str) {
  stream << str.toStdString();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const QUrl &url) {
  stream << url.toString().toStdString();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const QNetworkRequest &req) {
  stream << req.url().toString().toStdString();
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const QVariant &var) {
  stream << var.toString().toStdString();
  return stream;
}

void PrintTo(const ::QString &str, std::ostream &os) {
  os << str.toStdString();
}

void PrintTo(const ::QVariant &var, std::ostream &os) {
  os << var.toString().toStdString();
}

void PrintTo(const ::QUrl &url, std::ostream &os) {
  os << url.toString().toStdString();
}

namespace {

// Writes the data to a new temporary file and returns the filename, the QTemporaryFile is destroyed before returning, so the file is closed.
// qFatal() is used instead of Q_ASSERT(), so a file that can't be written also stops the tests in release builds.
QString WriteTemporaryFile(const QString &extension, const QByteArray &data, const QString &description) {

  QTemporaryFile temporary_file(QDir::tempPath() + u"/strawberry_test-XXXXXX."_s + extension);
  temporary_file.setAutoRemove(false);
  if (!temporary_file.open()) {
    qFatal("Could not create a temporary file for %s: %s", qUtf8Printable(description), qUtf8Printable(temporary_file.errorString()));
  }
  if (temporary_file.write(data) != data.size() || !temporary_file.flush()) {
    qFatal("Could not write %s to %s: %s", qUtf8Printable(description), qUtf8Printable(temporary_file.fileName()), qUtf8Printable(temporary_file.errorString()));
  }

  return temporary_file.fileName();

}

}  // namespace

TemporaryResource::TemporaryResource(const QString &filename, QObject *parent) : QFile(parent) {

  QFile resource(filename);
  if (!resource.open(QIODevice::ReadOnly)) {
    qFatal("Could not open %s: %s", qUtf8Printable(filename), qUtf8Printable(resource.errorString()));
  }

  setFileName(WriteTemporaryFile(filename.section(u'.', -1, -1), resource.readAll(), filename));

}

TemporaryResource::~TemporaryResource() {
  remove();
}

TemporaryDataFile::TemporaryDataFile(const QString &extension, const QByteArray &data, QObject *parent) : QFile(parent) {

  setFileName(WriteTemporaryFile(extension, data, u"data"_s));

}

TemporaryDataFile::~TemporaryDataFile() {
  remove();
}

TestQObject::TestQObject(QObject *parent)
    : QObject(parent),
      invoked_(0) {
}

void TestQObject::Emit() {
  Q_EMIT Emitted();
}

void TestQObject::Invoke() {
  ++invoked_;
}

TestUrlHandler::TestUrlHandler(QObject *parent) : QObject(parent) {}

void TestUrlHandler::OpenUrl(const QUrl &url) {
  urls_ << url;
}
