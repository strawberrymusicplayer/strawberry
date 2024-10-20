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

using namespace Qt::Literals::StringLiterals;

About::About(QWidget *parent) : QDialog(parent), ui_{} {

  ui_.setupUi(this);
  setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("About Strawberry"));

  strawberry_authors_ \
           << Person(u"Jonas Kvinge"_s);

  strawberry_contributors_ \
           << Person(u"Gavin D. Howard"_s)
           << Person(u"Martin Delille"_s)
           << Person(u"Roman Lebedev"_s)
           << Person(u"Daniel Ostertag"_s)
           << Person(u"Gustavo L Conte"_s);

  clementine_authors_
           << Person(u"David Sansome"_s)
           << Person(u"John Maguire"_s)
           << Person(u"Paweł Bara"_s)
           << Person(u"Arnaud Bienner"_s);

  clementine_contributors_ \
           << Person(u"Jakub Stachowski"_s)
           << Person(u"Paul Cifarelli"_s)
           << Person(u"Felipe Rivera"_s)
           << Person(u"Alexander Peitz"_s)
           << Person(u"Andreas Muttscheller"_s)
           << Person(u"Mark Furneaux"_s)
           << Person(u"Florian Bigard"_s)
           << Person(u"Alex Bikadorov"_s)
           << Person(u"Mattias Andersson"_s)
           << Person(u"Alan Briolat"_s)
           << Person(u"Arun Narayanankutty"_s)
           << Person(u"Bartłomiej Burdukiewicz"_s)
           << Person(u"Andre Siviero"_s)
           << Person(u"Santiago Gil"_s)
           << Person(u"Tyler Rhodes"_s)
           << Person(u"Vikram Ambrose"_s)
           << Person(u"David Guillen"_s)
           << Person(u"Krzysztof Sobiecki"_s)
           << Person(u"Valeriy Malov"_s)
           << Person(u"Nick Lanham"_s);

  strawberry_thanks_ \
           << Person(u"Mark Kretschmann"_s)
           << Person(u"Max Howell"_s)
           << Person(u"Artur Rona"_s)
           << Person(u"Robert-André Mauchin"_s)
           << Person(u"Thomas Pierson"_s)
           << Person(u"Fabio Loli"_s);

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
