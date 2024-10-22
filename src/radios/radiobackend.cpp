/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QMutexLocker>
#include <QSqlDatabase>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/sqlquery.h"
#include "core/song.h"
#include "radiobackend.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

RadioBackend::RadioBackend(const SharedPtr<Database> db, QObject *parent)
    : QObject(parent),
      db_(db),
      original_thread_(thread()) {}

void RadioBackend::Close() {

  if (db_) {
    QMutexLocker l(db_->Mutex());
    db_->Close();
  }

}

void RadioBackend::ExitAsync() {
  QMetaObject::invokeMethod(this, &RadioBackend::Exit, Qt::QueuedConnection);
}

void RadioBackend::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());

  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

void RadioBackend::AddChannelsAsync(const RadioChannelList &channels) {
  QMetaObject::invokeMethod(this, "AddChannels", Qt::QueuedConnection, Q_ARG(RadioChannelList, channels));
}

void RadioBackend::AddChannels(const RadioChannelList &channels) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(u"INSERT INTO radio_channels (source, name, url, thumbnail_url) VALUES (:source, :name, :url, :thumbnail_url)"_s);

  for (const RadioChannel &channel : channels) {
    q.BindValue(u":source"_s, static_cast<int>(channel.source));
    q.BindValue(u":name"_s, channel.name);
    q.BindValue(u":url"_s, channel.url);
    q.BindValue(u":thumbnail_url"_s, channel.thumbnail_url);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  Q_EMIT NewChannels(channels);

}

void RadioBackend::GetChannelsAsync() {

  QMetaObject::invokeMethod(this, &RadioBackend::GetChannels, Qt::QueuedConnection);

}

void RadioBackend::GetChannels() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(u"SELECT source, name, url, thumbnail_url FROM radio_channels"_s);

  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  RadioChannelList channels;
  while (q.next()) {
    RadioChannel channel;
    channel.source = static_cast<Song::Source>(q.value(0).toInt());
    channel.name = q.value(1).toString();
    channel.url.setUrl(q.value(2).toString());
    channel.thumbnail_url.setUrl(q.value(3).toString());
    channels << channel;
  }

  Q_EMIT NewChannels(channels);

}

void RadioBackend::DeleteChannelsAsync() {
  QMetaObject::invokeMethod(this, &RadioBackend::DeleteChannels, Qt::QueuedConnection);
}

void RadioBackend::DeleteChannels() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(u"DELETE FROM radio_channels"_s);

  if (!q.Exec()) {
    db_->ReportErrors(q);
  }

}
