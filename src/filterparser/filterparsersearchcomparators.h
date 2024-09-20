/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILTERPARSERSEARCHCOMPARATORS_H
#define FILTERPARSERSEARCHCOMPARATORS_H

#include "config.h"

#include <QVariant>
#include <QString>
#include <QScopedPointer>

class FilterParserSearchTermComparator {
 public:
  FilterParserSearchTermComparator() = default;
  virtual ~FilterParserSearchTermComparator() = default;
  virtual bool Matches(const QVariant &value) const = 0;
 private:
  Q_DISABLE_COPY(FilterParserSearchTermComparator)
};

// "compares" by checking if the field contains the search term
class FilterParserTextContainsComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserTextContainsComparator(const QString &search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.metaType().id() == QMetaType::QString && value.toString().contains(search_term_, Qt::CaseInsensitive);
  }
 private:
  QString search_term_;

  Q_DISABLE_COPY(FilterParserTextContainsComparator)
};

class FilterParserTextEqComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserTextEqComparator(const QString &search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return search_term_.compare(value.toString(), Qt::CaseInsensitive) == 0;
  }
 private:
  QString search_term_;
};

class FilterParserTextNeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserTextNeComparator(const QString &search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return search_term_.compare(value.toString(), Qt::CaseInsensitive) != 0;
  }
 private:
  QString search_term_;
};

class FilterParserIntEqComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntEqComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() == search_term_;
  }
 private:
  int search_term_;
};

class FilterParserIntNeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntNeComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() != search_term_;
  }
 private:
  int search_term_;
};

class FilterParserIntGtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntGtComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() > search_term_;
  }
 private:
  int search_term_;
};

class FilterParserIntGeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntGeComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() >= search_term_;
  }
 private:
  int search_term_;
};

class FilterParserIntLtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntLtComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() < search_term_;
  }
 private:
  int search_term_;
};

class FilterParserIntLeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserIntLeComparator(const int search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toInt() <= search_term_;
  }
 private:
  int search_term_;
};

class FilterParserUIntEqComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntEqComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() == search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserUIntNeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntNeComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() != search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserUIntGtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntGtComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() > search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserUIntGeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntGeComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() >= search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserUIntLtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntLtComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() < search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserUIntLeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserUIntLeComparator(const uint search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toUInt() <= search_term_;
  }
 private:
  uint search_term_;
};

class FilterParserInt64EqComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64EqComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() == search_term_;
  }
 private:
  qint64 search_term_;
};

class FilterParserInt64NeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64NeComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() != search_term_;
  }
 private:
  qint64 search_term_;
};

class FilterParserInt64GtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64GtComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() > search_term_;
  }
 private:
  qint64 search_term_;
};

class FilterParserInt64GeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64GeComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() >= search_term_;
  }
 private:
  qint64 search_term_;
};

class FilterParserInt64LtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64LtComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() < search_term_;
  }
 private:
  qint64 search_term_;
};

class FilterParserInt64LeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserInt64LeComparator(const qint64 search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toLongLong() <= search_term_;
  }
 private:
  qint64 search_term_;
};

// Float Comparators are for the rating
class FilterParserFloatEqComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatEqComparator(const float search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() == search_term_;
  }
 private:
  float search_term_;
};

class FilterParserFloatNeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatNeComparator(const float value) : search_term_(value) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() != search_term_;
  }
 private:
  float search_term_;
};

class FilterParserFloatGtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatGtComparator(const float search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() > search_term_;
  }
 private:
  float search_term_;
};

class FilterParserFloatGeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatGeComparator(const float search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() >= search_term_;
  }
 private:
  float search_term_;
};

class FilterParserFloatLtComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatLtComparator(const float search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() < search_term_;
  }
 private:
  float search_term_;
};

class FilterParserFloatLeComparator : public FilterParserSearchTermComparator {
 public:
  explicit FilterParserFloatLeComparator(const float search_term) : search_term_(search_term) {}
  bool Matches(const QVariant &value) const override {
    return value.toFloat() <= search_term_;
  }
 private:
  float search_term_;
};

#endif  // FILTERPARSERSEARCHCOMPARATORS_H
