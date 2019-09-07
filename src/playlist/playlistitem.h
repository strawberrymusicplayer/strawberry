/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QFuture>
#include <QFlags>
#include <QMetaType>
#include <QList>
#include <QMap>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QColor>
#include <QVector>
#include <QAction>
#include <QSqlQuery>

#include "core/song.h"

class SqlRow;

class PlaylistItem : public std::enable_shared_from_this<PlaylistItem> {
 public:
  PlaylistItem(const Song::Source &source) : should_skip_(false), source_(source) {}
  virtual ~PlaylistItem();

  static PlaylistItem *NewFromSource(const Song::Source &source);
  static PlaylistItem *NewFromSongsTable(const QString &table, const Song &song);

  enum Option {
    Default = 0x00,

    // Disables the "pause" action.
    PauseDisabled = 0x01,

    // Disables the seek slider.
    SeekDisabled = 0x04,
  };
  Q_DECLARE_FLAGS(Options, Option);

  virtual Song::Source source() const { return source_; }

  virtual Options options() const { return Default; }

  virtual QList<QAction*> actions() { return QList<QAction*>(); }

  virtual bool InitFromQuery(const SqlRow &query) = 0;
  void BindToQuery(QSqlQuery* query) const;
  virtual void Reload() {}
  QFuture<void> BackgroundReload();

  virtual Song Metadata() const = 0;
  virtual QUrl Url() const = 0;

  void SetTemporaryMetadata(const Song &metadata);
  void ClearTemporaryMetadata();
  bool HasTemporaryMetadata() const { return temp_metadata_.is_valid(); }
  QUrl StreamUrl() const { return HasTemporaryMetadata() && temp_metadata_.is_valid() && temp_metadata_.url().isValid() ? temp_metadata_.url() : QUrl(); }

  // Background colors.
  void SetBackgroundColor(short priority, const QColor &color);
  bool HasBackgroundColor(short priority) const;
  void RemoveBackgroundColor(short priority);
  QColor GetCurrentBackgroundColor() const;
  bool HasCurrentBackgroundColor() const;

  // Foreground colors.
  void SetForegroundColor(short priority, const QColor &color);
  bool HasForegroundColor(short priority) const;
  void RemoveForegroundColor(short priority);
  QColor GetCurrentForegroundColor() const;
  bool HasCurrentForegroundColor() const;

  // Convenience function to find out whether this item is from the local collection, as opposed to a device, a file on disk, or a stream.
  // Remember that even if this returns true, the collection item might be invalid so you might want to check that its id is not equal to -1 before actually using it.
  virtual bool IsLocalCollectionItem() const { return false; }
  void SetShouldSkip(bool val);
  bool GetShouldSkip() const;

 protected:
  bool should_skip_;

  enum DatabaseColumn { Column_CollectionId };

  virtual QVariant DatabaseValue(DatabaseColumn) const {
    return QVariant(QVariant::String);
  }
  virtual Song DatabaseSongMetadata() const { return Song(); }

  Song::Source source_;

  Song temp_metadata_;

  QMap<short, QColor> background_colors_;
  QMap<short, QColor> foreground_colors_;
};
typedef std::shared_ptr<PlaylistItem> PlaylistItemPtr;
typedef QList<PlaylistItemPtr> PlaylistItemList;

Q_DECLARE_METATYPE(PlaylistItemPtr)
Q_DECLARE_METATYPE(QList<PlaylistItemPtr>)
Q_DECLARE_OPERATORS_FOR_FLAGS(PlaylistItem::Options)

#endif  // PLAYLISTITEM_H

