/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QFuture>
#include <QMetaType>
#include <QList>
#include <QMap>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QColor>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QAction;

class SqlQuery;
class SqlRow;

using std::enable_shared_from_this;

class PlaylistItem : public enable_shared_from_this<PlaylistItem> {
 public:
  explicit PlaylistItem(const Song::Source source);
  virtual ~PlaylistItem();

  static SharedPtr<PlaylistItem> NewFromSource(const Song::Source source);
  static SharedPtr<PlaylistItem> NewFromSong(const Song &song);

  enum class Option {
    Default = 0x00,

    // Disables the "pause" action.
    PauseDisabled = 0x01,

    // Disables the seek slider.
    SeekDisabled = 0x04,
  };
  Q_DECLARE_FLAGS(Options, Option)

  virtual Song::Source source() const { return source_; }

  virtual Options options() const { return Option::Default; }

  virtual QList<QAction*> actions() { return QList<QAction*>(); }

  virtual bool InitFromQuery(const SqlRow &query) = 0;
  void BindToQuery(SqlQuery *query) const;
  virtual void Reload() {}
  QFuture<void> BackgroundReload();

  virtual Song Metadata() const = 0;
  virtual Song OriginalMetadata() const = 0;
  virtual QUrl Url() const = 0;

  virtual void SetMetadata(const Song &song) { Q_UNUSED(song); }

  void SetTemporaryMetadata(const Song &metadata);
  void UpdateTemporaryMetadata(const Song &metadata);
  void ClearTemporaryMetadata();
  bool HasTemporaryMetadata() const { return temp_metadata_.is_valid(); }

  Song StreamMetadata() { return HasTemporaryMetadata() ? temp_metadata_ : Metadata(); }
  QUrl StreamUrl() const { return HasTemporaryMetadata() && temp_metadata_.effective_stream_url().isValid() ? temp_metadata_.effective_stream_url() : Url(); }

  std::optional<double> effective_ebur128_integrated_loudness_lufs() const { return HasTemporaryMetadata() && temp_metadata_.is_valid() ? temp_metadata_.ebur128_integrated_loudness_lufs() : Metadata().ebur128_integrated_loudness_lufs(); }

  qint64 effective_beginning_nanosec() const { return HasTemporaryMetadata() && temp_metadata_.is_valid() && temp_metadata_.beginning_nanosec() != -1 ? temp_metadata_.beginning_nanosec() : Metadata().beginning_nanosec(); }
  qint64 effective_end_nanosec() const { return HasTemporaryMetadata() && temp_metadata_.is_valid() && temp_metadata_.end_nanosec() != -1 ? temp_metadata_.end_nanosec() : Metadata().end_nanosec(); }

  virtual void SetArtManual(const QUrl &cover_url) = 0;

  // Background colors.
  void SetBackgroundColor(const short priority, const QColor &color);
  bool HasBackgroundColor(const short priority) const;
  void RemoveBackgroundColor(const short priority);
  QColor GetCurrentBackgroundColor() const;
  bool HasCurrentBackgroundColor() const;

  // Foreground colors.
  void SetForegroundColor(const short priority, const QColor &color);
  bool HasForegroundColor(const short priority) const;
  void RemoveForegroundColor(const short priority);
  QColor GetCurrentForegroundColor() const;
  bool HasCurrentForegroundColor() const;

  // Convenience function to find out whether this item is from the local collection, as opposed to a device, a file on disk, or a stream.
  // Remember that even if this returns true, the collection item might be invalid, so you might want to check that its id is not equal to -1 before actually using it.
  virtual bool IsLocalCollectionItem() const { return false; }
  void SetShouldSkip(const bool should_skip);
  bool GetShouldSkip() const;

 protected:
  bool should_skip_;

  enum class DatabaseColumn {
    CollectionId
  };

  virtual QVariant DatabaseValue(const DatabaseColumn database_column) const { Q_UNUSED(database_column); return QVariant(QString()); }
  virtual Song DatabaseSongMetadata() const { return Song(); }

  Song::Source source_;
  Song temp_metadata_;

  QMap<short, QColor> background_colors_;
  QMap<short, QColor> foreground_colors_;

  Q_DISABLE_COPY(PlaylistItem)
};
using PlaylistItemPtr = SharedPtr<PlaylistItem>;
using PlaylistItemPtrList = QList<PlaylistItemPtr>;

Q_DECLARE_METATYPE(PlaylistItemPtr)
Q_DECLARE_METATYPE(PlaylistItemPtrList)
Q_DECLARE_OPERATORS_FOR_FLAGS(PlaylistItem::Options)

#endif  // PLAYLISTITEM_H
