/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QSortFilterProxyModel>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "core/logging.h"
#include "utilities/timeconstants.h"
#include "utilities/searchparserutils.h"

#include "collectionfilter.h"
#include "collectionmodel.h"
#include "collectionitem.h"

const QStringList CollectionFilter::Operators = QStringList() << QStringLiteral(":")
                                                              << QStringLiteral("=")
                                                              << QStringLiteral("==")
                                                              << QStringLiteral("<>")
                                                              << QStringLiteral("<")
                                                              << QStringLiteral("<=")
                                                              << QStringLiteral(">")
                                                              << QStringLiteral(">=");

CollectionFilter::CollectionFilter(QObject *parent) : QSortFilterProxyModel(parent) {}

bool CollectionFilter::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const {

  CollectionModel *model = qobject_cast<CollectionModel*>(sourceModel());
  if (!model) return false;
  const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  if (!idx.isValid()) return false;
  CollectionItem *item = model->IndexToItem(idx);
  if (!item) return false;

  if (item->type == CollectionItem::Type::LoadingIndicator) return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QString filter_text = filterRegularExpression().pattern().remove(QLatin1Char('\\'));
#else
  QString filter_text = filterRegExp().pattern();
#endif

  if (filter_text.isEmpty()) return true;

  filter_text = filter_text.replace(QRegularExpression(QStringLiteral("\\s*:\\s*")), QStringLiteral(":"))
                           .replace(QRegularExpression(QStringLiteral("\\s*=\\s*")), QStringLiteral("="))
                           .replace(QRegularExpression(QStringLiteral("\\s*==\\s*")), QStringLiteral("=="))
                           .replace(QRegularExpression(QStringLiteral("\\s*<>\\s*")), QStringLiteral("<>"))
                           .replace(QRegularExpression(QStringLiteral("\\s*<\\s*")), QStringLiteral("<"))
                           .replace(QRegularExpression(QStringLiteral("\\s*>\\s*")), QStringLiteral(">"))
                           .replace(QRegularExpression(QStringLiteral("\\s*<=\\s*")), QStringLiteral("<="))
                           .replace(QRegularExpression(QStringLiteral("\\s*>=\\s*")), QStringLiteral(">="));

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  const QStringList tokens = filter_text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
#else
  const QStringList tokens = filter_text.split(QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
#endif

  filter_text.clear();

  FilterList filters;
  static QRegularExpression operator_regex(QStringLiteral("(=|<[>=]?|>=?|!=)"));
  for (int i = 0; i < tokens.count(); ++i) {
    const QString &token = tokens[i];
    if (token.contains(QLatin1Char(':'))) {
      QString field = token.section(QLatin1Char(':'), 0, 0).remove(QLatin1Char(':')).trimmed();
      QString value = token.section(QLatin1Char(':'), 1, -1).remove(QLatin1Char(':')).trimmed();
      if (field.isEmpty() || value.isEmpty()) continue;
      if (Song::kTextSearchColumns.contains(field, Qt::CaseInsensitive) && value.count(QLatin1Char('"')) <= 2) {
        bool quotation_mark_start = false;
        bool quotation_mark_end = false;
        if (value.left(1) == QLatin1Char('"')) {
          value.remove(0, 1);
          quotation_mark_start = true;
          if (value.length() >= 1 && value.count(QLatin1Char('"')) == 1) {
            value = value.section(QLatin1Char(QLatin1Char('"')), 0, 0).remove(QLatin1Char('"')).trimmed();
            quotation_mark_end = true;
          }
        }
        for (int y = i + 1; y < tokens.count() && !quotation_mark_end; ++y) {
          QString next_value = tokens[y];
          if (!quotation_mark_start && ContainsOperators(next_value)) {
            break;
          }
          if (quotation_mark_start && next_value.contains(QLatin1Char('"'))) {
            next_value = next_value.section(QLatin1Char(QLatin1Char('"')), 0, 0).remove(QLatin1Char('"')).trimmed();
            quotation_mark_end = true;
          }
          value.append(QLatin1Char(' ') + next_value);
          i = y;
        }
        if (!field.isEmpty() && !value.isEmpty()) {
          filters.insert(field, Filter(field, value));
        }
        continue;
      }
    }
    else if (token.contains(operator_regex)) {
      QRegularExpressionMatch re_match = operator_regex.match(token);
      if (re_match.hasMatch()) {
        const QString foperator = re_match.captured(0);
        const QString field = token.section(foperator, 0, 0).remove(foperator).trimmed();
        const QString value = token.section(foperator, 1, -1).remove(foperator).trimmed();
        if (value.isEmpty()) continue;
        if (Song::kNumericalSearchColumns.contains(field, Qt::CaseInsensitive)) {
          if (Song::kIntSearchColumns.contains(field, Qt::CaseInsensitive)) {
            bool ok = false;
            const int value_int = value.toInt(&ok);
            if (ok) {
              filters.insert(field, Filter(field, value_int, foperator));
              continue;
            }
          }
          else if (Song::kUIntSearchColumns.contains(field, Qt::CaseInsensitive)) {
            bool ok = false;
            const uint value_uint = value.toUInt(&ok);
            if (ok) {
              filters.insert(field, Filter(field, value_uint, foperator));
              continue;
            }
          }
          else if (field.compare(QLatin1String("length"), Qt::CaseInsensitive) == 0) {
            filters.insert(field, Filter(field, static_cast<qint64>(Utilities::ParseSearchTime(value)) * kNsecPerSec, foperator));
            continue;
          }
          else if (field.compare(QLatin1String("rating"), Qt::CaseInsensitive) == 0) {
            filters.insert(field, Filter(field, Utilities::ParseSearchRating(value), foperator));
          }
        }
      }
    }
    if (!filter_text.isEmpty()) filter_text.append(QLatin1Char(' '));
    filter_text += token;
  }

  if (filter_text.isEmpty() && filters.isEmpty()) return true;

  return ItemMatchesFilters(item, filters, filter_text);

}

bool CollectionFilter::ItemMatchesFilters(CollectionItem *item, const FilterList &filters, const QString &filter_text) {

 if (item->type == CollectionItem::Type::Song &&
     item->metadata.is_valid() &&
     ItemMetadataMatchesFilters(item->metadata, filters, filter_text)) {
    return true;
 }

  for (CollectionItem *child : std::as_const(item->children)) {
    if (ItemMatchesFilters(child, filters, filter_text)) return true;
  }

  return false;

}

bool CollectionFilter::ItemMetadataMatchesFilters(const Song &metadata, const FilterList &filters, const QString &filter_text) {

  for (FilterList::const_iterator it = filters.begin() ; it != filters.end() ; ++it) {
    const QString &field = it.key();
    const Filter &filter = it.value();
    const QVariant &value = filter.value;
    const QString &foperator = filter.foperator;
    if (field.isEmpty() || !value.isValid()) {
      continue;
    }
    const QVariant data = DataFromField(field, metadata);
    if (
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        value.metaType() != data.metaType()
#else
        value.type() != data.type()
#endif
        || !FieldValueMatchesData(value, data, foperator)) {
      return false;
    }
  }

  return filter_text.isEmpty() || ItemMetadataMatchesFilterText(metadata, filter_text);

}

bool CollectionFilter::ItemMetadataMatchesFilterText(const Song &metadata, const QString &filter_text) {

  return metadata.effective_albumartist().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.artist().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.album().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.title().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.composer().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.performer().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.grouping().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.genre().contains(filter_text, Qt::CaseInsensitive) ||
         metadata.comment().contains(filter_text, Qt::CaseInsensitive);

}

QVariant CollectionFilter::DataFromField(const QString &field, const Song &metadata) {

  if (field == QLatin1String("albumartist")) return metadata.effective_albumartist();
  if (field == QLatin1String("artist"))      return metadata.artist();
  if (field == QLatin1String("album"))       return metadata.album();
  if (field == QLatin1String("title"))       return metadata.title();
  if (field == QLatin1String("composer"))    return metadata.composer();
  if (field == QLatin1String("performer"))   return metadata.performer();
  if (field == QLatin1String("grouping"))    return metadata.grouping();
  if (field == QLatin1String("genre"))       return metadata.genre();
  if (field == QLatin1String("comment"))     return metadata.comment();
  if (field == QLatin1String("track"))       return metadata.track();
  if (field == QLatin1String("year"))        return metadata.year();
  if (field == QLatin1String("length"))      return metadata.length_nanosec();
  if (field == QLatin1String("samplerate"))  return metadata.samplerate();
  if (field == QLatin1String("bitdepth"))    return metadata.bitdepth();
  if (field == QLatin1String("bitrate"))     return metadata.bitrate();
  if (field == QLatin1String("rating"))      return metadata.rating();
  if (field == QLatin1String("playcount"))   return metadata.playcount();
  if (field == QLatin1String("skipcount"))   return metadata.skipcount();

  return QVariant();

}

bool CollectionFilter::FieldValueMatchesData(const QVariant &value, const QVariant &data, const QString &foperator) {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  switch (value.metaType().id()) {
#else
  switch (value.userType()) {
#endif
    case QMetaType::QString:{
      const QString str_value = value.toString();
      const QString str_data = data.toString();
      return str_data.contains(str_value, Qt::CaseInsensitive);
    }
    case QMetaType::Int:{
      return FieldIntValueMatchesData(value.toInt(), foperator, data.toInt());
    }
    case QMetaType::UInt:{
      return FieldUIntValueMatchesData(value.toUInt(), foperator, data.toUInt());
    }
    case QMetaType::LongLong:{
      return FieldLongLongValueMatchesData(value.toLongLong(), foperator, data.toLongLong());
    }
    case QMetaType::Float:{
      return FieldFloatValueMatchesData(value.toFloat(), foperator, data.toFloat());
    }
    default:{
      return false;
    }
  }

  return false;

}

template<typename T>
bool CollectionFilter::FieldNumericalValueMatchesData(const T value, const QString &foperator, const T data) {

  if (foperator == QLatin1Char('=') || foperator == QLatin1String("==")) {
    return data == value;
  }
  if (foperator == QLatin1String("!=") || foperator == QLatin1String("<>")) {
    return data != value;
  }
  if (foperator == QLatin1Char('<')) {
    return data < value;
  }
  if (foperator == QLatin1Char('>')) {
    return data > value;
  }
  if (foperator == QLatin1String(">=")) {
    return data >= value;
  }
  if (foperator == QLatin1String("<=")) {
    return data <= value;
  }

  return false;

}

bool CollectionFilter::FieldIntValueMatchesData(const int value, const QString &foperator, const int data) {

  return FieldNumericalValueMatchesData(value, foperator, data);

}

bool CollectionFilter::FieldUIntValueMatchesData(const uint value, const QString &foperator, const uint data) {

  return FieldNumericalValueMatchesData(value, foperator, data);

}

bool CollectionFilter::FieldLongLongValueMatchesData(const qint64 value, const QString &foperator, const qint64 data) {

  return FieldNumericalValueMatchesData(value, foperator, data);

}

bool CollectionFilter::FieldFloatValueMatchesData(const float value, const QString &foperator, const float data) {

  return FieldNumericalValueMatchesData(value, foperator, data);

}

bool CollectionFilter::ContainsOperators(const QString &token) {

  for (const QString &foperator : std::as_const(Operators)) {
    if (token.contains(foperator, Qt::CaseInsensitive)) return true;
  }

  return false;

}
