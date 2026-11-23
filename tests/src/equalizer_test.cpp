/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QCoreApplication>
#include <QApplication>
#include <QTranslator>
#include <QInputDialog>
#include <QComboBox>
#include <QSlider>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QSignalSpy>
#include <QTest>

#include "core/settings.h"
#include "equalizer/equalizer.h"
#include "equalizer/equalizerslider.h"

using namespace Qt::Literals::StringLiterals;

class EqualizerUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings settings;
    settings.beginGroup(u"Equalizer"_s);
    settings.remove(QString());
  }
};

TEST_F(EqualizerUiTest, SlidersKeepEngineGainRangeAndEmitLiveChanges) {

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

TEST_F(EqualizerUiTest, SelectingPresetEmitsOneCompleteParameterUpdate) {

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

TEST_F(EqualizerUiTest, PreampLabelMatchesLinearEngineMultiplier) {

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

TEST_F(EqualizerUiTest, EnableAndBalanceSettingsPersistWithoutClosing) {

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

TEST_F(EqualizerUiTest, EditableDecibelsRoundToExistingEngineSteps) {

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

TEST_F(EqualizerUiTest, DisabledGainControlsExplainHowToEnableThem) {

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

TEST_F(EqualizerUiTest, EditablePreampUsesLinearMultiplierAndRetainsMute) {

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

TEST_F(EqualizerUiTest, ImportMergesPresetsAndAppliesStateWhichPersists) {

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
  EXPECT_EQ(imported.current, state.current);

}
// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

// Translates the name of the built-in "Classical" preset, like the German translation does.
class EqualizerTranslator : public QTranslator {
 public:
  QString translate(const char *context, const char *source_text, const char *disambiguation, const int n) const override {

    Q_UNUSED(disambiguation)
    Q_UNUSED(n)

    if (qstrcmp(context, "Equalizer") == 0 && qstrcmp(source_text, "Classical") == 0) return u"Klassik"_s;
    return QString();

  }

  bool isEmpty() const override { return false; }
};

}  // namespace

class EqualizerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    Settings().remove(u"Equalizer"_s);
    QCoreApplication::installTranslator(&translator_);

  }

  void TearDown() override {

    QCoreApplication::removeTranslator(&translator_);
    Settings().remove(u"Equalizer"_s);

  }

  static QComboBox *PresetComboBox(Equalizer *equalizer) { return equalizer->findChild<QComboBox*>(u"preset"_s); }

  static void SelectPreset(Equalizer *equalizer, const QString &name_displayed) {

    QComboBox *combobox = PresetComboBox(equalizer);
    combobox->setCurrentIndex(combobox->findText(name_displayed));

  }

  static void SetFirstGain(Equalizer *equalizer, const int value) { equalizer->gain_[0]->set_value(value); }

  // Saves the current preset with the name suggested in the dialog.
  static void SavePresetWithSuggestedName(Equalizer *equalizer) {

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &timer, [&timer]() {
      if (QInputDialog *dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
        timer.stop();
        dialog->accept();
      }
    });
    timer.start(10);
    equalizer->SavePreset();

  }

  static QStringList PresetNames(Equalizer *equalizer) { return equalizer->presets_.keys(); }

  // A precise timer, the remaining time of a coarse timer with a long interval is not exact.
  static void SetSaveDelay(Equalizer *equalizer, const int msec) {
    equalizer->save_timer_->setTimerType(Qt::PreciseTimer);
    equalizer->save_timer_->setInterval(msec);
  }
  static bool SaveTimerActive(Equalizer *equalizer) { return equalizer->save_timer_->isActive(); }
  static int SaveTimerRemainingTime(Equalizer *equalizer) { return equalizer->save_timer_->remainingTime(); }

  static void SetStereoBalance(Equalizer *equalizer, const int value) { equalizer->findChild<QSlider*>(u"stereo_balance_slider"_s)->setValue(value); }

  static QVariant SavedValue(const QString &key) {
    Settings s;
    s.beginGroup(u"Equalizer"_s);
    const QVariant value = s.value(key);
    s.endGroup();
    return value;
  }

  // Processes events until the condition is true, returns false on timeout.
  template <typename Condition>
  static bool WaitFor(Condition condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > 5000) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
  }

  static Equalizer::Params PresetParams(Equalizer *equalizer, const QString &name) { return equalizer->presets_.value(name); }

  // Accepts the dialog asking for the name of the preset with the suggested name, and counts how many times it was shown.
  void AcceptSaveDialogs() {

    QObject::connect(&dialog_timer_, &QTimer::timeout, &dialog_timer_, [this]() {
      if (QInputDialog *dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
        ++save_dialog_count_;
        dialog->accept();
      }
    });
    dialog_timer_.start(10);

  }

  QTimer dialog_timer_;
  int save_dialog_count_ = 0;

  EqualizerTranslator translator_;
};

namespace {

// Regression test: the preset was saved under the translated name, which is not shown since it has the same text as the built-in preset, so the built-in preset kept the old values.
TEST_F(EqualizerTest, SavingTranslatedPresetSavesBuiltInPreset) {

  {
    Equalizer equalizer;
    const int preset_count = PresetComboBox(&equalizer)->count();
    SelectPreset(&equalizer, u"Klassik"_s);
    ASSERT_EQ(u"Klassik"_s, PresetComboBox(&equalizer)->currentText());
    SetFirstGain(&equalizer, 77);

    SavePresetWithSuggestedName(&equalizer);

    EXPECT_FALSE(PresetNames(&equalizer).contains(u"Klassik"_s));
    EXPECT_EQ(77, equalizer.current_params().gain[0]);
    EXPECT_EQ(preset_count, PresetComboBox(&equalizer)->count());
    EXPECT_EQ(u"Klassik"_s, PresetComboBox(&equalizer)->currentText());
  }

  // The saved preset has the new values when it's loaded again.
  Equalizer equalizer;
  EXPECT_FALSE(PresetNames(&equalizer).contains(u"Klassik"_s));
  SelectPreset(&equalizer, u"Zero"_s);
  SelectPreset(&equalizer, u"Klassik"_s);
  EXPECT_EQ(77, equalizer.current_params().gain[0]);

}

// A new name still adds a new preset.
TEST_F(EqualizerTest, SavingPresetWithNewNameAddsPreset) {

  Equalizer equalizer;
  const int preset_count = PresetComboBox(&equalizer)->count();
  SelectPreset(&equalizer, u"Klassik"_s);
  SetFirstGain(&equalizer, 66);

  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, &timer, [&timer]() {
    if (QInputDialog *dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
      timer.stop();
      dialog->setTextValue(u"Mine"_s);
      dialog->accept();
    }
  });
  timer.start(10);
  QMetaObject::invokeMethod(&equalizer, "SavePreset", Qt::DirectConnection);

  EXPECT_TRUE(PresetNames(&equalizer).contains(u"Mine"_s));
  EXPECT_EQ(preset_count + 1, PresetComboBox(&equalizer)->count());
  EXPECT_EQ(u"Mine"_s, PresetComboBox(&equalizer)->currentText());

}

// Regression test: Escape closed the dialog without asking to save the changed preset, only closing it with the title bar asked.
TEST_F(EqualizerTest, EscapeAsksToSaveChangedPreset) {

  Equalizer equalizer;
  equalizer.show();
  SelectPreset(&equalizer, u"Rock"_s);
  SetFirstGain(&equalizer, 77);
  AcceptSaveDialogs();

  QTest::keyClick(&equalizer, Qt::Key_Escape);

  EXPECT_EQ(1, save_dialog_count_);
  EXPECT_EQ(77, PresetParams(&equalizer, u"Rock"_s).gain[0]);
  EXPECT_FALSE(equalizer.isVisible());

}

TEST_F(EqualizerTest, CloseAsksToSaveChangedPreset) {

  Equalizer equalizer;
  equalizer.show();
  SelectPreset(&equalizer, u"Rock"_s);
  SetFirstGain(&equalizer, 77);
  AcceptSaveDialogs();

  equalizer.close();

  EXPECT_EQ(1, save_dialog_count_);
  EXPECT_EQ(77, PresetParams(&equalizer, u"Rock"_s).gain[0]);
  EXPECT_FALSE(equalizer.isVisible());

}

TEST_F(EqualizerTest, EscapeWithoutChangesDoesNotAsk) {

  Equalizer equalizer;
  equalizer.show();
  SelectPreset(&equalizer, u"Rock"_s);
  AcceptSaveDialogs();

  QTest::keyClick(&equalizer, Qt::Key_Escape);

  EXPECT_EQ(0, save_dialog_count_);
  EXPECT_FALSE(equalizer.isVisible());

}

// Regression test: changes which were not saved to the preset were lost when quitting with the dialog open, since quitting doesn't close the dialog.
// They are also kept after quitting again without changing them.
TEST_F(EqualizerTest, UnsavedChangesAreKeptAfterQuitting) {

  int preset_gain = 0;
  {
    Equalizer equalizer;
    equalizer.show();
    SelectPreset(&equalizer, u"Rock"_s);
    preset_gain = PresetParams(&equalizer, u"Rock"_s).gain[0];
    ASSERT_NE(55, preset_gain);
    SetFirstGain(&equalizer, 55);
  }

  for (int restart = 1; restart <= 2; ++restart) {
    SCOPED_TRACE(restart);
    Equalizer equalizer;
    EXPECT_EQ(u"Rock"_s, PresetComboBox(&equalizer)->currentText());
    EXPECT_EQ(55, equalizer.current_params().gain[0]);
    // The preset is not changed without asking.
    EXPECT_EQ(preset_gain, PresetParams(&equalizer, u"Rock"_s).gain[0]);
  }

}

// Regression test: loading the settings when starting saved them again, also the values of the preset as the current values.
TEST_F(EqualizerTest, StartingDoesNotSaveSettings) {

  {
    Equalizer equalizer;
  }

  Settings s;
  s.beginGroup(u"Equalizer"_s);
  EXPECT_EQ(QStringList(), s.allKeys());
  s.endGroup();

}

// Regression test: without a selected preset in the settings, the preset which is shown was not loaded, so the changes to it were lost without asking to save them when selecting another preset.
TEST_F(EqualizerTest, ChangedDefaultPresetAsksToSaveWhenSelectingAnotherPreset) {

  Equalizer equalizer;
  equalizer.show();
  ASSERT_EQ(u"Custom"_s, PresetComboBox(&equalizer)->currentText());
  SetFirstGain(&equalizer, 77);
  AcceptSaveDialogs();

  SelectPreset(&equalizer, u"Rock"_s);

  EXPECT_EQ(1, save_dialog_count_);
  EXPECT_EQ(77, PresetParams(&equalizer, u"Custom"_s).gain[0]);

}

// Selecting another preset still uses the values of that preset after a restart, also when the current values of the previous preset were saved, and the player is notified about the values.
TEST_F(EqualizerTest, SelectedPresetValuesAreUsedAfterRestart) {

  {
    Equalizer equalizer;
    SelectPreset(&equalizer, u"Zero"_s);
    // Changing the values saves them as the current values, they are changed back so selecting another preset doesn't ask to save the preset.
    SetFirstGain(&equalizer, 55);
    SetFirstGain(&equalizer, 0);
    QSignalSpy parameters_changed_spy(&equalizer, &Equalizer::EqualizerParametersChanged);
    SelectPreset(&equalizer, u"Rock"_s);
    ASSERT_EQ(1, parameters_changed_spy.count());
    EXPECT_EQ(PresetParams(&equalizer, u"Rock"_s).gain[0], parameters_changed_spy.first().at(1).value<QList<int>>().value(0));
  }

  Equalizer equalizer;
  EXPECT_EQ(u"Rock"_s, PresetComboBox(&equalizer)->currentText());
  EXPECT_EQ(PresetParams(&equalizer, u"Rock"_s).gain[0], equalizer.current_params().gain[0]);

}

// Regression test: moving a slider saved the settings for every step, which wrote the settings file for every step.
TEST_F(EqualizerTest, SliderStepsAreSavedAfterDelay) {

  Equalizer equalizer;
  QElapsedTimer timer;
  timer.start();
  for (int value = 1; value <= 10; ++value) {
    SetFirstGain(&equalizer, value);
    SetStereoBalance(&equalizer, value);
  }
  // Not saved for every step.
  if (timer.elapsed() < 400) {
    EXPECT_FALSE(SavedValue(u"current_params"_s).isValid());
    EXPECT_FALSE(SavedValue(u"stereo_balance"_s).isValid());
  }

  // Saved after the delay, with the last values.
  ASSERT_TRUE(WaitFor([]() { return SavedValue(u"current_params"_s).isValid() && SavedValue(u"stereo_balance"_s).isValid(); }));
  EXPECT_EQ(10, SavedValue(u"current_params"_s).value<Equalizer::Params>().gain[0]);
  EXPECT_EQ(10, SavedValue(u"stereo_balance"_s).toInt());

}

// The steps which were not saved yet are saved when the equalizer is deleted, like when quitting.
TEST_F(EqualizerTest, PendingSliderStepsAreSavedWhenDeleted) {

  {
    Equalizer equalizer;
    SetFirstGain(&equalizer, 44);
    SetStereoBalance(&equalizer, 33);
  }

  EXPECT_EQ(44, SavedValue(u"current_params"_s).value<Equalizer::Params>().gain[0]);
  EXPECT_EQ(33, SavedValue(u"stereo_balance"_s).toInt());

}

// The steps which were not saved yet are saved when quitting, also when the equalizer is not deleted then.
TEST_F(EqualizerTest, PendingSliderStepsAreSavedWhenQuitting) {

  Equalizer equalizer;
  SetFirstGain(&equalizer, 22);
  ASSERT_FALSE(SavedValue(u"current_params"_s).isValid());

  ASSERT_TRUE(QMetaObject::invokeMethod(QCoreApplication::instance(), "aboutToQuit", Qt::DirectConnection));

  EXPECT_EQ(22, SavedValue(u"current_params"_s).value<Equalizer::Params>().gain[0]);

}

// The steps which were not saved yet are saved when the dialog is closed, which doesn't delete it, also when saving them to the preset was cancelled.
TEST_F(EqualizerTest, PendingSliderStepsAreSavedWhenClosed) {

  Equalizer equalizer;
  equalizer.show();
  SelectPreset(&equalizer, u"Rock"_s);
  SetFirstGain(&equalizer, 88);
  ASSERT_FALSE(SavedValue(u"current_params"_s).isValid());

  // Cancel saving the changes to the preset.
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, &timer, [&timer]() {
    if (QInputDialog *dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
      timer.stop();
      dialog->reject();
    }
  });
  timer.start(10);
  equalizer.close();
  ASSERT_FALSE(equalizer.isVisible());

  EXPECT_EQ(88, SavedValue(u"current_params"_s).value<Equalizer::Params>().gain[0]);

}

// The delay is counted from the last step, so the changes are not saved while a slider is still moving.
TEST_F(EqualizerTest, SliderStepRestartsSaveDelay) {

  Equalizer equalizer;
  // A delay which doesn't elapse during the test, so only the restart is checked, events are not processed either.
  SetSaveDelay(&equalizer, 3600 * 1000);

  SetFirstGain(&equalizer, 1);
  ASSERT_TRUE(SaveTimerActive(&equalizer));
  QThread::msleep(50);
  const int remaining_time = SaveTimerRemainingTime(&equalizer);
  ASSERT_LT(remaining_time, 3600 * 1000);

  SetFirstGain(&equalizer, 2);
  EXPECT_GT(SaveTimerRemainingTime(&equalizer), remaining_time);
  EXPECT_FALSE(SavedValue(u"current_params"_s).isValid());

}

}  // namespace
