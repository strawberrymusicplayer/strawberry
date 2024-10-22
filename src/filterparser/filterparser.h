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

#ifndef FILTERPARSER_H
#define FILTERPARSER_H

#include <QString>

class FilterTree;

// A utility class to parse search filter strings into a decision tree
// that can decide whether a song matches the filter.
//
// Here's a grammar describing the filters we expect:
//   　expr      ::= or-group
//     or-group  ::= and-group ('OR' and-group)*
//     and-group ::= sexpr ('AND' sexpr)*
//     sexpr     ::= sterm | '-' sexpr | '(' or-group ')'
//     sterm     ::= col ':' sstring | sstring
//     sstring   ::= prefix? string
//     string    ::= [^:-()" ]+ | '"' [^"]+ '"'
//     prefix    ::= '=' | '<' | '>' | '<=' | '>='
//     col       ::= "title" | "artist" | ...
class FilterParser {
 public:
  explicit FilterParser(const QString &filter_string);

  FilterTree *parse();

  static QString ToolTip();

 protected:
  void advance();
  // Check if iter is at the start of 'AND' if so, step over it and return true if not, return false and leave iter where it was
  bool checkAnd();
  // Check if iter is at the start of 'OR'
  bool checkOr(const bool step_over = true);

  FilterTree *parseOrGroup();
  FilterTree *parseAndGroup();
  FilterTree *parseSearchExpression();
  FilterTree *parseSearchTerm();

  FilterTree *createSearchTermTreeNode(const QString &column, const QString &prefix, const QString &value) const;

  static qint64 ParseTime(const QString &time_str);
  static float ParseRating(const QString &rating_str);

  const QString filter_string_;
  QString::const_iterator iter_;
  QString::const_iterator end_;
  QString buf_;
};

#endif  // FILTERPARSER_H
