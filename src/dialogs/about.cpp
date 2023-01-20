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

About::About(QWidget *parent) : QDialog(parent), ui_{} {

  ui_.setupUi(this);
  setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("About Strawberry"));

  strawberry_authors_ \
           << Person("Jonas Kvinge");

  strawberry_contributors_ \
           << Person("Gavin D. Howard")
           << Person("Martin Delille");

  clementine_authors_
           << Person("David Sansome")
           << Person("John Maguire")
           << Person(QString::fromUtf8("Paweł Bara"))
           << Person("Arnaud Bienner");

  clementine_contributors_ \
           << Person("Jakub Stachowski")
           << Person("Paul Cifarelli")
           << Person("Felipe Rivera")
           << Person("Alexander Peitz")
           << Person("Andreas Muttscheller")
           << Person("Mark Furneaux")
           << Person("Florian Bigard")
           << Person("Alex Bikadorov")
           << Person("Mattias Andersson")
           << Person("Alan Briolat")
           << Person("Arun Narayanankutty")
           << Person(QString::fromUtf8("Bartłomiej Burdukiewicz"))
           << Person("Andre Siviero")
           << Person("Santiago Gil")
           << Person("Tyler Rhodes")
           << Person("Vikram Ambrose")
           << Person("David Guillen")
           << Person("Krzysztof Sobiecki")
           << Person("Valeriy Malov")
           << Person("Nick Lanham");

  strawberry_thanks_ \
           << Person("Mark Kretschmann")
           << Person("Max Howell")
           << Person("Artur Rona")
           << Person("Robert-André Mauchin")
           << Person("Thomas Pierson")
           << Person("Fabio Loli");

  QFont title_font;
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 4);

  ui_.label_title->setFont(title_font);
  ui_.label_title->setText(windowTitle());
  ui_.label_text->setText(MainHtml());
  ui_.text_contributors->document()->setDefaultStyleSheet(QString("a {color: %1; }").arg(palette().text().color().name()));
  ui_.text_contributors->setText(ContributorsHtml());

  ui_.buttonBox->button(QDialogButtonBox::Close)->setShortcut(QKeySequence::Close);

}

QString About::MainHtml() const {

  QString ret;

  ret += QString("<p>");
  ret += tr("Version %1").arg(QCoreApplication::applicationVersion());
  ret += QString("</p>");

  ret += QString("<p>");
  ret += tr("Strawberry is a music player and music collection organizer.");
  ret += QString("<br />");
  ret += tr("It is a fork of Clementine released in 2018 aimed at music collectors and audiophiles.");
  ret += QString("</p>");

  ret += QString("<p>");
  ret += tr("Strawberry is free software released under GPL. The source code is available on %1").arg(QString("<a style=\"color:%1;\" href=\"https://github.com/strawberrymusicplayer/strawberry\">GitHub</a>.").arg(palette().text().color().name()));
  ret += QString("<br />");
  ret += tr("You should have received a copy of the GNU General Public License along with this program.  If not, see %1").arg(QString("<a style=\"color:%1;\" href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>").arg(palette().text().color().name()));
  ret += QString("</p>");

  ret += QString("<p>");
  ret += tr("If you like Strawberry and can make use of it, consider sponsoring or donating.");
  ret += QString("<br />");
  ret += tr("You can sponsor the author on %1. You can also make a one-time payment through %2.").arg(
    QString("<a style=\"color:%1;\" href=\"https://github.com/sponsors/jonaski\">GitHub sponsors</a>").arg(palette().text().color().name()),
    QString("<a style=\"color:%1;\" href=\"https://paypal.me/jonaskvinge\">paypal.me/jonaskvinge</a>").arg(palette().text().color().name())
  );

  ret += QString("</p>");

  return ret;

}

QString About::ContributorsHtml() const {

  QString ret;

  ret += QString("<p>");
  ret += "<b>";
  ret += tr("Author and maintainer");
  ret += "</b>";
  for (const Person &person : strawberry_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>");
  ret += "<b>";
  ret += tr("Contributors");
  ret += "</b>";
  for (const Person &person : strawberry_contributors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>");
  ret += "<b>";
  ret += tr("Clementine authors");
  ret += "</b>";
  for (const Person &person : clementine_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>");
  ret += "<b>";
  ret += tr("Clementine contributors");
  ret += "</b>";
  for (const Person &person : clementine_contributors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>");
  ret += "<b>";
  ret += tr("Thanks to");
  ret += "</b>";
  for (const Person &person : strawberry_thanks_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>");
  ret += tr("Thanks to all the other Amarok and Clementine contributors.");
  ret += QString("</p>");
  return ret;

}

QString About::PersonToHtml(const Person &person) {

  if (person.email.isEmpty()) {
    return person.name;
  }
  else {
    return QString("%1 &lt;<a href=\"mailto:%2\">%3</a>&gt;").arg(person.name, person.email, person.email);
  }
}
