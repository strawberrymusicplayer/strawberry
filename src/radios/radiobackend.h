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

#ifndef RADIOBACKEND_H
#define RADIOBACKEND_H

#include <QObject>

#include "includes/shared_ptr.h"
#include "radiochannel.h"

class QThread;
class Database;

class RadioBackend : public QObject {
  Q_OBJECT

 public:
  explicit RadioBackend(const SharedPtr<Database> db, QObject *parent = nullptr);

  void Close();
  void ExitAsync();

  void AddChannelsAsync(const RadioChannelList &channels);
  void GetChannelsAsync();
  void DeleteChannelsAsync();

 private Q_SLOTS:
  void AddChannels(const RadioChannelList &channels);
  void GetChannels();
  void DeleteChannels();

 Q_SIGNALS:
  void NewChannels(const RadioChannelList &channels);
  void ExitFinished();

 private Q_SLOTS:
  void Exit();

 private:
  const SharedPtr<Database> db_;
  QThread *original_thread_;
};

#endif  // RADIOBACKEND_H
