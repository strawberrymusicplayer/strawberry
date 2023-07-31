/*
 * Strawberry Music Player
 * Copyright 2019-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include "searchparserutils.h"

namespace Utilities {

/**
 * @brief Try and parse the string as '[[h:]m:]s' (ignoring all spaces),
 * and return the number of seconds if it parses correctly.
 * If not, the original string is returned.
 * The 'h', 'm' and 's' components can have any length (including 0).
 * A few examples:
 *  "::"       is parsed to "0"
 *  "1::"      is parsed to "3600"
 *  "3:45"     is parsed to "225"
 *  "1:165"    is parsed to "225"
 *  "225"      is parsed to "225" (srsly! ^.^)
 *  "2:3:4:5"  is parsed to "2:3:4:5"
 *  "25m"      is parsed to "25m"
 * @param time_str
 * @return
 */
int ParseSearchTime(const QString &time_str) {

  int seconds = 0;
  int accum = 0;
  int colon_count = 0;
  for (const QChar &c : time_str) {
    if (c.isDigit()) {
      accum = accum * 10 + c.digitValue();
    }
    else if (c == ':') {
      seconds = seconds * 60 + accum;
      accum = 0;
      ++colon_count;
      if (colon_count > 2) {
        return 0;
      }
    }
    else if (!c.isSpace()) {
      return 0;
    }
  }
  seconds = seconds * 60 + accum;

  return seconds;

}

/**
 * @brief Parses a rating search term to float.
 *  If the rating is a number from 0-5, map it to 0-1
 *  To use float values directly, the search term can be prefixed with "f" (rating:>f0.2)
 *  If search str is 0, or by default, uses -1
 * @param rating_str: Rating search 0-5, or "f0.2"
 * @return float: rating from 0-1 or -1 if not rated.
 */
float ParseSearchRating(const QString &rating_str) {

  if (rating_str.isEmpty()) {
    return -1;
  }
  float rating = -1.0F;
  bool ok = false;
  float rating_input = rating_str.toFloat(&ok);
  // is valid int from 0-5: convert to float
  if (ok && rating_input >= 0 && rating_input <= 5) {
    rating = rating_input / 5.0F;
  }

  // check if the search is a float
  else if (rating_str.at(0) == 'f') {
    QString rating_float = rating_str;
    rating_float = rating_float.remove(0, 1);

    ok = false;
    rating_float.toFloat(&ok);
    if (ok) {
      rating = rating_float.toFloat(&ok);
    }
  }
  // Songs with zero rating have -1 in the DB
  if (rating == 0) {
    rating = -1;
  }

  return rating;

}


}  // namespace Utilities
