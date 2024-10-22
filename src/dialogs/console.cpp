/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QStringList>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>

#include "console.h"

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"

using namespace Qt::Literals::StringLiterals;

Console::Console(const SharedPtr<Database> database, QWidget *parent) : QDialog(parent), ui_{}, database_(database) {

  ui_.setupUi(this);

  setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

  QObject::connect(ui_.run, &QPushButton::clicked, this, &Console::RunQuery);

  QFont font(u"Monospace"_s);
  font.setStyleHint(QFont::TypeWriter);

  ui_.output->setFont(font);
  ui_.query->setFont(font);

}

void Console::RunQuery() {

  QSqlDatabase db = database_->Connect();
  QSqlQuery query(db);
  if (!query.prepare(ui_.query->text())) {
    qLog(Error) << query.lastError();
    Q_EMIT Error(query.lastError().text());
    return;
  }
  if (!query.exec()) {
    qLog(Error) << query.lastError();
    Q_EMIT Error(query.lastError().text());
    return;
  }

  ui_.output->append(u"<b>&gt; "_s + query.executedQuery() + u"</b>"_s);

  while (query.next() && query.isValid()) {
    QSqlRecord record = query.record();
    QStringList values;  // clazy:exclude=container-inside-loop
    values.reserve(record.count());
    for (int i = 0; i < record.count(); ++i) {
      values.append(record.value(i).toString());
    }

    ui_.output->append(values.join(u'|'));

  }

  ui_.output->verticalScrollBar()->setValue(ui_.output->verticalScrollBar()->maximum());

}
