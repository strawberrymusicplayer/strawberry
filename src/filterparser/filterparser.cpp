/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2023, Daniel Ostertag <daniel.ostertag@dakes.de>
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

#include <QString>

#include "filterparser.h"
#include "filtertreenop.h"
#include "filtertreeand.h"
#include "filtertreeor.h"
#include "filtertreenot.h"
#include "filtertreeterm.h"
#include "filtertreecolumnterm.h"
#include "filterparsersearchcomparators.h"

using namespace Qt::Literals::StringLiterals;

FilterParser::FilterParser(const QString &filter_string) : filter_string_(filter_string), iter_{}, end_{} {}

FilterTree *FilterParser::parse() {

  iter_ = filter_string_.constBegin();
  end_ = filter_string_.constEnd();

  return parseOrGroup();

}

void FilterParser::advance() {

  while (iter_ != end_ && iter_->isSpace()) {
    ++iter_;
  }

}

FilterTree *FilterParser::parseOrGroup() {

  advance();
  if (iter_ == end_) return new FilterTreeNop;

  FilterTreeOr *group = new FilterTreeOr;
  group->add(parseAndGroup());
  advance();
  while (checkOr()) {
    group->add(parseAndGroup());
    advance();
  }

  return group;

}

FilterTree *FilterParser::parseAndGroup() {

  advance();
  if (iter_ == end_) return new FilterTreeNop;

  FilterTreeAnd *group = new FilterTreeAnd();
  do {
    group->add(parseSearchExpression());
    advance();
    if (iter_ != end_ && *iter_ == u')') break;
    if (checkOr(false)) {
      break;
    }
    checkAnd();  // If there's no 'AND', we'll add the term anyway...
  } while (iter_ != end_);

  return group;

}

bool FilterParser::checkAnd() {

  QString::const_iterator and_iter = iter_;

  if (and_iter != end_) {
    if (*and_iter == u'A') {
      ++and_iter;
      if (and_iter != end_ && *and_iter == u'N') {
        ++and_iter;
        if (and_iter != end_ && *and_iter == u'D') {
          ++and_iter;
          if (and_iter != end_ && (and_iter->isSpace() || *and_iter == u'-' || *and_iter == u'(')) {
            iter_ = and_iter;
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

bool FilterParser::checkOr(const bool step_over) {

  if (!buf_.isEmpty()) {
    if (buf_ == "OR"_L1) {
      if (step_over) {
        buf_.clear();
        advance();
      }
      return true;
    }
  }
  else {
    QString::const_iterator or_iter = iter_;
    if (or_iter != end_) {
      if (*or_iter == u'O') {
        ++or_iter;
        if (or_iter != end_ && *or_iter == u'R') {
          ++or_iter;
          if (or_iter != end_ && (or_iter->isSpace() || *or_iter == u'-' || *or_iter == u'(')) {
            iter_ = or_iter;
            if (step_over) {
              buf_.clear();
              advance();
            }
            else {
              buf_ += "OR"_L1;
            }
            return true;
          }
        }
      }
    }
  }

  return false;

}

FilterTree *FilterParser::parseSearchExpression() {

  advance();
  if (iter_ == end_) return new FilterTreeNop;
  if (*iter_ == u'(') {
    ++iter_;
    advance();
    FilterTree *tree = parseOrGroup();
    advance();
    if (iter_ != end_) {
      if (*iter_ == u')') {
        ++iter_;
      }
    }
    return tree;
  }
  else if (*iter_ == u'-') {
    ++iter_;
    FilterTree *tree = parseSearchExpression();
    if (tree->type() != FilterTree::FilterType::Nop) return new FilterTreeNot(tree);
    return tree;
  }
  else {
    return parseSearchTerm();
  }

}

FilterTree *FilterParser::parseSearchTerm() {

  QString column;
  QString prefix;
  QString value;

  bool in_quotes = false;
  bool previous_char_operator = false;

  for (; iter_ != end_; ++iter_) {
    if (previous_char_operator) {
      if (iter_->isSpace()) {
        continue;
      }
      previous_char_operator = false;
    }
    if (in_quotes) {
      if (*iter_ == u'"') {
        in_quotes = false;
      }
      else {
        buf_ += *iter_;
      }
    }
    else {
      if (*iter_ == u'"') {
        in_quotes = true;
      }
      else if (column.isEmpty() && *iter_ == u':') {
        column = buf_.toLower();
        buf_.clear();
        prefix.clear();  // Prefix isn't allowed here - let's ignore it
        previous_char_operator = true;
      }
      else if (iter_->isSpace() || *iter_ == u'(' || *iter_ == u')' || *iter_ == u'-') {
        break;
      }
      else if (buf_.isEmpty()) {
        // We don't know whether there is a column part in this search term thus we assume the latter and just try and read a prefix
        if (prefix.isEmpty() && (*iter_ == u'>' || *iter_ == u'<' || *iter_ == u'=' || *iter_ == u'!')) {
          prefix += *iter_;
          previous_char_operator = true;
        }
        else if (prefix != u'=' && *iter_ == u'=') {
          prefix += *iter_;
          previous_char_operator = true;
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

  value = buf_.toLower();
  buf_.clear();

  return createSearchTermTreeNode(column, prefix, value);

}

FilterTree *FilterParser::createSearchTermTreeNode(const QString &column, const QString &prefix, const QString &value) const {

  if (value.isEmpty() && prefix != u'=') {
    return new FilterTreeNop;
  }

  FilterParserSearchTermComparator *cmp = nullptr;

  if (!column.isEmpty()) {
    if (Song::kTextSearchColumns.contains(column, Qt::CaseInsensitive)) {
      if (prefix == u'=' || prefix == "=="_L1) {
        cmp = new FilterParserTextEqComparator(value);
      }
      else if (prefix == "!="_L1 || prefix == "<>"_L1) {
        cmp = new FilterParserTextNeComparator(value);
      }
      else {
        cmp = new FilterParserTextContainsComparator(value);
      }
    }
    else if (Song::kIntSearchColumns.contains(column, Qt::CaseInsensitive)) {
      bool ok = false;
      int number = value.toInt(&ok);
      if (ok) {
        if (prefix == u'=' || prefix == "=="_L1) {
          cmp = new FilterParserIntEqComparator(number);
        }
        else if (prefix == "!="_L1 || prefix == "<>"_L1) {
          cmp = new FilterParserIntNeComparator(number);
        }
        else if (prefix == u'>') {
          cmp = new FilterParserIntGtComparator(number);
        }
        else if (prefix == ">="_L1) {
          cmp = new FilterParserIntGeComparator(number);
        }
        else if (prefix == u'<') {
          cmp = new FilterParserIntLtComparator(number);
        }
        else if (prefix == "<="_L1) {
          cmp = new FilterParserIntLeComparator(number);
        }
        else {
          cmp = new FilterParserIntEqComparator(number);
        }
      }
    }
    else if (Song::kUIntSearchColumns.contains(column, Qt::CaseInsensitive)) {
      bool ok = false;
      uint number = value.toUInt(&ok);
      if (ok) {
        if (prefix == u'=' || prefix == "=="_L1) {
          cmp = new FilterParserUIntEqComparator(number);
        }
        else if (prefix == "!="_L1 || prefix == "<>"_L1) {
          cmp = new FilterParserUIntNeComparator(number);
        }
        else if (prefix == u'>') {
          cmp = new FilterParserUIntGtComparator(number);
        }
        else if (prefix == ">="_L1) {
          cmp = new FilterParserUIntGeComparator(number);
        }
        else if (prefix == u'<') {
          cmp = new FilterParserUIntLtComparator(number);
        }
        else if (prefix == "<="_L1) {
          cmp = new FilterParserUIntLeComparator(number);
        }
        else {
          cmp = new FilterParserUIntEqComparator(number);
        }
      }
    }
    else if (Song::kInt64SearchColumns.contains(column, Qt::CaseInsensitive)) {
      qint64 number = 0;
      if (column == "length"_L1) {
        number = ParseTime(value);
      }
      else {
        number = value.toLongLong();
      }
      if (prefix == u'=' || prefix == "=="_L1) {
        cmp = new FilterParserInt64EqComparator(number);
      }
      else if (prefix == "!="_L1 || prefix == "<>"_L1) {
        cmp = new FilterParserInt64NeComparator(number);
      }
      else if (prefix == u'>') {
        cmp = new FilterParserInt64GtComparator(number);
      }
      else if (prefix == ">="_L1) {
        cmp = new FilterParserInt64GeComparator(number);
      }
      else if (prefix == u'<') {
        cmp = new FilterParserInt64LtComparator(number);
      }
      else if (prefix == "<="_L1) {
        cmp = new FilterParserInt64LeComparator(number);
      }
      else {
        cmp = new FilterParserInt64EqComparator(number);
      }
    }
    else if (Song::kFloatSearchColumns.contains(column, Qt::CaseInsensitive)) {
      const float rating = ParseRating(value);
      if (prefix == u'=' || prefix == "=="_L1) {
        cmp = new FilterParserFloatEqComparator(rating);
      }
      else if (prefix == "!="_L1 || prefix == "<>"_L1) {
        cmp = new FilterParserFloatNeComparator(rating);
      }
      else if (prefix == u'>') {
        cmp = new FilterParserFloatGtComparator(rating);
      }
      else if (prefix == ">="_L1) {
        cmp = new FilterParserFloatGeComparator(rating);
      }
      else if (prefix == u'<') {
        cmp = new FilterParserFloatLtComparator(rating);
      }
      else if (prefix == "<="_L1) {
        cmp = new FilterParserFloatLeComparator(rating);
      }
      else {
        cmp = new FilterParserFloatEqComparator(rating);
      }
    }
  }

  if (cmp) {
    return new FilterTreeColumnTerm(column, cmp);
  }

  return new FilterTreeTerm(new FilterParserTextContainsComparator(value));

}

// Try and parse the string as '[[h:]m:]s' (ignoring all spaces),
// and return the number of seconds if it parses correctly.
// If not, the original string is returned.
// The 'h', 'm' and 's' components can have any length (including 0).
// A few examples:
//  "::"       is parsed to "0"
//  "1::"      is parsed to "3600"
//  "3:45"     is parsed to "225"
//  "1:165"    is parsed to "225"
//  "225"      is parsed to "225" (srsly! ^.^)
//  "2:3:4:5"  is parsed to "2:3:4:5"
//  "25m"      is parsed to "25m"

qint64 FilterParser::ParseTime(const QString &time_str) {

  qint64 seconds = 0;
  qint64 accum = 0;
  qint64 colon_count = 0;
  for (const QChar &c : time_str) {
    if (c.isDigit()) {
      accum = accum * 10LL + static_cast<qint64>(c.digitValue());
    }
    else if (c == u':') {
      seconds = seconds * 60LL + accum;
      accum = 0LL;
      ++colon_count;
      if (colon_count > 2) {
        return 0LL;
      }
    }
    else if (!c.isSpace()) {
      return 0LL;
    }
  }
  seconds = seconds * 60LL + accum;

  return seconds;

}

// Parses a rating search term to float.
//  If the rating is a number from 0-5, map it to 0-1
//  To use float values directly, the search term can be prefixed with "f" (rating:>f0.2)
//  If search string is 0, or by default, uses -1
// @param rating_str: Rating search 0-5, or "f0.2"
// @return float: rating from 0-1 or -1 if not rated.

float FilterParser::ParseRating(const QString &rating_str) {

  if (rating_str.isEmpty()) {
    return -1;
  }

  float rating = -1.0F;

  // Check if the search is a float
  if (rating_str.contains(u'f', Qt::CaseInsensitive)) {
    if (rating_str.count(u'f', Qt::CaseInsensitive) > 1) {
      return rating;
    }
    QString rating_float_str = rating_str;
    if (rating_str.at(0) == u'f' || rating_str.at(0) == u'F') {
      rating_float_str = rating_float_str.remove(0, 1);
    }
    if (rating_str.right(1) == u'f' || rating_str.right(1) == u'F') {
      rating_float_str.chop(1);
    }
    bool ok = false;
    const float rating_input = rating_float_str.toFloat(&ok);
    if (ok) {
      rating = rating_input;
    }
  }
  else {
    bool ok = false;
    const int rating_input = rating_str.toInt(&ok);
    // Is valid int from 0-5: convert to float
    if (ok && rating_input >= 0 && rating_input <= 5) {
      rating = static_cast<float>(rating_input) / 5.0F;
    }
  }

  // Songs with zero rating have -1 in the DB
  if (rating == 0) {
    rating = -1;
  }

  return rating;

}

QString FilterParser::ToolTip() {

  return "<html><head/><body><p>"_L1 +
         QObject::tr("Prefix a search term with a field name to limit the search to that field, e.g.:") +
         u' ' +
         "<span style=\"font-weight:600;\">"_L1 +
         QObject::tr("artist") +
         ":</span><span style=\"font-style:italic;\">Strawbs</span> "_L1 +
         QObject::tr("searches for all artists containing the word %1.").arg("Strawbs"_L1) +
         "</p><p>"_L1 +

         QObject::tr("A word can be excluded with a preceding \"%1\", if you need to search for a word including \"%1\", place quotes around the word.").arg("-"_L1) +
         "</p><p>"_L1 +

         QObject::tr("Search terms for numerical fields can be prefixed with %1 or %2 to refine the search, e.g.: ")
                     .arg(" =, !=, &lt;, &gt;, &lt;="_L1, "&gt;="_L1) +
         "<span style=\"font-weight:600;\">"_L1 +
         QObject::tr("rating") +
         "</span>"_L1 +
         ":>="_L1 +
         "<span style=\"font-weight:italic;\">4</span>"_L1 +
         "</p><p>"_L1 +

         QObject::tr("Multiple search terms can also be combined with \"%1\" (default) and \"%2\", as well as grouped with parentheses.")
                     .arg("AND"_L1, "OR"_L1) +

         "</p><p><span style=\"font-weight:600;\">"_L1 +
         QObject::tr("Available fields") +
         ": "_L1 + "</span><span style=\"font-style:italic;\">"_L1 +
         Song::kSearchColumns.join(", "_L1) +
         "</span>."_L1 +
         "</p></body></html>"_L1;

}
