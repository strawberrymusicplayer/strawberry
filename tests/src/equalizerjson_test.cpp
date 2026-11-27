/*
 * Strawberry Music Player
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

#include "gtest_include.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "equalizer/equalizer.h"
#include "equalizer/equalizerjson.h"

using namespace Qt::Literals::StringLiterals;

namespace {

EqualizerJson::State ExampleState() {

  EqualizerJson::State state;
  state.presets.insert(u"Custom"_s, Equalizer::Params(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -5));
  state.presets.insert(u"Rock"_s, Equalizer::Params(40, 25, -30, -40, -20, 20, 45, 55, 55, 55));
  state.selected_preset = u"Rock"_s;
  state.current = Equalizer::Params(-100, 0, 0, 0, 0, 0, 0, 0, 0, 100, 100);
  state.enabled = true;
  state.stereo_balancer_enabled = true;
  state.stereo_balance = -75;
  return state;

}

QJsonObject ExampleObject() {
  return QJsonDocument::fromJson(EqualizerJson::ToJson(ExampleState())).object();
}

QByteArray ToBytes(const QJsonObject &object) {
  return QJsonDocument(object).toJson();
}

}  // namespace

TEST(EqualizerJsonTest, RoundTripKeepsAllSettings) {

  const QByteArray json = EqualizerJson::ToJson(ExampleState());
  const QJsonObject object = QJsonDocument::fromJson(json).object();
  EXPECT_EQ(u"strawberry-equalizer"_s, object.value(u"format"_s).toString());
  EXPECT_EQ(1, object.value(u"version"_s).toInt());

  EqualizerJson::State state;
  QString error;
  ASSERT_TRUE(EqualizerJson::FromJson(json, &state, &error)) << error.toStdString();
  const EqualizerJson::State expected = ExampleState();
  EXPECT_EQ(expected.presets, state.presets);
  EXPECT_EQ(expected.selected_preset, state.selected_preset);
  EXPECT_EQ(expected.current, state.current);
  EXPECT_EQ(expected.enabled, state.enabled);
  EXPECT_EQ(expected.stereo_balancer_enabled, state.stereo_balancer_enabled);
  EXPECT_EQ(expected.stereo_balance, state.stereo_balance);

}

TEST(EqualizerJsonTest, RejectsInvalidDocumentsWithoutChangingState) {

  const auto rejects = [](const QByteArray &json) {
    EqualizerJson::State state;
    state.selected_preset = u"unchanged"_s;
    QString error;
    const bool accepted = EqualizerJson::FromJson(json, &state, &error);
    EXPECT_EQ(u"unchanged"_s, state.selected_preset);
    return !accepted && !error.isEmpty();
  };

  EXPECT_TRUE(rejects("not json"));
  EXPECT_TRUE(rejects("[]"));

  QJsonObject object = ExampleObject();
  object[u"format"_s] = u"something-else"_s;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  object[u"version"_s] = 2;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  object.remove(u"enabled"_s);
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  object[u"selected_preset"_s] = u"Missing"_s;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  QJsonObject current = object.value(u"current"_s).toObject();
  current[u"gains"_s] = QJsonArray({0, 0, 0});
  object[u"current"_s] = current;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  current = object.value(u"current"_s).toObject();
  current[u"preamp"_s] = 101;
  object[u"current"_s] = current;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  current = object.value(u"current"_s).toObject();
  current[u"preamp"_s] = 1.5;
  object[u"current"_s] = current;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  QJsonObject balancer = object.value(u"stereo_balancer"_s).toObject();
  balancer[u"balance"_s] = u"left"_s;
  object[u"stereo_balancer"_s] = balancer;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  QJsonArray presets = object.value(u"presets"_s).toArray();
  presets.append(presets.first());
  object[u"presets"_s] = presets;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  presets = object.value(u"presets"_s).toArray();
  QJsonObject unnamed = presets.first().toObject();
  unnamed[u"name"_s] = u"  "_s;
  presets[0] = unnamed;
  object[u"presets"_s] = presets;
  EXPECT_TRUE(rejects(ToBytes(object)));

  object = ExampleObject();
  object[u"presets"_s] = QJsonArray();
  EXPECT_TRUE(rejects(ToBytes(object)));

  EXPECT_TRUE(rejects(QByteArray(EqualizerJson::kMaximumFileSize + 1, ' ')));

}

TEST(EqualizerJsonTest, ConflictsOnlyListChangedExistingPresets) {

  QMap<QString, Equalizer::Params> existing;
  existing.insert(u"Rock"_s, Equalizer::Params(40, 25, -30, -40, -20, 20, 45, 55, 55, 55));
  existing.insert(u"Mine"_s, Equalizer::Params(1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
  QMap<QString, Equalizer::Params> imported;
  imported.insert(u"Rock"_s, existing.value(u"Rock"_s));
  imported.insert(u"Mine"_s, Equalizer::Params(2, 2, 2, 2, 2, 2, 2, 2, 2, 2));
  imported.insert(u"New"_s, Equalizer::Params());
  EXPECT_EQ(QStringList({u"Mine"_s}), EqualizerJson::ConflictingPresets(existing, imported));

}
