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

#ifndef PLAYLISTQUERYGENERATOR_H
#define PLAYLISTQUERYGENERATOR_H

#include "config.h"

#include <QList>
#include <QByteArray>
#include <QString>

#include "playlistgenerator.h"
#include "smartplaylistsearch.h"

class PlaylistQueryGenerator : public PlaylistGenerator {
  Q_OBJECT

 public:
  explicit PlaylistQueryGenerator(QObject *parent = nullptr);
  explicit PlaylistQueryGenerator(const QString &name, const SmartPlaylistSearch &search, const bool dynamic = false, QObject *parent = nullptr);

  Type type() const override { return Type::Query; }

  void Load(const SmartPlaylistSearch &search);
  void Load(const QByteArray &data) override;
  QByteArray Save() const override;

  PlaylistItemPtrList Generate() override;
  PlaylistItemPtrList GenerateMore(const int count) override;
  bool is_dynamic() const override { return dynamic_; }
  void set_dynamic(bool dynamic) override { dynamic_ = dynamic; }

  SmartPlaylistSearch search() const { return search_; }
  int GetDynamicFuture() override { return search_.limit_; }

 private:
  SmartPlaylistSearch search_;
  bool dynamic_;

  QList<int> previous_ids_;
  int current_pos_;
};

#endif  // PLAYLISTQUERYGENERATOR_H
