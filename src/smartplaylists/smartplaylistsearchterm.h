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

#include <QList>
#include <QDataStream>
#include <QVariant>
#include <QString>

class SmartPlaylistSearchTerm {
 public:
  // These values are persisted, so add to the end of the enum only
  enum Field {
    Field_AlbumArtist = 0,
    Field_Artist,
    Field_Album,
    Field_Title,
    Field_Track,
    Field_Disc,
    Field_Year,
    Field_OriginalYear,
    Field_Genre,
    Field_Composer,
    Field_Performer,
    Field_Grouping,
    Field_Comment,
    Field_Length,
    Field_Filepath,
    Field_Filetype,
    Field_Filesize,
    Field_DateCreated,
    Field_DateModified,
    Field_PlayCount,
    Field_SkipCount,
    Field_LastPlayed,
    Field_Rating,
    Field_Samplerate,
    Field_Bitdepth,
    Field_Bitrate,
    FieldCount
  };

  // These values are persisted, so add to the end of the enum only
  enum Operator {
    // For text
    Op_Contains = 0,
    Op_NotContains = 1,
    Op_StartsWith = 2,
    Op_EndsWith = 3,

    // For numbers
    Op_GreaterThan = 4,
    Op_LessThan = 5,

    // For everything
    Op_Equals = 6,
    Op_NotEquals = 9,

    // For numeric dates (e.g. in the last X days)
    Op_NumericDate = 7,
    // For relative dates
    Op_RelativeDate = 8,

    // For numeric dates (e.g. not in the last X days)
    Op_NumericDateNot = 10,

    Op_Empty = 11,
    Op_NotEmpty = 12,

    // Next value = 13
  };

  enum Type {
    Type_Text,
    Type_Date,
    Type_Time,
    Type_Number,
    Type_Rating,
    Type_Invalid
  };

  // These values are persisted, so add to the end of the enum only
  enum DateType {
    Date_Hour = 0,
    Date_Day,
    Date_Week,
    Date_Month, Date_Year
  };

  explicit SmartPlaylistSearchTerm();
  explicit SmartPlaylistSearchTerm(const Field field, const Operator op, const QVariant &value);

  Field field_;
  Operator operator_;
  QVariant value_;
  DateType date_;
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
  static QString DateName(const DateType date, const bool forQuery);

};

typedef QList<SmartPlaylistSearchTerm::Operator> OperatorList;

QDataStream &operator<<(QDataStream &s, const SmartPlaylistSearchTerm &term);
QDataStream &operator>>(QDataStream &s, SmartPlaylistSearchTerm &term);

#endif  // SMARTPLAYLISTSEARCHTERM_H
