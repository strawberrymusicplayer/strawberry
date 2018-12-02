/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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
#include <QStringBuilder>
#include <QFlags>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>

#include "about.h"
#include "ui_about.h"

About::About(QWidget *parent):QDialog(parent) {

  ui_.setupUi(this);
  setWindowFlags(this->windowFlags()|Qt::WindowStaysOnTopHint);

  strawberry_authors_ \
           << Person("Jonas Kvinge", "jonas@strawbs.net");

  clementine_authors_
           << Person("David Sansome", "me@davidsansome.com")
           << Person("John Maguire", "john.maguire@gmail.com")
           << Person(QString::fromUtf8("Paweł Bara"), "keirangtp@gmail.com")
           << Person("Arnaud Bienner", "arnaud.bienner@gmail.com");

 constributors_ \
           << Person("Mark Kretschmann", "kretschmann@kde.org")
           << Person("Max Howell", "max.howell@methylblue.com")
           << Person("Jakub Stachowski", "qbast@go2.pl")
           << Person("Paul Cifarelli", "paul@cifarelli.net")
           << Person("Felipe Rivera", "liebremx@users.sourceforge.net")
           << Person("Alexander Peitz")
           << Person("Artur Rona", "artur.rona@gmail.com")
           << Person("Andreas Muttscheller", "asfa194@gmail.com")
           << Person("Mark Furneaux", "mark@furneaux.ca")
           << Person("Florian Bigard", "florian.bigard@gmail.com")
           << Person("Alex Bikadorov", "wegwerf@abwesend.de")
           << Person("Mattias Andersson", "mandersson444@gmail.com")
           << Person("Alan Briolat", "alan.briolat@gmail.com")
           << Person("Arun Narayanankutty", "n.arun.lifescience@gmail.com")
           << Person(QString::fromUtf8("Bartłomiej Burdukiewicz"), "dev.strikeu@gmail.com")
           << Person("Andre Siviero", "altsiviero@gmail.com")
           << Person("Santiago Gil")
           << Person("Tyler Rhodes", "tyler.s.rhodes@gmail.com")
           << Person("Vikram Ambrose", "ambroseworks@gmail.com")
           << Person("David Guillen", "david@davidgf.net")
           << Person("Krzysztof Sobiecki", "sobkas@gmail.com")
           << Person("Valeriy Malov", "jazzvoid@gmail.com")
           << Person("Nick Lanham", "nick@afternight.org");

  QString Title("About Strawberry");

  QFont title_font;
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 4);

  setWindowTitle(Title);

  ui_.label_title->setFont(title_font);
  ui_.label_title->setText(Title);

  ui_.label_text->setText(MainHtml());
  ui_.text_constributors->setText(ContributorsHtml());

  ui_.buttonBox->button(QDialogButtonBox::Close)->setShortcut(QKeySequence::Close);

}

QString About::MainHtml() const {

  QString ret;

  ret = QString("<p>Version %1</p>").arg(QCoreApplication::applicationVersion());

  ret += QString("<p>");
  ret += QString("Strawberry is a audio player and music collection organizer.<br />");
  ret += QString("It is a fork of Clementine released in 2018 aimed at music collectors, audio enthusiasts and audiophiles.<br />");
  ret += QString("The name is inspired by the band Strawbs. It's based on a heavily modified version of Clementine created in 2012-2013. It's written in C++ and Qt 5.");
  ret += QString("</p>");
  //ret += QString("<p>Website: <a href=\"http://www.strawbs.org/licenses/\">http://www.strawbs.org/</a></p>");
  ret += QString("<p>");
  ret += QString("Strawberry is free software: you can redistribute it and/or modify<br />");
  ret += QString("it under the terms of the GNU General Public License as published by<br />");
  ret += QString("the Free Software Foundation, either version 3 of the License, or<br />");
  ret += QString("(at your option) any later version.<br />");
  ret += QString("<br />");
  ret += QString("Strawberry is distributed in the hope that it will be useful,<br />");
  ret += QString("but WITHOUT ANY WARRANTY; without even the implied warranty of<br />");
  ret += QString("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the<br />");
  ret += QString("GNU General Public License for more details.<br />");
  ret += QString("<br />");
  ret += QString("You should have received a copy of the GNU General Public License<br />");
  ret += QString("along with Strawberry.  If not, see <a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>.");
  ret += QString("</p>");

  return ret;

}

QString About::ContributorsHtml() const {

  QString ret;

  ret += QString("<p><b>Strawberry Authors</b>");
  for (const Person &person : strawberry_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p><b>Clementine Authors</b>");
  for (const Person &person : clementine_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p><b>Clementine Contributors</b>");
  for (const Person &person : constributors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += QString("</p>");

  ret += QString("<p>... and all the Amarok and Clementine contributors</p>");
  return ret;

}

QString About::PersonToHtml(const Person &person) const {

  if (person.email.isNull())
    return person.name;
  else
    return QString("%1 &lt;<a href=\"mailto:%2\">%3</a>&gt;").arg(person.name, person.email, person.email);
}
