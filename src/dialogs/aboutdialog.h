/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QList>
#include <QString>

#include "ui_aboutdialog.h"

class QWidget;

class AboutDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AboutDialog(QWidget *parent = nullptr);

 private:
  struct Person {
    explicit Person(const QString &n, const QString &e = QString()) : name(n), email(e) {}
    bool operator<(const Person &other) const { return name < other.name; }
    QString name;
    QString email;
  };

  QString MainHtml() const;
  QString ContributorsHtml() const;
  static QString PersonToHtml(const Person &person);

 private:
  Ui::AboutDialog ui_;

  QList<Person> strawberry_authors_;
  QList<Person> strawberry_contributors_;
  QList<Person> strawberry_thanks_;
  QList<Person> clementine_authors_;
  QList<Person> clementine_contributors_;
};

#endif  // ABOUTDIALOG_H
