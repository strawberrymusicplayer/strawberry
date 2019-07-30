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
           << Person("Jonas Kvinge", "jonas@jkvinge.net");

  strawberry_constributors_ \
           << Person("Gavin D. Howard", "yzena.tech@gmail.com")
           << Person("Martin Delille", "martin@lylo.tv");

  strawberry_thanks_ \
           << Person("Robert-André Mauchin", "eclipseo@fedoraproject.org")
           << Person("Thomas Pierson", "contact@thomaspierson.fr")
           << Person("Fabio Loli", "fabio.lolix@gmail.com");

  clementine_authors_
           << Person("David Sansome", "me@davidsansome.com")
           << Person("John Maguire", "john.maguire@gmail.com")
           << Person(QString::fromUtf8("Paweł Bara"), "keirangtp@gmail.com")
           << Person("Arnaud Bienner", "arnaud.bienner@gmail.com");

  clementine_constributors_ \
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

  QString Title(tr("About Strawberry"));

  QFont title_font;
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 4);

  setWindowTitle(Title);

  ui_.label_title->setFont(title_font);
  ui_.label_title->setText(Title);

  ui_.label_text->setText(MainHtml());
  ui_.text_constributors->setText(ContributorsHtml());
  ui_.text_constributors->updateGeometry();
  updateGeometry();

  ui_.buttonBox->button(QDialogButtonBox::Close)->setShortcut(QKeySequence::Close);

}

QString About::MainHtml() const {

  QString ret;

  ret = tr("<p>Version %1</p>").arg(QCoreApplication::applicationVersion());

  ret += "<p>";
  ret += tr("Strawberry is a music player and music collection organizer.<br />");
  ret += tr("It is a fork of Clementine released in 2018 aimed at music collectors, audio enthusiasts and audiophiles.<br />");
  ret += tr("The name is inspired by the band Strawbs. It's based on a heavily modified version of Clementine created in 2012-2013. It's written in C++ and Qt 5.");
  ret += "</p>";
  ret += "<p>";
  ret += tr("Strawberry is free software: you can redistribute it and/or modify<br />");
  ret += tr("it under the terms of the GNU General Public License as published by<br />");
  ret += tr("the Free Software Foundation, either version 3 of the License, or<br />");
  ret += tr("(at your option) any later version.<br />");
  ret += "<br />";
  ret += tr("Strawberry is distributed in the hope that it will be useful,<br />");
  ret += tr("but WITHOUT ANY WARRANTY; without even the implied warranty of<br />");
  ret += tr("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the<br />");
  ret += tr("GNU General Public License for more details.<br />");
  ret += "<br />";
  ret += tr("You should have received a copy of the GNU General Public License<br />");
  ret += tr("along with Strawberry.  If not, see <a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>.");
  ret += "</p>";

  return ret;

}

QString About::ContributorsHtml() const {

  QString ret;

  ret += tr("<p><b>Strawberry authors</b>");
  for (const Person &person : strawberry_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += "</p>";

  ret += tr("<p><b>Strawberry contributors</b>");
  for (const Person &person : strawberry_constributors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += "</p>";

  ret += tr("<p><b>Thanks to</b>");
  for (const Person &person : strawberry_thanks_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += "</p>";

  ret += tr("<p><b>Clementine authors</b>");
  for (const Person &person : clementine_authors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += "</p>";

  ret += tr("<p><b>Clementine contributors</b>");
  for (const Person &person : clementine_constributors_) {
    ret += "<br />" + PersonToHtml(person);
  }
  ret += "</p>";

  ret += tr("<p>Thanks to all the other Amarok and Clementine contributors.</p>");
  return ret;

}

QString About::PersonToHtml(const Person &person) const {

  if (person.email.isNull())
    return person.name;
  else
    return QString("%1 &lt;<a href=\"mailto:%2\">%3</a>&gt;").arg(person.name, person.email, person.email);
}
