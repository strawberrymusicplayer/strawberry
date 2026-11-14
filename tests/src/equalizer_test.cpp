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

#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>

#include "core/settings.h"
#include "equalizer/equalizer.h"
#include "equalizer/equalizerjson.h"
#include "equalizer/equalizerslider.h"

using namespace Qt::Literals::StringLiterals;

class EqualizerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings settings;
    settings.beginGroup(u"Equalizer"_s);
    settings.remove(QString());
  }
};

TEST_F(EqualizerTest, SlidersKeepEngineGainRangeAndEmitLiveChanges) {

  Equalizer equalizer;
  const QList<EqualizerSlider *> sliders = equalizer.findChildren<EqualizerSlider *>();
  ASSERT_EQ(Equalizer::kBands + 1, sliders.count());
  QSignalSpy parameters(&equalizer, &Equalizer::EqualizerParametersChanged);
  sliders.first()->set_value(-50);
  EXPECT_EQ(-50, equalizer.preamp_value());
  ASSERT_EQ(1, parameters.count());
  sliders.at(1)->set_value(100);
  EXPECT_EQ(100, equalizer.gain_values().first());
  ASSERT_EQ(2, parameters.count());
  const QSlider *control = sliders.at(1)->findChild<QSlider *>();
  ASSERT_NE(nullptr, control);
  EXPECT_EQ(-100, control->minimum());
  EXPECT_EQ(100, control->maximum());
  EXPECT_EQ(u"12 dB"_s, sliders.at(1)->findChild<QLabel *>(u"gain"_s)->text());
  sliders.at(1)->set_value(-100);
  EXPECT_EQ(u"-24 dB"_s, sliders.at(1)->findChild<QLabel *>(u"gain"_s)->text());
  sliders.at(1)->set_value(-33);
  EXPECT_EQ(u"-7.92 dB"_s, sliders.at(1)->findChild<QLabel *>(u"gain"_s)->text());
  sliders.at(1)->set_value(7);
  EXPECT_EQ(u"0.84 dB"_s, sliders.at(1)->findChild<QLabel *>(u"gain"_s)->text());

}

TEST_F(EqualizerTest, SelectingPresetEmitsOneCompleteParameterUpdate) {

  Equalizer equalizer;
  QSignalSpy parameters(&equalizer, &Equalizer::EqualizerParametersChanged);
  QComboBox *preset = equalizer.findChild<QComboBox *>(u"preset"_s);
  ASSERT_NE(nullptr, preset);
  const int rock = preset->findData(u"Rock"_s);
  ASSERT_GE(rock, 0);
  preset->setCurrentIndex(rock);
  ASSERT_EQ(1, parameters.count());
  EXPECT_EQ(QList<int>({40, 25, -30, -40, -20, 20, 45, 55, 55, 55}), equalizer.gain_values());
  EXPECT_EQ(equalizer.gain_values(), parameters.first().at(1).value<QList<int>>());

}

TEST_F(EqualizerTest, PreampLabelMatchesLinearEngineMultiplier) {

  Equalizer equalizer;
  EqualizerSlider *preamp = equalizer.findChildren<EqualizerSlider *>().first();
  const QLabel *label = preamp->findChild<QLabel *>(u"gain"_s);
  ASSERT_NE(nullptr, label);
  preamp->set_value(-50);
  EXPECT_EQ(u"-6 dB"_s, label->text());
  preamp->set_value(100);
  EXPECT_EQ(u"6 dB"_s, label->text());
  preamp->set_value(-100);
  EXPECT_EQ(u"Muted"_s, label->text());
  preamp->set_value(0);
  EXPECT_EQ(u"0 dB"_s, label->text());

}

TEST_F(EqualizerTest, EnableAndBalanceSettingsPersistWithoutClosing) {

  {
    Equalizer equalizer;
    QCheckBox *enabled = equalizer.findChild<QCheckBox *>(u"enable_equalizer"_s);
    QCheckBox *balanced = equalizer.findChild<QCheckBox *>(u"enable_stereo_balancer"_s);
    QSlider *balance = equalizer.findChild<QSlider *>(u"stereo_balance_slider"_s);
    ASSERT_NE(nullptr, enabled);
    ASSERT_NE(nullptr, balanced);
    ASSERT_NE(nullptr, balance);
    QSignalSpy changes(&equalizer, &Equalizer::EqualizerEnabledChanged);
    enabled->setChecked(true);
    ASSERT_EQ(1, changes.count());
    balanced->setChecked(true);
    balance->setValue(-75);
    EXPECT_FLOAT_EQ(-0.75F, equalizer.stereo_balance());
  }
  Equalizer restored;
  EXPECT_TRUE(restored.is_equalizer_enabled());
  EXPECT_TRUE(restored.is_stereo_balancer_enabled());
  EXPECT_FLOAT_EQ(-0.75F, restored.stereo_balance());
  restored.findChild<QCheckBox *>(u"enable_stereo_balancer"_s)->setChecked(false);
  EXPECT_FLOAT_EQ(0.0F, restored.stereo_balance());

}

TEST_F(EqualizerTest, EditableDecibelsRoundToExistingEngineSteps) {

  EqualizerSlider band(u"600 Hz"_s);
  QSignalSpy changes(&band, &EqualizerSlider::ValueChanged);
  band.set_gain_db(1.0);
  EXPECT_EQ(8, band.value());
  EXPECT_NEAR(0.96, band.gain_db(), 0.001);
  EXPECT_EQ(1, changes.count());
  band.set_gain_db(-1.0);
  EXPECT_EQ(-4, band.value());
  EXPECT_NEAR(-0.96, band.gain_db(), 0.001);
  band.set_gain_db(-24.0);
  EXPECT_EQ(-100, band.value());
  band.set_gain_db(12.0);
  EXPECT_EQ(100, band.value());
  band.set_gain_db(0.0);
  EXPECT_EQ(0, band.value());

}

TEST_F(EqualizerTest, DisabledGainControlsExplainHowToEnableThem) {

  Equalizer equalizer;
  QCheckBox *enabled = equalizer.findChild<QCheckBox *>(u"enable_equalizer"_s);
  ASSERT_NE(nullptr, enabled);
  const QList<EqualizerSlider *> sliders = equalizer.findChildren<EqualizerSlider *>();
  ASSERT_EQ(Equalizer::kBands + 1, sliders.count());
  for (const EqualizerSlider *slider : sliders) {
    EXPECT_FALSE(slider->isEnabled());
    EXPECT_EQ(u"Enable equalizer first"_s, slider->toolTip());
  }
  enabled->setChecked(true);
  for (const EqualizerSlider *slider : sliders) {
    EXPECT_TRUE(slider->isEnabled());
    EXPECT_TRUE(slider->toolTip().isEmpty());
  }
  enabled->setChecked(false);
  for (const EqualizerSlider *slider : sliders) {
    EXPECT_EQ(u"Enable equalizer first"_s, slider->toolTip());
  }

}

TEST_F(EqualizerTest, EditablePreampUsesLinearMultiplierAndRetainsMute) {

  EqualizerSlider preamp(u"Pre-amp"_s, nullptr, true);
  preamp.set_gain_db(-6.0);
  EXPECT_EQ(-50, preamp.value());
  EXPECT_NEAR(-6.0206, preamp.gain_db(), 0.001);
  preamp.set_gain_db(6.02);
  EXPECT_EQ(100, preamp.value());
  preamp.set_gain_db(-41.0);
  EXPECT_EQ(-100, preamp.value());
  EXPECT_TRUE(std::isinf(preamp.gain_db()));
  EXPECT_LT(preamp.gain_db(), 0.0);
  preamp.set_gain_db(-40.0);
  EXPECT_EQ(-99, preamp.value());
  preamp.set_gain_db(0.0);
  EXPECT_EQ(0, preamp.value());

}

TEST_F(EqualizerTest, ImportMergesPresetsAndAppliesStateWhichPersists) {

  EqualizerJson::State exported;
  {
    Equalizer equalizer;
    exported = equalizer.ExportState();
    EXPECT_TRUE(exported.presets.contains(u"Rock"_s));
    EXPECT_EQ(u"Custom"_s, exported.selected_preset);
  }

  EqualizerJson::State imported;
  imported.presets.insert(u"Rock"_s, Equalizer::Params(1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
  imported.presets.insert(u"Imported"_s, Equalizer::Params(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, -20));
  imported.selected_preset = u"Imported"_s;
  imported.current = Equalizer::Params(-10, -20, -30, -40, -50, -60, -70, -80, -90, -100, 30);
  imported.enabled = true;
  imported.stereo_balancer_enabled = true;
  imported.stereo_balance = 40;
  {
    Equalizer equalizer;
    QSignalSpy parameters(&equalizer, &Equalizer::EqualizerParametersChanged);
    QSignalSpy enabled(&equalizer, &Equalizer::EqualizerEnabledChanged);
    equalizer.ImportState(imported);
    ASSERT_EQ(1, parameters.count());
    EXPECT_EQ(QList<int>({-10, -20, -30, -40, -50, -60, -70, -80, -90, -100}), parameters.first().at(1).value<QList<int>>());
    EXPECT_EQ(30, parameters.first().at(0).toInt());
    EXPECT_EQ(1, enabled.count());
    EXPECT_EQ(u"Imported"_s, equalizer.findChild<QComboBox *>(u"preset"_s)->currentData().toString());
  }

  Equalizer restored;
  const EqualizerJson::State state = restored.ExportState();
  EXPECT_EQ(imported.presets.value(u"Rock"_s), state.presets.value(u"Rock"_s));
  EXPECT_EQ(imported.presets.value(u"Imported"_s), state.presets.value(u"Imported"_s));
  EXPECT_EQ(exported.presets.value(u"Techno"_s), state.presets.value(u"Techno"_s));
  EXPECT_EQ(exported.presets.count() + 1, state.presets.count());
  EXPECT_EQ(u"Imported"_s, state.selected_preset);
  EXPECT_TRUE(state.enabled);
  EXPECT_TRUE(state.stereo_balancer_enabled);
  EXPECT_EQ(40, state.stereo_balance);
#ifndef Q_OS_ANDROID
  // The desktop equalizer restores the selected preset's saved curve; Android also retains the unsaved current curve.
  EXPECT_EQ(imported.presets.value(u"Imported"_s), state.current);
#endif

}
