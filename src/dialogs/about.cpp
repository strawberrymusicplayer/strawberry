/*
 * Strawberry Music Player
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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
    
  authors_ \
           << Person("Jonas Kvinge", "jonas@strawbs.net");
  
  clementine_authors_
           << Person("David Sansome", "me@davidsansome.com")
           << Person("John Maguire", "john.maguire@gmail.com")
           << Person(QString::fromUtf8("Paweł Bara"), "keirangtp@gmail.com")
           << Person("Arnaud Bienner", "arnaud.bienner@gmail.com");

 thanks_to_ \
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
           << Person("Tyler Rhodes", "tyler.s.rhodes@gmail.com");


  QString Title = "";

  ui_.setupUi(this);
  setWindowFlags(this->windowFlags()|Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("About Strawberry"));
  
  Title = QString("About Strawberry");
  
  ui_.title->setText(Title);

  QFont title_font;
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 4);
  ui_.title->setFont(title_font);

  ui_.text->setWordWrap(true);
  ui_.text->setText(MakeHtml());

  ui_.buttonBox->button(QDialogButtonBox::Close)->setShortcut(QKeySequence::Close);

}

QString About::MakeHtml() const {

  QString ret = "";

  ret = tr("<p>Version %1</p>").arg(QCoreApplication::applicationVersion());

  ret += tr("<p>");

  ret += tr("Strawberry is a audio player and music collection organizer.<br />");
  ret += tr("It's based on Clementine and Amarok 1.4, especially aimed at audiophiles.<br />");
  ret += tr("The name is inspired by the band Strawbs.</p>");

  //ret += tr("<p><a href=\"%1\">%2</a></p><p><b>%3:</b>").arg(kUrl, kUrl, tr("Authors"));

  ret += QString("<p><b>%1</b>").arg(tr("Strawberry Authors"));

  for (const Person &person : authors_) {
    ret += "<br />" + MakeHtml(person);
  }
  
  ret += QString("</p><p><b>%3:</b>").arg(tr("Clementine Authors"));

  for (const Person &person : clementine_authors_) {
    ret += "<br />" + MakeHtml(person);
  }

  ret += QString("</p><p><b>%3:</b>").arg(tr("Thanks to"));

  for (const Person &person : thanks_to_) {
    ret += "<br />" + MakeHtml(person);
  }

  ret += QString("<br />%1</p>").arg(tr("... and all the Amarok and Clementine contributors"));

  return ret;

}

QString About::MakeHtml(const Person &person) const {

  if (person.email.isNull())
    return person.name;
  else
    return QString("%1 &lt;<a href=\"mailto:%2\">%3</a>&gt;").arg(person.name, person.email, person.email);
}
