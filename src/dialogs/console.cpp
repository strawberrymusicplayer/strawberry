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

#include "config.h"

#include <QWidget>
#include <QDialog>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>

#include "console.h"
#include "core/application.h"
#include "core/database.h"

Console::Console(Application *app, QWidget *parent) : QDialog(parent), app_(app) {

  ui_.setupUi(this);

  setWindowFlags(windowFlags()|Qt::WindowMaximizeButtonHint);

  connect(ui_.run, SIGNAL(clicked()), SLOT(RunQuery()));

  QFont font("Monospace");
  font.setStyleHint(QFont::TypeWriter);

  ui_.output->setFont(font);
  ui_.query->setFont(font);

}

void Console::RunQuery() {

  QSqlDatabase db = app_->database()->Connect();
  QSqlQuery query = db.exec(ui_.query->text());
  //ui_.query->clear();

  ui_.output->append("<b>&gt; " + query.executedQuery() + "</b>");

  query.next();

  while (query.isValid()) {
    QSqlRecord record = query.record();
    QStringList values;
    for (int i = 0; i < record.count(); ++i) {
      values.append(record.value(i).toString());
    }

    ui_.output->append(values.join("|"));

    query.next();
  }

  ui_.output->verticalScrollBar()->setValue(ui_.output->verticalScrollBar()->maximum());

}
