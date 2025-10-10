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

using namespace Qt::Literals::StringLiterals;

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm() : field_(Field::Title), operator_(Operator::Equals), datetype_(DateType::Hour) {}

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm(Field field, Operator op, const QVariant &value)
    : field_(field), operator_(op), value_(value), datetype_(DateType::Hour) {}

QString SmartPlaylistSearchTerm::ToSql() const {

  QString col = FieldColumnName(field_);
  QString date = DateName(datetype_, true);
  QString value = value_.toString();
  value.replace(u'\'', "''"_L1);

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
      col = "DATETIME("_L1 + col + ", 'unixepoch', 'localtime')"_L1;
      second_value = second_value_.toString();
      second_value.replace(u'\'', "''"_L1);
      if (date == "weeks"_L1) {
        // Sqlite doesn't know weeks, transform them to days
        date = "days"_L1;
        value = QString::number(value_.toInt() * 7);
        second_value = QString::number(second_value_.toInt() * 7);
      }
    }
    else {
      // We have the exact date
      // The calendar widget specifies no time so ditch the possible time part
      // from integers representing the dates.
      col = "DATE("_L1 + col + ", 'unixepoch', 'localtime')"_L1;
      value = "DATE("_L1 + value + ", 'unixepoch', 'localtime')"_L1;
    }
  }
  else if (TypeOf(field_) == Type::Time) {
    // Convert seconds to nanoseconds
    value = "CAST ("_L1 + value + " *1000000000 AS INTEGER)"_L1;
  }

  // File paths need some extra processing since they are stored as encoded urls in the database.
  if (field_ == Field::Filepath) {
    if (operator_ == Operator::StartsWith || operator_ == Operator::Equals) {
      value = QString::fromUtf8(QUrl::fromLocalFile(value).toEncoded());
    }
    else {
      value = QString::fromUtf8(QUrl(value).toEncoded());
    }
  }
  else if (TypeOf(field_) == Type::Rating) {
    col = "CAST ((replace("_L1 + col + ", -1, 0) + 0.05) * 10 AS INTEGER)"_L1;
    value = "CAST (("_L1 + value + " + 0.05) * 10 AS INTEGER)"_L1;
  }

  switch (operator_) {
    case Operator::Contains:
      return col + " LIKE '%"_L1 + value + "%'"_L1;
    case Operator::NotContains:
      return col + " NOT LIKE '%"_L1 + value + "%'"_L1;
    case Operator::StartsWith:
      return col + " LIKE '"_L1 + value + "%'"_L1;
    case Operator::EndsWith:
      return col + " LIKE '%"_L1 + value + u'\'';
    case Operator::Equals:
      if (TypeOf(field_) == Type::Text) {
        return col + " LIKE '"_L1 + value + u'\'';
      }
      else if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " = "_L1 + value;
      }
      else {
        return col + " = '"_L1 + value + u'\'';
      }
    case Operator::GreaterThan:
      if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " > "_L1 + value;
      }
      else {
        return col + " > '"_L1 + value + u'\'';
      }
    case Operator::LessThan:
      if (TypeOf(field_) == Type::Date || TypeOf(field_) == Type::Time || TypeOf(field_) == Type::Rating) {
        return col + " < "_L1 + value;
      }
      else {
        return col + " < '"_L1 + value + u'\'';
      }
    case Operator::NumericDate:
      return col + " > "_L1 + "DATETIME('now', '-"_L1 + value + u' ' + date + "', 'localtime')"_L1;
    case Operator::NumericDateNot:
      return col + " < "_L1 + "DATETIME('now', '-"_L1 + value + u' ' + date + "', 'localtime')"_L1;
    case Operator::RelativeDate:
      // Consider the time range before the first date but after the second one
      return "("_L1 + col + " < "_L1 + "DATETIME('now', '-"_L1 + value + u' ' + date + "', 'localtime') AND "_L1 + col + " > "_L1 + "DATETIME('now', '-"_L1 + second_value + u' ' + date + "', 'localtime'))"_L1;
    case Operator::NotEquals:
      if (TypeOf(field_) == Type::Text) {
        return col + " <> '"_L1 + value + u'\'';
      }
      else {
        return col + " <> "_L1 + value;
      }
    case Operator::Empty:
      return col + " = ''"_L1;
    case Operator::NotEmpty:
      return col + " <> ''"_L1;
  }

  return QString();
}

bool SmartPlaylistSearchTerm::is_valid() const {

  // We can accept also a zero value in these cases
  if (operator_ == Operator::NumericDate) {
    return value_.toInt() >= 0;
  }
  if (operator_ == Operator::RelativeDate) {
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
    case Field::BPM:
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

}

QString SmartPlaylistSearchTerm::FieldColumnName(const Field field) {

  switch (field) {
    case Field::AlbumArtist:
      return u"albumartist"_s;
    case Field::Artist:
      return u"artist"_s;
    case Field::Album:
      return u"album"_s;
    case Field::Title:
      return u"title"_s;
    case Field::Track:
      return u"track"_s;
    case Field::Disc:
      return u"disc"_s;
    case Field::Year:
      return u"year"_s;
    case Field::OriginalYear:
      return u"originalyear"_s;
    case Field::Genre:
      return u"genre"_s;
    case Field::Composer:
      return u"composer"_s;
    case Field::Performer:
      return u"performer"_s;
    case Field::Grouping:
      return u"grouping"_s;
    case Field::Comment:
      return u"comment"_s;
    case Field::Length:
      return u"length"_s;
    case Field::Filepath:
      return u"url"_s;
    case Field::Filetype:
      return u"filetype"_s;
    case Field::Filesize:
      return u"filesize"_s;
    case Field::DateCreated:
      return u"ctime"_s;
    case Field::DateModified:
      return u"mtime"_s;
    case Field::PlayCount:
      return u"playcount"_s;
    case Field::SkipCount:
      return u"skipcount"_s;
    case Field::LastPlayed:
      return u"lastplayed"_s;
    case Field::Rating:
      return u"rating"_s;
    case Field::Samplerate:
      return u"samplerate"_s;
    case Field::Bitdepth:
      return u"bitdepth"_s;
    case Field::Bitrate:
      return u"bitrate"_s;
    case Field::ArtistSort:
      return u"artistsort"_s;
    case Field::AlbumArtistSort:
      return u"albumartistsort"_s;
    case Field::AlbumSort:
      return u"albumsort"_s;
    case Field::ComposerSort:
      return u"composersort"_s;
    case Field::PerformerSort:
      return u"performersort"_s;
    case Field::TitleSort:
      return u"titlesort"_s;
    case Field::BPM:
      return u"bpm"_s;
    case Field::Mood:
      return u"mood"_s;
    case Field::InitialKey:
      return u"initial_key"_s;
    case Field::FieldCount:
      Q_ASSERT(0);
  }

  return QString();

}

QString SmartPlaylistSearchTerm::FieldName(const Field field) {

  switch (field) {
    case Field::AlbumArtist:
      return Playlist::column_name(Playlist::Column::AlbumArtist);
    case Field::Artist:
      return Playlist::column_name(Playlist::Column::Artist);
    case Field::Album:
      return Playlist::column_name(Playlist::Column::Album);
    case Field::Title:
      return Playlist::column_name(Playlist::Column::Title);
    case Field::Track:
      return Playlist::column_name(Playlist::Column::Track);
    case Field::Disc:
      return Playlist::column_name(Playlist::Column::Disc);
    case Field::Year:
      return Playlist::column_name(Playlist::Column::Year);
    case Field::OriginalYear:
      return Playlist::column_name(Playlist::Column::OriginalYear);
    case Field::Genre:
      return Playlist::column_name(Playlist::Column::Genre);
    case Field::Composer:
      return Playlist::column_name(Playlist::Column::Composer);
    case Field::Performer:
      return Playlist::column_name(Playlist::Column::Performer);
    case Field::Grouping:
      return Playlist::column_name(Playlist::Column::Grouping);
    case Field::Comment:
      return QObject::tr("Comment");
    case Field::Length:
      return Playlist::column_name(Playlist::Column::Length);
    case Field::Filepath:
      return Playlist::column_name(Playlist::Column::URL);
    case Field::Filetype:
      return Playlist::column_name(Playlist::Column::Filetype);
    case Field::Filesize:
      return Playlist::column_name(Playlist::Column::Filesize);
    case Field::DateCreated:
      return Playlist::column_name(Playlist::Column::DateCreated);
    case Field::DateModified:
      return Playlist::column_name(Playlist::Column::DateModified);
    case Field::PlayCount:
      return Playlist::column_name(Playlist::Column::PlayCount);
    case Field::SkipCount:
      return Playlist::column_name(Playlist::Column::SkipCount);
    case Field::LastPlayed:
      return Playlist::column_name(Playlist::Column::LastPlayed);
    case Field::Rating:
      return Playlist::column_name(Playlist::Column::Rating);
    case Field::Samplerate:
      return Playlist::column_name(Playlist::Column::Samplerate);
    case Field::Bitdepth:
      return Playlist::column_name(Playlist::Column::Bitdepth);
    case Field::Bitrate:
      return Playlist::column_name(Playlist::Column::Bitrate);
    case Field::ArtistSort:
      return Playlist::column_name(Playlist::Column::ArtistSort);
    case Field::AlbumArtistSort:
      return Playlist::column_name(Playlist::Column::AlbumArtistSort);
    case Field::AlbumSort:
      return Playlist::column_name(Playlist::Column::AlbumSort);
    case Field::ComposerSort:
      return Playlist::column_name(Playlist::Column::ComposerSort);
    case Field::PerformerSort:
      return Playlist::column_name(Playlist::Column::PerformerSort);
    case Field::TitleSort:
      return Playlist::column_name(Playlist::Column::TitleSort);
    case Field::BPM:
      return Playlist::column_name(Playlist::Column::BPM);
    case Field::Mood:
      return Playlist::column_name(Playlist::Column::Mood);
    case Field::InitialKey:
      return Playlist::column_name(Playlist::Column::InitialKey);
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
      return (forQuery ? u"hours"_s : QObject::tr("Hours"));
    case DateType::Day:
      return (forQuery ? u"days"_s : QObject::tr("Days"));
    case DateType::Week:
      return (forQuery ? u"weeks"_s : QObject::tr("Weeks"));
    case DateType::Month:
      return (forQuery ? u"months"_s : QObject::tr("Months"));
    case DateType::Year:
      return (forQuery ? u"years"_s : QObject::tr("Years"));
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
