/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QCoreApplication>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QString>
#include <QFlags>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>
#include <QTextBrowser>

#include "about.h"
#include "ui_about.h"

using namespace Qt::StringLiterals;

About::About(QWidget *parent) : QDialog(parent), ui_{} {

  ui_.setupUi(this);
  setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("About Strawberry"));

  strawberry_authors_ \
           << Person(QStringLiteral("Jonas Kvinge"));

  strawberry_contributors_ \
           << Person(QStringLiteral("Gavin D. Howard"))
           << Person(QStringLiteral("Martin Delille"))
           << Person(QStringLiteral("Roman Lebedev"))
           << Person(QStringLiteral("Daniel Ostertag"))
           << Person(QStringLiteral("Gustavo L Conte"));

  clementine_authors_
           << Person(QStringLiteral("David Sansome"))
           << Person(QStringLiteral("John Maguire"))
           << Person(QStringLiteral("Paweł Bara"))
           << Person(QStringLiteral("Arnaud Bienner"));

  clementine_contributors_ \
           << Person(QStringLiteral("Jakub Stachowski"))
           << Person(QStringLiteral("Paul Cifarelli"))
           << Person(QStringLiteral("Felipe Rivera"))
           << Person(QStringLiteral("Alexander Peitz"))
           << Person(QStringLiteral("Andreas Muttscheller"))
           << Person(QStringLiteral("Mark Furneaux"))
           << Person(QStringLiteral("Florian Bigard"))
           << Person(QStringLiteral("Alex Bikadorov"))
           << Person(QStringLiteral("Mattias Andersson"))
           << Person(QStringLiteral("Alan Briolat"))
           << Person(QStringLiteral("Arun Narayanankutty"))
           << Person(QStringLiteral("Bartłomiej Burdukiewicz"))
           << Person(QStringLiteral("Andre Siviero"))
           << Person(QStringLiteral("Santiago Gil"))
           << Person(QStringLiteral("Tyler Rhodes"))
           << Person(QStringLiteral("Vikram Ambrose"))
           << Person(QStringLiteral("David Guillen"))
           << Person(QStringLiteral("Krzysztof Sobiecki"))
           << Person(QStringLiteral("Valeriy Malov"))
           << Person(QStringLiteral("Nick Lanham"));

  strawberry_thanks_ \
           << Person(QStringLiteral("Mark Kretschmann"))
           << Person(QStringLiteral("Max Howell"))
           << Person(QStringLiteral("Artur Rona"))
           << Person(QStringLiteral("Robert-André Mauchin"))
           << Person(QStringLiteral("Thomas Pierson"))
           << Person(QStringLiteral("Fabio Loli"));

  QFont title_font;
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 4);

  ui_.label_title->setFont(title_font);
  ui_.label_title->setText(windowTitle());
  ui_.label_text->setText(MainHtml());
  ui_.text_contributors->document()->setDefaultStyleSheet(QStringLiteral("a {color: %1; }").arg(palette().text().color().name()));
  ui_.text_contributors->setText(ContributorsHtml());

  ui_.buttonBox->button(QDialogButtonBox::Close)->setShortcut(QKeySequence::Close);

}

QString About::MainHtml() const {

  QString ret;

  ret += "<p>"_L1;
  ret += tr("Version %1").arg(QCoreApplication::applicationVersion());
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += tr("Strawberry is a music player and music collection organizer.");
  ret += "<br />"_L1;
  ret += tr("It is a fork of Clementine released in 2018 aimed at music collectors and audiophiles.");
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += tr("Strawberry is free software released under GPL. The source code is available on %1").arg(QStringLiteral("<a style=\"color:%1;\" href=\"https://github.com/strawberrymusicplayer/strawberry\">GitHub</a>.").arg(palette().text().color().name()));
  ret += "<br />"_L1;
  ret += tr("You should have received a copy of the GNU General Public License along with this program.  If not, see %1").arg(QStringLiteral("<a style=\"color:%1;\" href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>").arg(palette().text().color().name()));
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += tr("If you like Strawberry and can make use of it, consider sponsoring or donating.");
  ret += "<br />"_L1;
  ret += tr("You can sponsor the author on %1. You can also make a one-time payment through %2.").arg(
    QStringLiteral("<a style=\"color:%1;\" href=\"https://github.com/sponsors/jonaski\">GitHub sponsors</a>").arg(palette().text().color().name()),
    QStringLiteral("<a style=\"color:%1;\" href=\"https://paypal.me/jonaskvinge\">paypal.me/jonaskvinge</a>").arg(palette().text().color().name())
  );

  ret += "</p>"_L1;

  return ret;

}

QString About::ContributorsHtml() const {

  QString ret;

  ret += "<p>"_L1;
  ret += "<b>"_L1;
  ret += tr("Author and maintainer");
  ret += "</b>"_L1;
  for (const Person &person : strawberry_authors_) {
    ret += "<br />"_L1 + PersonToHtml(person);
  }
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += "<b>"_L1;
  ret += tr("Contributors");
  ret += "</b>"_L1;
  for (const Person &person : strawberry_contributors_) {
    ret += "<br />"_L1 + PersonToHtml(person);
  }
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += "<b>"_L1;
  ret += tr("Clementine authors");
  ret += "</b>"_L1;
  for (const Person &person : clementine_authors_) {
    ret += "<br />"_L1 + PersonToHtml(person);
  }
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += "<b>"_L1;
  ret += tr("Clementine contributors");
  ret += "</b>"_L1;
  for (const Person &person : clementine_contributors_) {
    ret += "<br />"_L1 + PersonToHtml(person);
  }
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += "<b>"_L1;
  ret += tr("Thanks to");
  ret += "</b>"_L1;
  for (const Person &person : strawberry_thanks_) {
    ret += "<br />"_L1 + PersonToHtml(person);
  }
  ret += "</p>"_L1;

  ret += "<p>"_L1;
  ret += tr("Thanks to all the other Amarok and Clementine contributors.");
  ret += "</p>"_L1;
  return ret;

}

QString About::PersonToHtml(const Person &person) {

  if (person.email.isEmpty()) {
    return person.name;
  }

  return QStringLiteral("%1 &lt;<a href=\"mailto:%2\">%3</a>&gt;").arg(person.name, person.email, person.email);

}
