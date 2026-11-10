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

TemporaryResource::TemporaryResource(const QString &filename, QObject *parent) : QFile(parent) {

  // qFatal() is used instead of Q_ASSERT(), so a resource that can't be copied also stops the tests in release builds.
  QFile resource(filename);
  if (!resource.open(QIODevice::ReadOnly)) {
    qFatal("Could not open %s: %s", qUtf8Printable(filename), qUtf8Printable(resource.errorString()));
  }

  QTemporaryFile temporary_file(QDir::tempPath() + u"/strawberry_test-XXXXXX."_s + filename.section(u'.', -1, -1));
  temporary_file.setAutoRemove(false);
  if (!temporary_file.open()) {
    qFatal("Could not create a temporary file for %s: %s", qUtf8Printable(filename), qUtf8Printable(temporary_file.errorString()));
  }
  if (temporary_file.write(resource.readAll()) != resource.size() || !temporary_file.flush()) {
    qFatal("Could not write %s to %s: %s", qUtf8Printable(filename), qUtf8Printable(temporary_file.fileName()), qUtf8Printable(temporary_file.errorString()));
  }
  setFileName(temporary_file.fileName());

}

TemporaryResource::~TemporaryResource() {
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
