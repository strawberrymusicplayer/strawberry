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

#include "config.h"

#include <algorithm>
#include <cmath>

#include <QList>
#include <QMap>
#include <QSet>
#include <QChar>
#include <QScopedPointer>
#include <QString>
#include <QtAlgorithms>
#include <QAbstractItemModel>

#include "playlist.h"
#include "playlistfilterparser.h"
#include "utilities/searchparserutils.h"

class PlaylistSearchTermComparator {
 public:
  PlaylistSearchTermComparator() = default;
  virtual ~PlaylistSearchTermComparator() = default;
  virtual bool Matches(const QString &element) const = 0;
 private:
  Q_DISABLE_COPY(PlaylistSearchTermComparator)
};

// "compares" by checking if the field contains the search term
class PlaylistDefaultComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistDefaultComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.contains(search_term_);
  }
 private:
  QString search_term_;

  Q_DISABLE_COPY(PlaylistDefaultComparator)
};

class PlaylistEqComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistEqComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return search_term_ == element;
  }
 private:
  QString search_term_;
};

class PlaylistNeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistNeComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return search_term_ != element;
  }
 private:
  QString search_term_;
};

class PlaylistLexicalGtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLexicalGtComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element > search_term_;
  }
 private:
  QString search_term_;
};

class PlaylistLexicalGeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLexicalGeComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element >= search_term_;
  }
 private:
  QString search_term_;
};

class PlaylistLexicalLtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLexicalLtComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element < search_term_;
  }
 private:
  QString search_term_;
};

class PlaylistLexicalLeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLexicalLeComparator(const QString &value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element <= search_term_;
  }
 private:
  QString search_term_;
};

// Float Comparators are for the rating
class PlaylistFloatEqComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatEqComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return search_term_ == element.toFloat();
  }
 private:
  float search_term_;
};

class PlaylistFloatNeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatNeComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return search_term_ != element.toFloat();
  }
 private:
  float search_term_;
};

class PlaylistFloatGtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatGtComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toFloat() > search_term_;
  }
 private:
  float search_term_;
};

class PlaylistFloatGeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatGeComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toFloat() >= search_term_;
  }
 private:
  float search_term_;
};

class PlaylistFloatLtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatLtComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toFloat() < search_term_;
  }
 private:
  float search_term_;
};

class PlaylistFloatLeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistFloatLeComparator(const float value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toFloat() <= search_term_;
  }
 private:
  float search_term_;
};

class PlaylistGtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistGtComparator(const int value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toInt() > search_term_;
  }
 private:
  int search_term_;
};

class PlaylistGeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistGeComparator(const int value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toInt() >= search_term_;
  }
 private:
  int search_term_;
};

class PlaylistLtComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLtComparator(const int value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toInt() < search_term_;
  }
 private:
  int search_term_;
};

class PlaylistLeComparator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistLeComparator(const int value) : search_term_(value) {}
  bool Matches(const QString &element) const override {
    return element.toInt() <= search_term_;
  }
 private:
  int search_term_;
};

// The length field of the playlist (entries) contains a song's running time in nanoseconds.
// However, We don't really care about nanoseconds, just seconds.
// Thus, with this decorator we drop the last 9 digits, if that many are present.
class PlaylistDropTailComparatorDecorator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistDropTailComparatorDecorator(PlaylistSearchTermComparator *cmp) : cmp_(cmp) {}

  bool Matches(const QString &element) const override {
    if (element.length() > 9) {
      return cmp_->Matches(element.left(element.length() - 9));
    }
    else {
      return cmp_->Matches(element);
    }
  }
 private:
  QScopedPointer<PlaylistSearchTermComparator> cmp_;
};

class PlaylistRatingComparatorDecorator : public PlaylistSearchTermComparator {
 public:
  explicit PlaylistRatingComparatorDecorator(PlaylistSearchTermComparator *cmp) : cmp_(cmp) {}
  bool Matches(const QString &element) const override {
    return cmp_->Matches(QString::number(lround(element.toDouble() * 10.0)));
  }
 private:
  QScopedPointer<PlaylistSearchTermComparator> cmp_;
};

// Filter that applies a SearchTermComparator to all fields of a playlist entry
class PlaylistFilterTerm : public PlaylistFilterTree {
 public:
  explicit PlaylistFilterTerm(PlaylistSearchTermComparator *comparator, const QList<int> &columns) : cmp_(comparator), columns_(columns) {}

  bool accept(const int row, const QModelIndex &parent, const QAbstractItemModel *const model) const override {
    for (const int i : columns_) {
      const QModelIndex idx = model->index(row, i, parent);
      if (cmp_->Matches(idx.data().toString().toLower())) return true;
    }
    return false;
  }
  FilterType type() override { return FilterType::Term; }
 private:
  QScopedPointer<PlaylistSearchTermComparator> cmp_;
  QList<int> columns_;
};

// Filter that applies a SearchTermComparator to one specific field of a playlist entry
class PlaylistFilterColumnTerm : public PlaylistFilterTree {
 public:
  PlaylistFilterColumnTerm(const int column, PlaylistSearchTermComparator *comparator) : col(column), cmp_(comparator) {}

  bool accept(const int row, const QModelIndex &parent, const QAbstractItemModel *const model) const override {
    const QModelIndex idx = model->index(row, col, parent);
    return cmp_->Matches(idx.data().toString().toLower());
  }
  FilterType type() override { return FilterType::Column; }
 private:
  int col;
  QScopedPointer<PlaylistSearchTermComparator> cmp_;
};

class PlaylistNotFilter : public PlaylistFilterTree {
 public:
  explicit PlaylistNotFilter(const PlaylistFilterTree *inv) : child_(inv) {}

  bool accept(const int row, const QModelIndex &parent, const QAbstractItemModel *const model) const override {
    return !child_->accept(row, parent, model);
  }
  FilterType type() override { return FilterType::Not; }
 private:
  QScopedPointer<const PlaylistFilterTree> child_;
};

class PlaylistOrFilter : public PlaylistFilterTree {
 public:
  ~PlaylistOrFilter() override { qDeleteAll(children_); }
  virtual void add(PlaylistFilterTree *child) { children_.append(child); }
  bool accept(const int row, const QModelIndex &parent, const QAbstractItemModel *const model) const override {
    return std::any_of(children_.begin(), children_.end(), [row, parent, model](PlaylistFilterTree *child) { return child->accept(row, parent, model); });
  }
  FilterType type() override { return FilterType::Or; }
 private:
  QList<PlaylistFilterTree*> children_;
};

class PlaylistAndFilter : public PlaylistFilterTree {
 public:
  ~PlaylistAndFilter() override { qDeleteAll(children_); }
  virtual void add(PlaylistFilterTree *child) { children_.append(child); }
  bool accept(const int row, const QModelIndex &parent, const QAbstractItemModel *const model) const override {
    return !std::any_of(children_.begin(), children_.end(), [row, parent, model](PlaylistFilterTree *child) { return !child->accept(row, parent, model); });
  }
  FilterType type() override { return FilterType::And; }
 private:
  QList<PlaylistFilterTree*> children_;
};

PlaylistFilterParser::PlaylistFilterParser(const QString &filter, const QMap<QString, int> &columns, const QSet<int> &numerical_cols) : iter_{}, end_{}, filterstring_(filter), columns_(columns), numerical_columns_(numerical_cols) {}

PlaylistFilterTree *PlaylistFilterParser::parse() {
  iter_ = filterstring_.constBegin();
  end_ = filterstring_.constEnd();
  return parseOrGroup();
}

void PlaylistFilterParser::advance() {

  while (iter_ != end_ && iter_->isSpace()) {
    ++iter_;
  }

}

PlaylistFilterTree *PlaylistFilterParser::parseOrGroup() {

  advance();
  if (iter_ == end_) return new PlaylistNopFilter;

  PlaylistOrFilter *group = new PlaylistOrFilter;
  group->add(parseAndGroup());
  advance();
  while (checkOr()) {
    group->add(parseAndGroup());
    advance();
  }

  return group;

}

PlaylistFilterTree *PlaylistFilterParser::parseAndGroup() {

  advance();
  if (iter_ == end_) return new PlaylistNopFilter;

  PlaylistAndFilter *group = new PlaylistAndFilter();
  do {
    group->add(parseSearchExpression());
    advance();
    if (iter_ != end_ && *iter_ == QLatin1Char(')')) break;
    if (checkOr(false)) {
      break;
    }
    checkAnd();  // if there's no 'AND', we'll add the term anyway...
  } while (iter_ != end_);

  return group;

}

bool PlaylistFilterParser::checkAnd() {

  if (iter_ != end_) {
    if (*iter_ == QLatin1Char('A')) {
      buf_ += *iter_;
      ++iter_;
      if (iter_ != end_ && *iter_ == QLatin1Char('N')) {
        buf_ += *iter_;
        ++iter_;
        if (iter_ != end_ && *iter_ == QLatin1Char('D')) {
          buf_ += *iter_;
          ++iter_;
          if (iter_ != end_ && (iter_->isSpace() || *iter_ == QLatin1Char('-') || *iter_ == QLatin1Char('('))) {
            advance();
            buf_.clear();
            return true;
          }
        }
      }
    }
  }

  return false;

}

bool PlaylistFilterParser::checkOr(const bool step_over) {

  if (!buf_.isEmpty()) {
    if (buf_ == QLatin1String("OR")) {
      if (step_over) {
        buf_.clear();
        advance();
      }
      return true;
    }
  }
  else {
    if (iter_ != end_) {
      if (*iter_ == QLatin1Char('O')) {
        buf_ += *iter_;
        ++iter_;
        if (iter_ != end_ && *iter_ == QLatin1Char('R')) {
          buf_ += *iter_;
          ++iter_;
          if (iter_ != end_ && (iter_->isSpace() || *iter_ == QLatin1Char('-') || *iter_ == QLatin1Char('('))) {
            if (step_over) {
              buf_.clear();
              advance();
            }
            return true;
          }
        }
      }
    }
  }

  return false;

}

PlaylistFilterTree *PlaylistFilterParser::parseSearchExpression() {

  advance();
  if (iter_ == end_) return new PlaylistNopFilter;
  if (*iter_ == QLatin1Char('(')) {
    ++iter_;
    advance();
    PlaylistFilterTree *tree = parseOrGroup();
    advance();
    if (iter_ != end_) {
      if (*iter_ == QLatin1Char(')')) {
        ++iter_;
      }
    }
    return tree;
  }
  else if (*iter_ == QLatin1Char('-')) {
    ++iter_;
    PlaylistFilterTree *tree = parseSearchExpression();
    if (tree->type() != PlaylistFilterTree::FilterType::Nop) return new PlaylistNotFilter(tree);
    return tree;
  }
  else {
    return parseSearchTerm();
  }

}

PlaylistFilterTree *PlaylistFilterParser::parseSearchTerm() {

  QString col;
  QString search;
  QString prefix;
  bool inQuotes = false;
  for (; iter_ != end_; ++iter_) {
    if (inQuotes) {
      if (*iter_ == QLatin1Char('"')) {
        inQuotes = false;
      }
      else {
        buf_ += *iter_;
      }
    }
    else {
      if (*iter_ == QLatin1Char('"')) {
        inQuotes = true;
      }
      else if (col.isEmpty() && *iter_ == QLatin1Char(':')) {
        col = buf_.toLower();
        buf_.clear();
        prefix.clear();  // prefix isn't allowed here - let's ignore it
      }
      else if (iter_->isSpace() || *iter_ == QLatin1Char('(') || *iter_ == QLatin1Char(')') || *iter_ == QLatin1Char('-')) {
        break;
      }
      else if (buf_.isEmpty()) {
        // we don't know whether there is a column part in this search term thus we assume the latter and just try and read a prefix
        if (prefix.isEmpty() && (*iter_ == QLatin1Char('>') || *iter_ == QLatin1Char('<') || *iter_ == QLatin1Char('=') || *iter_ == QLatin1Char('!'))) {
          prefix += *iter_;
        }
        else if (prefix != QLatin1Char('=') && *iter_ == QLatin1Char('=')) {
          prefix += *iter_;
        }
        else {
          buf_ += *iter_;
        }
      }
      else {
        buf_ += *iter_;
      }
    }
  }

  search = buf_.toLower();
  buf_.clear();

  return createSearchTermTreeNode(col, prefix, search);

}

PlaylistFilterTree *PlaylistFilterParser::createSearchTermTreeNode(const QString &col, const QString &prefix, const QString &search) const {

  if (search.isEmpty() && prefix != QLatin1Char('=')) {
    return new PlaylistNopFilter;
  }

  PlaylistSearchTermComparator *cmp = nullptr;

  // Handle the float based Rating Column
  if (columns_[col] == static_cast<int>(Playlist::Column::Rating)) {
    float parsed_search = Utilities::ParseSearchRating(search);

    if (prefix == QLatin1Char('=')) {
      cmp = new PlaylistFloatEqComparator(parsed_search);
    }
    else if (prefix == QLatin1String("!=") || prefix == QLatin1String("<>")) {
      cmp = new PlaylistFloatNeComparator(parsed_search);
    }
    else if (prefix == QLatin1Char('>')) {
      cmp = new PlaylistFloatGtComparator(parsed_search);
    }
    else if (prefix == QLatin1String(">=")) {
      cmp = new PlaylistFloatGeComparator(parsed_search);
    }
    else if (prefix == QLatin1Char('<')) {
      cmp = new PlaylistFloatLtComparator(parsed_search);
    }
    else if (prefix == QLatin1String("<=")) {
      cmp = new PlaylistFloatLeComparator(parsed_search);
    }
    else {
      cmp = new PlaylistFloatEqComparator(parsed_search);
    }
  }
  else if (prefix == QLatin1String("!=") || prefix == QLatin1String("<>")) {
    cmp = new PlaylistNeComparator(search);
  }
  else if (!col.isEmpty() && columns_.contains(col) && numerical_columns_.contains(columns_[col])) {
    // The length column contains the time in seconds (nanoseconds, actually - the "nano" part is handled by the DropTailComparatorDecorator,  though).
    int search_value = 0;
    if (columns_[col] == static_cast<int>(Playlist::Column::Length)) {
      search_value = Utilities::ParseSearchTime(search);
    }
    else {
      search_value = search.toInt();
    }
    // Alright, back to deciding which comparator we'll use
    if (prefix == QLatin1Char('>')) {
      cmp = new PlaylistGtComparator(search_value);
    }
    else if (prefix == QLatin1String(">=")) {
      cmp = new PlaylistGeComparator(search_value);
    }
    else if (prefix == QLatin1Char('<')) {
      cmp = new PlaylistLtComparator(search_value);
    }
    else if (prefix == QLatin1String("<=")) {
      cmp = new PlaylistLeComparator(search_value);
    }
    else {
      // Convert back because for time/rating
      cmp = new PlaylistEqComparator(QString::number(search_value));
    }
  }
  else {
    if (prefix == QLatin1Char('=')) {
      cmp = new PlaylistEqComparator(search);
    }
    else if (prefix == QLatin1Char('>')) {
      cmp = new PlaylistLexicalGtComparator(search);
    }
    else if (prefix == QLatin1String(">=")) {
      cmp = new PlaylistLexicalGeComparator(search);
    }
    else if (prefix == QLatin1Char('<')) {
      cmp = new PlaylistLexicalLtComparator(search);
    }
    else if (prefix == QLatin1String("<=")) {
      cmp = new PlaylistLexicalLeComparator(search);
    }
    else {
      cmp = new PlaylistDefaultComparator(search);
    }
  }

  if (columns_.contains(col)) {
    if (columns_[col] == static_cast<int>(Playlist::Column::Length)) {
      cmp = new PlaylistDropTailComparatorDecorator(cmp);
    }
    return new PlaylistFilterColumnTerm(columns_[col], cmp);
  }
  else {
    return new PlaylistFilterTerm(cmp, columns_.values());
  }

}
