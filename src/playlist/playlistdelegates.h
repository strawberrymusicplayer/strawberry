/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTDELEGATES_H
#define PLAYLISTDELEGATES_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeView>
#include <QCompleter>
#include <QLocale>
#include <QVariant>
#include <QUrl>
#include <QPixmap>
#include <QPainter>
#include <QRect>
#include <QColor>
#include <QRgb>
#include <QSize>
#include <QFont>
#include <QString>
#include <QStringListModel>
#include <QStyleOption>
#include <QHelpEvent>
#include <QLineEdit>

#include "playlist.h"
#include "core/song.h"
#include "widgets/ratingwidget.h"

class CollectionBackend;
class Player;

class QueuedItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit QueuedItemDelegate(QObject *parent, const int indicator_column = static_cast<int>(Playlist::Column::Title));

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  static void DrawBox(QPainter *painter, const QRect line_rect, const QFont &font, const QString &text, int width = -1, const float opacity = 1.0);

  int queue_indicator_size(const QModelIndex &idx) const;

 private:
  int indicator_column_;
};

class PlaylistDelegateBase : public QueuedItemDelegate {
  Q_OBJECT

 public:
  explicit PlaylistDelegateBase(QObject *parent, const QString &suffix = QString());

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  QString displayText(const QVariant &value, const QLocale &locale) const override;
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override;

  QStyleOptionViewItem Adjusted(const QStyleOptionViewItem &option, const QModelIndex &idx) const;

  static const int kMinHeight;

 public Q_SLOTS:
  bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &idx) override;

 protected:
  QTreeView *view_;
  QString suffix_;
};

class LengthItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit LengthItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class SizeItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit SizeItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class DateItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit DateItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class LastPlayedItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit LastPlayedItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class FileTypeItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit FileTypeItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class TextItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit TextItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
};

class TagCompletionModel : public QStringListModel {
  Q_OBJECT

 public:
  explicit TagCompletionModel(SharedPtr<CollectionBackend> backend, const Playlist::Column column, QObject *parent = nullptr);

 private:
  static QString database_column(const Playlist::Column column);
};

class TagCompleter : public QCompleter {
  Q_OBJECT

 public:
  explicit TagCompleter(SharedPtr<CollectionBackend> backend, const Playlist::Column column, QLineEdit *editor);
  ~TagCompleter() override;

 private Q_SLOTS:
  void ModelReady();

 private:
  QLineEdit *editor_;
};

class TagCompletionItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit TagCompletionItemDelegate(QObject *parent, SharedPtr<CollectionBackend> backend, Playlist::Column column) : PlaylistDelegateBase(parent), backend_(backend), column_(column) {};

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;

 private:
  SharedPtr<CollectionBackend> backend_;
  Playlist::Column column_;
};

class NativeSeparatorsDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit NativeSeparatorsDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class SongSourceDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit SongSourceDelegate(QObject *parent);
  QString displayText(const QVariant &value, const QLocale &locale) const override;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;

 private:
  QPixmap LookupPixmap(const Song::Source source, const QSize size, const qreal device_pixel_ratio) const;
};

class RatingItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  RatingItemDelegate(QObject *parent);
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  QString displayText(const QVariant &value, const QLocale &locale) const override;

  void set_mouse_over(const QModelIndex &idx, const QModelIndexList &selected_indexes, const QPoint pos) {
    mouse_over_index_ = idx;
    selected_indexes_ = selected_indexes;
    mouse_over_pos_ = pos;
  }

  void set_mouse_out() { mouse_over_index_ = QModelIndex(); }
  bool is_mouse_over() const { return mouse_over_index_.isValid(); }
  QModelIndex mouse_over_index() const { return mouse_over_index_; }

 private:
  RatingPainter painter_;

  QModelIndex mouse_over_index_;
  QPoint mouse_over_pos_;
  QModelIndexList selected_indexes_;
};

class Ebur128LoudnessLUFSItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit Ebur128LoudnessLUFSItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class Ebur128LoudnessRangeLUItemDelegate : public PlaylistDelegateBase {
  Q_OBJECT

 public:
  explicit Ebur128LoudnessRangeLUItemDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}
  QString displayText(const QVariant &value, const QLocale &locale) const override;
};

#endif  // PLAYLISTDELEGATES_H
