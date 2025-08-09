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

#ifndef SMARTPLAYLISTSEARCHTERM_H
#define SMARTPLAYLISTSEARCHTERM_H

#include "config.h"

#include <QMetaType>
#include <QList>
#include <QDataStream>
#include <QVariant>
#include <QString>

class SmartPlaylistSearchTerm {
 public:
  // These values are persisted, so add to the end of the enum only
  enum class Field {
    AlbumArtist = 0,
    Artist,
    Album,
    Title,
    Track,
    Disc,
    Year,
    OriginalYear,
    Genre,
    Composer,
    Performer,
    Grouping,
    Comment,
    Length,
    Filepath,
    Filetype,
    Filesize,
    DateCreated,
    DateModified,
    PlayCount,
    SkipCount,
    LastPlayed,
    Rating,
    Samplerate,
    Bitdepth,
    Bitrate,
    ArtistSort,
    AlbumArtistSort,
    AlbumSort,
    ComposerSort,
    PerformerSort,
    TitleSort,
    BPM,
    Mood,
    InitialKey,
    FieldCount
  };

  // These values are persisted, so add to the end of the enum only
  enum class Operator {
    // For text
    Contains = 0,
    NotContains = 1,
    StartsWith = 2,
    EndsWith = 3,

    // For numbers
    GreaterThan = 4,
    LessThan = 5,

    // For everything
    Equals = 6,
    NotEquals = 9,

    // For numeric dates (e.g. in the last X days)
    NumericDate = 7,
    // For relative dates
    RelativeDate = 8,

    // For numeric dates (e.g. not in the last X days)
    NumericDateNot = 10,

    Empty = 11,
    NotEmpty = 12,

    // Next value = 13
  };
  using OperatorList = QList<Operator>;

  enum class Type {
    Text,
    Date,
    Time,
    Number,
    Rating,
    Invalid
  };

  // These values are persisted, so add to the end of the enum only
  enum class DateType {
    Hour = 0,
    Day,
    Week,
    Month,
    Year
  };

  explicit SmartPlaylistSearchTerm();
  explicit SmartPlaylistSearchTerm(const Field field, const Operator op, const QVariant &value);

  Field field_;
  Operator operator_;
  QVariant value_;
  DateType datetype_;
  // For relative dates, we need a second parameter, might be useful somewhere else
  QVariant second_value_;

  QString ToSql() const;
  bool is_valid() const;
  bool operator==(const SmartPlaylistSearchTerm &other) const;
  bool operator!=(const SmartPlaylistSearchTerm &other) const { return !(*this == other); }

  static Type TypeOf(const Field field);
  static QList<Operator> OperatorsForType(const Type type);
  static QString OperatorText(const Type type, const Operator op);
  static QString FieldName(const Field field);
  static QString FieldColumnName(const Field field);
  static QString FieldSortOrderText(const Type type, const bool ascending);
  static QString DateName(const DateType datetype, const bool forQuery);
};

QDataStream &operator<<(QDataStream &s, const SmartPlaylistSearchTerm &term);
QDataStream &operator>>(QDataStream &s, SmartPlaylistSearchTerm &term);

Q_DECLARE_METATYPE(SmartPlaylistSearchTerm::Field)
Q_DECLARE_METATYPE(SmartPlaylistSearchTerm::Operator)
Q_DECLARE_METATYPE(SmartPlaylistSearchTerm::OperatorList)
Q_DECLARE_METATYPE(SmartPlaylistSearchTerm::Type)
Q_DECLARE_METATYPE(SmartPlaylistSearchTerm::DateType)

#endif  // SMARTPLAYLISTSEARCHTERM_H
