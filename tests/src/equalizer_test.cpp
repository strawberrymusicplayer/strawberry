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

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>

#include "core/settings.h"
#include "equalizer/equalizer.h"
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
