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

#include <QObject>

#include "tagreaderresult.h"

QString TagReaderResult::error_string() const {

  switch (error_code) {
    case ErrorCode::Success:
      return QObject::tr("Success");
    case ErrorCode::Unsupported:
      return QObject::tr("File is unsupported");
    case ErrorCode::FilenameMissing:
      return QObject::tr("Filename is missing");
    case ErrorCode::FileDoesNotExist:
      return QObject::tr("File does not exist");
    case ErrorCode::FileOpenError:
      return QObject::tr("File could not be opened");
    case ErrorCode::FileParseError:
      return QObject::tr("Could not parse file");
    case ErrorCode::FileSaveError:
      return QObject::tr("Could save file");
    case ErrorCode::CustomError:
      return error_text;
  }

  return QObject::tr("Unknown error");

}
