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

#ifndef FILTERTREE_H
#define FILTERTREE_H

#include "config.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QScopedPointer>

#include "core/song.h"
#include "filterparsersearchcomparators.h"

class FilterTree {
 public:
  explicit FilterTree();
  virtual ~FilterTree();

  enum class FilterType {
    Nop = 0,
    Or,
    And,
    Not,
    Column,
    Term
  };

  virtual FilterType type() const = 0;

  virtual bool accept(const Song &song) const = 0;

 protected:
  static QVariant DataFromColumn(const QString &column, const Song &metadata);

 private:
  Q_DISABLE_COPY(FilterTree)
};

// Trivial filter that accepts *anything*
class NopFilter : public FilterTree {
 public:
  FilterType type() const override { return FilterType::Nop; }
  bool accept(const Song &song) const override { Q_UNUSED(song); return true; }
};

// Filter that applies a SearchTermComparator to all fields
class FilterTerm : public FilterTree {
 public:
  explicit FilterTerm(FilterParserSearchTermComparator *comparator) : cmp_(comparator) {}

  FilterType type() const override { return FilterType::Term; }

  bool accept(const Song &song) const override {

    if (cmp_->Matches(song.PrettyTitle())) return true;
    if (cmp_->Matches(song.album())) return true;
    if (cmp_->Matches(song.artist())) return true;
    if (cmp_->Matches(song.albumartist())) return true;
    if (cmp_->Matches(song.composer())) return true;
    if (cmp_->Matches(song.performer())) return true;
    if (cmp_->Matches(song.grouping())) return true;
    if (cmp_->Matches(song.genre())) return true;
    if (cmp_->Matches(song.comment())) return true;

    return false;

  }

 private:
  QScopedPointer<FilterParserSearchTermComparator> cmp_;
};

class FilterColumnTerm : public FilterTree {
 public:
  explicit FilterColumnTerm(const QString &column, FilterParserSearchTermComparator *comparator) : column_(column), cmp_(comparator) {}

  FilterType type() const override { return FilterType::Column; }

  bool accept(const Song &song) const override {
    return cmp_->Matches(DataFromColumn(column_, song));
  }

 private:
  const QString column_;
  QScopedPointer<FilterParserSearchTermComparator> cmp_;
};

class NotFilter : public FilterTree {
 public:
  explicit NotFilter(const FilterTree *inv) : child_(inv) {}

  FilterType type() const override { return FilterType::Not; }

  bool accept(const Song &song) const override {
    return !child_->accept(song);
  }

 private:
  QScopedPointer<const FilterTree> child_;
};

class OrFilter : public FilterTree {
 public:
  ~OrFilter() override { qDeleteAll(children_); }

  FilterType type() const override { return FilterType::Or; }

  virtual void add(FilterTree *child) { children_.append(child); }

  bool accept(const Song &song) const override {
    return std::any_of(children_.begin(), children_.end(), [song](FilterTree *child) { return child->accept(song); });
  }

 private:
  QList<FilterTree*> children_;
};

class AndFilter : public FilterTree {
 public:
  ~AndFilter() override { qDeleteAll(children_); }

  FilterType type() const override { return FilterType::And; }

  virtual void add(FilterTree *child) { children_.append(child); }

  bool accept(const Song &song) const override {
    return !std::any_of(children_.begin(), children_.end(), [song](FilterTree *child) { return !child->accept(song); });
  }

 private:
  QList<FilterTree*> children_;
};

#endif  // FILTERTREE_H
