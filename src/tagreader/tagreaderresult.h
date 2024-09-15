/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERRESULT_H
#define TAGREADERRESULT_H

#include <QString>

class TagReaderResult {
 public:
  enum class ErrorCode {
    Success,
    Unsupported,
    FilenameMissing,
    FileDoesNotExist,
    FileOpenError,
    FileParseError,
    FileSaveError,
    CustomError,
  };
  TagReaderResult(const ErrorCode _error_code = ErrorCode::Unsupported, const QString &_error_text = QString()) : error_code(_error_code), error_text(_error_text) {}
  ErrorCode error_code;
  QString error_text;
  bool success() const { return error_code == ErrorCode::Success; }
  QString error_string() const;
};

#endif // TAGREADERRESULT_H
