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

#include "config.h"

#include <QObject>
#include <QDataStream>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "smartplaylistsearchterm.h"
#include "playlist/playlist.h"

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm() : field_(Field::Title), operator_(Operator::Equals), datetype_(DateType::Hour) {}

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm(Field field, Operator op, const QVariant &value)
    : field_(field), operator_(op), value_(value), datetype_(DateType::Hour) {}

QString SmartPlaylistSearchTerm::ToSql() const {

  QString col = FieldColumnName(field_);
  QString date = DateName(datetype_, true);
  QString value = value_.toString();
  value.replace('\'', "''");

  if (field_ == Field::Filetype) {
    Song::FileType filetype = Song::FiletypeByExtension(value);
    if (filetype == Song::FileType::Unknown) {
      filetype = Song::FiletypeByDescription(value);
    }
    value = QString::number(static_cast<int>(filetype));
  }

  QString second_value;

  bool special_date_query = (operator_ == Operator::NumericDate ||
                             operator_ == Operator::NumericDateNot ||
                             operator_ == Operator::RelativeDate);

  // Floating point problems...
  // Theoretically 0.0 == 0 stars, 0.1 == 0.5 star, 0.2 == 1 star etc.
  // but in reality we need to consider anything from [0.05, 0.15) range to be 0.5 star etc.
  // To make this simple, I transform the ranges to integeres and then operate on ints: [0.0, 0.05) -> 0, [0.05, 0.15) -> 1 etc.
  if (TypeOf(field_) == Type::Date) {
    if (special_date_query) {
      // We have a numeric date, consider also the time for more precision
      col = "DATETIME(" + col + ", 'unixepoch', 'localtime')";
      second_value = second_value_.toString();
      second_value.replace('\'', "''");
      if (date == "weeks") {
        // Sqlite doesn't know weeks, transform them to days
        date = "days";
        value = QString::number(value_.toInt() * 7);
        second_value = QString::number(second_value_.toInt() * 7);
      }
    }
    else {
      // We have the exact date
      // The calendar widget specifies no time so ditch the possible time part
      // from integers representing the dates.
      col = "DATE(" + col + ", 'unixepoch', 'localtime')";
      value = "DATE(" + value + ", 'unixepoch', 'localtime')";
    }
  }
  else if (TypeOf(field_) == Type::Time) {
    // Convert seconds to nanoseconds
    value = "CAST (" + value + " *1000000000 AS INTEGER)";
  }

  // File paths need some extra processing since they are stored as encoded urls in the database.
  if (field_ == Field::Filepath) {
    if (operator_ == Operator::StartsWith || operator_ == Operator::Equals) {
      value = QUrl::fromLocalFile(value).toEncoded();
    }
    else {
      value = QUrl(value).toEncoded();
    }
  }
  else if (TypeOf(field_) == Type::Rating) {
    col = "CAST ((" + col + " + 0.05) * 10 AS INTEGER)";
    value = "CAST ((" + value + " + 0.05) * 10 AS INTEGER)";
  }

  switch (operator_) {
    case Operator::Contains:
      return col + " LIKE '%" + value + "%'";
    case Operator::NotContains:
      return col + " NOT LIKE '%" + value + "%'";
    case Operator::StartsWith:
      return col + " LIKE '" + value + "%'";
    case Operator::EndsWith:
      return col + " LIKE '%" + value + "'";
    case Operator::Equals:
      if (TypeOf(field_) == Type::Text) {
        return col + " LIKE '" + value + "'";
      }
      else if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " = " + value;
      }
      else {
        return col + " = '" + value + "'";
      }
    case Operator::GreaterThan:
      if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " > " + value;
      }
      else {
        return col + " > '" + value + "'";
      }
    case Operator::LessThan:
      if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " < " + value;
      }
      else {
        return col + " < '" + value + "'";
      }
    case Operator::NumericDate:
      return col + " > " + "DATETIME('now', '-" + value + " " + date + "', 'localtime')";
    case Operator::NumericDateNot:
      return col + " < " + "DATETIME('now', '-" + value + " " + date + "', 'localtime')";
    case Operator::RelativeDate:
      // Consider the time range before the first date but after the second one
      return "(" + col + " < " + "DATETIME('now', '-" + value + " " + date + "', 'localtime') AND " + col + " > " + "DATETIME('now', '-" + second_value + " " + date + "', 'localtime'))";
    case Operator::NotEquals:
      if (TypeOf(field_) == Type::Text) {
        return col + " <> '" + value + "'";
      }
      else {
        return col + " <> " + value;
      }
    case Operator::Empty:
      return col + " = ''";
    case Operator::NotEmpty:
      return col + " <> ''";
  }

  return QString();
}

bool SmartPlaylistSearchTerm::is_valid() const {

  // We can accept also a zero value in these cases
  if (operator_ == Operator::NumericDate) {
    return value_.toInt() >= 0;
  }
  else if (operator_ == Operator::RelativeDate) {
    return (value_.toInt() >= 0 && value_.toInt() < second_value_.toInt());
  }

  switch (TypeOf(field_)) {
    case Type::Text:
      if (operator_ == Operator::Empty || operator_ == Operator::NotEmpty) {
        return true;
      }
      // Empty fields should be possible.
      // All values for Type::Text should be valid.
      return !value_.toString().isEmpty();
    case Type::Date:
      return value_.toInt() != 0;
    case Type::Number:
      return value_.toInt() >= 0;
    case Type::Time:
      return true;
    case Type::Rating:
      return value_.toFloat() >= 0.0;
    case Type::Invalid:
      return false;
  }
  return false;

}

bool SmartPlaylistSearchTerm::operator==(const SmartPlaylistSearchTerm &other) const {
  return field_ == other.field_ &&
         operator_ == other.operator_ &&
         value_ == other.value_ &&
         datetype_ == other.datetype_ &&
         second_value_ == other.second_value_;
}

SmartPlaylistSearchTerm::Type SmartPlaylistSearchTerm::TypeOf(const Field field) {

  switch (field) {
    case Field::Length:
      return Type::Time;

    case Field::Track:
    case Field::Disc:
    case Field::Year:
    case Field::OriginalYear:
    case Field::Filesize:
    case Field::PlayCount:
    case Field::SkipCount:
    case Field::Samplerate:
    case Field::Bitdepth:
    case Field::Bitrate:
      return Type::Number;

    case Field::LastPlayed:
    case Field::DateCreated:
    case Field::DateModified:
      return Type::Date;

    case Field::Rating:
      return Type::Rating;

    default:
      return Type::Text;
  }

}

SmartPlaylistSearchTerm::OperatorList SmartPlaylistSearchTerm::OperatorsForType(const Type type) {

  switch (type) {
    case Type::Text:
      return OperatorList() << Operator::Contains << Operator::NotContains << Operator::Equals
                            << Operator::NotEquals << Operator::Empty << Operator::NotEmpty
                            << Operator::StartsWith << Operator::EndsWith;
    case Type::Date:
      return OperatorList() << Operator::Equals << Operator::NotEquals << Operator::GreaterThan
                            << Operator::LessThan << Operator::NumericDate
                            << Operator::NumericDateNot << Operator::RelativeDate;
    default:
      return OperatorList() << Operator::Equals << Operator::NotEquals << Operator::GreaterThan
                            << Operator::LessThan;
  }

}

QString SmartPlaylistSearchTerm::OperatorText(const Type type, const Operator op) {

  if (type == Type::Date) {
    switch (op) {
      case Operator::GreaterThan:
        return QObject::tr("after");
      case Operator::LessThan:
        return QObject::tr("before");
      case Operator::Equals:
        return QObject::tr("on");
      case Operator::NotEquals:
        return QObject::tr("not on");
      case Operator::NumericDate:
        return QObject::tr("in the last");
      case Operator::NumericDateNot:
        return QObject::tr("not in the last");
      case Operator::RelativeDate:
        return QObject::tr("between");
      default:
        return QString();
    }
  }

  switch (op) {
    case Operator::Contains:
      return QObject::tr("contains");
    case Operator::NotContains:
      return QObject::tr("does not contain");
    case Operator::StartsWith:
      return QObject::tr("starts with");
    case Operator::EndsWith:
      return QObject::tr("ends with");
    case Operator::GreaterThan:
      return QObject::tr("greater than");
    case Operator::LessThan:
      return QObject::tr("less than");
    case Operator::Equals:
      return QObject::tr("equals");
    case Operator::NotEquals:
      return QObject::tr("not equals");
    case Operator::Empty:
      return QObject::tr("empty");
    case Operator::NotEmpty:
      return QObject::tr("not empty");
    default:
      return QString();
  }

  return QString();

}

QString SmartPlaylistSearchTerm::FieldColumnName(const Field field) {

  switch (field) {
    case Field::AlbumArtist:
      return "albumartist";
    case Field::Artist:
      return "artist";
    case Field::Album:
      return "album";
    case Field::Title:
      return "title";
    case Field::Track:
      return "track";
    case Field::Disc:
      return "disc";
    case Field::Year:
      return "year";
    case Field::OriginalYear:
      return "originalyear";
    case Field::Genre:
      return "genre";
    case Field::Composer:
      return "composer";
    case Field::Performer:
      return "performer";
    case Field::Grouping:
      return "grouping";
    case Field::Comment:
      return "comment";
    case Field::Length:
      return "length";
    case Field::Filepath:
      return "url";
    case Field::Filetype:
      return "filetype";
    case Field::Filesize:
      return "filesize";
    case Field::DateCreated:
      return "ctime";
    case Field::DateModified:
      return "mtime";
    case Field::PlayCount:
      return "playcount";
    case Field::SkipCount:
      return "skipcount";
    case Field::LastPlayed:
      return "lastplayed";
    case Field::Rating:
      return "rating";
    case Field::Samplerate:
      return "samplerate";
    case Field::Bitdepth:
      return "bitdepth";
    case Field::Bitrate:
      return "bitrate";
    case Field::FieldCount:
      Q_ASSERT(0);
  }
  return QString();

}

QString SmartPlaylistSearchTerm::FieldName(const Field field) {

  switch (field) {
    case Field::AlbumArtist:
      return Playlist::column_name(Playlist::Column_AlbumArtist);
    case Field::Artist:
      return Playlist::column_name(Playlist::Column_Artist);
    case Field::Album:
      return Playlist::column_name(Playlist::Column_Album);
    case Field::Title:
      return Playlist::column_name(Playlist::Column_Title);
    case Field::Track:
      return Playlist::column_name(Playlist::Column_Track);
    case Field::Disc:
      return Playlist::column_name(Playlist::Column_Disc);
    case Field::Year:
      return Playlist::column_name(Playlist::Column_Year);
    case Field::OriginalYear:
      return Playlist::column_name(Playlist::Column_OriginalYear);
    case Field::Genre:
      return Playlist::column_name(Playlist::Column_Genre);
    case Field::Composer:
      return Playlist::column_name(Playlist::Column_Composer);
    case Field::Performer:
      return Playlist::column_name(Playlist::Column_Performer);
    case Field::Grouping:
      return Playlist::column_name(Playlist::Column_Grouping);
    case Field::Comment:
      return QObject::tr("Comment");
    case Field::Length:
      return Playlist::column_name(Playlist::Column_Length);
    case Field::Filepath:
      return Playlist::column_name(Playlist::Column_Filename);
    case Field::Filetype:
      return Playlist::column_name(Playlist::Column_Filetype);
    case Field::Filesize:
      return Playlist::column_name(Playlist::Column_Filesize);
    case Field::DateCreated:
      return Playlist::column_name(Playlist::Column_DateCreated);
    case Field::DateModified:
      return Playlist::column_name(Playlist::Column_DateModified);
    case Field::PlayCount:
      return Playlist::column_name(Playlist::Column_PlayCount);
    case Field::SkipCount:
      return Playlist::column_name(Playlist::Column_SkipCount);
    case Field::LastPlayed:
      return Playlist::column_name(Playlist::Column_LastPlayed);
    case Field::Rating:
      return Playlist::column_name(Playlist::Column_Rating);
    case Field::Samplerate:
      return Playlist::column_name(Playlist::Column_Samplerate);
    case Field::Bitdepth:
      return Playlist::column_name(Playlist::Column_Bitdepth);
    case Field::Bitrate:
      return Playlist::column_name(Playlist::Column_Bitrate);
    case Field::FieldCount:
      Q_ASSERT(0);
  }
  return QString();

}

QString SmartPlaylistSearchTerm::FieldSortOrderText(const Type type, const bool ascending) {

  switch (type) {
    case Type::Text:
      return ascending ? QObject::tr("A-Z") : QObject::tr("Z-A");
    case Type::Date:
      return ascending ? QObject::tr("oldest first") : QObject::tr("newest first");
    case Type::Time:
      return ascending ? QObject::tr("shortest first") : QObject::tr("longest first");
    case Type::Number:
    case Type::Rating:
      return ascending ? QObject::tr("smallest first") : QObject::tr("biggest first");
    case Type::Invalid:
      return QString();
  }
  return QString();

}

QString SmartPlaylistSearchTerm::DateName(const DateType datetype, const bool forQuery) {

  // If forQuery is true, untranslated keywords are returned
  switch (datetype) {
    case DateType::Hour:
      return (forQuery ? "hours" : QObject::tr("Hours"));
    case DateType::Day:
      return (forQuery ? "days" : QObject::tr("Days"));
    case DateType::Week:
      return (forQuery ? "weeks" : QObject::tr("Weeks"));
    case DateType::Month:
      return (forQuery ? "months" : QObject::tr("Months"));
    case DateType::Year:
      return (forQuery ? "years" : QObject::tr("Years"));
  }
  return QString();

}

QDataStream &operator<<(QDataStream &s, const SmartPlaylistSearchTerm &term) {

  s << static_cast<quint8>(term.field_);
  s << static_cast<quint8>(term.operator_);
  s << term.value_;
  s << term.second_value_;
  s << static_cast<quint8>(term.datetype_);
  return s;

}

QDataStream &operator>>(QDataStream &s, SmartPlaylistSearchTerm &term) {

  quint8 field = 0, op = 0, date = 0;
  s >> field >> op >> term.value_ >> term.second_value_ >> date;
  term.field_ = static_cast<SmartPlaylistSearchTerm::Field>(field);
  term.operator_ = static_cast<SmartPlaylistSearchTerm::Operator>(op);
  term.datetype_ = static_cast<SmartPlaylistSearchTerm::DateType>(date);
  return s;

}
