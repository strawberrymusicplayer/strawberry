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

#ifndef LOGGING_ENV_H
#define LOGGING_ENV_H

#include "gtest_include.h"

#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

class LoggingEnvironment : public ::testing::Environment {
 public:
  LoggingEnvironment() = default;
  void SetUp() override {
    logging::Init();
    logging::SetLevels(u"*:4"_s);
  }
 private:
  Q_DISABLE_COPY(LoggingEnvironment)
};

#endif  // LOGGING_ENV_H
