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

#ifndef MOCK_SETTINGSPROVIDER_H
#define MOCK_SETTINGSPROVIDER_H

#include "gmock_include.h"

#include "core/settingsprovider.h"

// clazy:excludeall=function-args-by-value

class MockSettingsProvider : public SettingsProvider {
 public:
  MOCK_METHOD1(set_group, void(const char *group));
  MOCK_CONST_METHOD2(value, QVariant(const QString &key, const QVariant &default_value));
  MOCK_METHOD2(setValue, void(const QString &key, const QVariant &value));
  MOCK_METHOD1(beginReadArray, int(const QString &prefix));
  MOCK_METHOD2(beginWriteArray, void(const QString &prefix, int size));
  MOCK_METHOD1(setArrayIndex, void(int i));
  MOCK_METHOD0(endArray, void());  // clazy:exclude=returning-void-expression
};

class DummySettingsProvider : public SettingsProvider {
 public:
  DummySettingsProvider() {}

  void set_group(const char*) {}

  QVariant value(const QString&, const QVariant& = QVariant()) const { return QVariant(); }
  void setValue(const QString&, const QVariant&) {}
  int beginReadArray(const QString&) { return 0; }
  void beginWriteArray(const QString&, int = -1) {}
  void setArrayIndex(int) {}
  void endArray() {}

};

#endif  // MOCK_SETTINGSPROVIDER_H
