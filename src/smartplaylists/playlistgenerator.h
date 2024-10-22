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

#ifndef PLAYLISTGENERATOR_H
#define PLAYLISTGENERATOR_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "playlist/playlistitem.h"

class CollectionBackend;

using std::enable_shared_from_this;

class PlaylistGenerator : public QObject, public enable_shared_from_this<PlaylistGenerator> {
  Q_OBJECT

 public:
  explicit PlaylistGenerator(QObject *parent = nullptr);

  static const int kDefaultLimit;
  static const int kDefaultDynamicHistory;
  static const int kDefaultDynamicFuture;

  enum class Type {
    None = 0,
    Query = 1
  };

  // Creates a new PlaylistGenerator of the given type
  static SharedPtr<PlaylistGenerator> Create(const Type type = Type::Query);

  // Should be called before Load on a new PlaylistGenerator
  void set_collection_backend(SharedPtr<CollectionBackend> collection_backend) { collection_backend_ = collection_backend; }
  void set_name(const QString &name) { name_ = name; }
  SharedPtr<CollectionBackend> collection() const { return collection_backend_; }
  QString name() const { return name_; }

  // Name of the subclass
  virtual Type type() const = 0;

  // Serializes the PlaylistGenerator's settings
  // Called on UI-thread.
  virtual void Load(const QByteArray &data) = 0;
  // Called on UI-thread.
  virtual QByteArray Save() const = 0;

  // Creates and returns a playlist
  // Called from non-UI thread.
  virtual PlaylistItemPtrList Generate() = 0;

  // If the generator can be used as a dynamic playlist then GenerateMore should return the next tracks in the sequence.
  // The subclass should remember the last GetDynamicHistory() + GetDynamicFuture() tracks,
  // and ensure that the tracks returned from this method are not in that set.
  virtual bool is_dynamic() const { return false; }
  virtual void set_dynamic(const bool dynamic) { Q_UNUSED(dynamic); }
  // Called from non-UI thread.
  virtual PlaylistItemPtrList GenerateMore(int count) {
    Q_UNUSED(count);
    return PlaylistItemPtrList();
  }

  virtual int GetDynamicHistory() { return kDefaultDynamicHistory; }
  virtual int GetDynamicFuture() { return kDefaultDynamicFuture; }

 Q_SIGNALS:
  void Error(const QString &message);

 protected:
  SharedPtr<CollectionBackend> collection_backend_;

 private:
  QString name_;
};

#include "playlistgenerator_fwd.h"

#endif  // PLAYLISTGENERATOR_H
