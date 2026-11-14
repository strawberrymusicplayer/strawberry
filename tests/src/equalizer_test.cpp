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

#include <QCoreApplication>
#include <QApplication>
#include <QTranslator>
#include <QInputDialog>
#include <QComboBox>
#include <QTimer>
#include <QString>
#include <QTest>

#include "core/settings.h"
#include "equalizer/equalizer.h"
#include "equalizer/equalizerslider.h"

using namespace Qt::Literals::StringLiterals;

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
// They are also kept after quitting again without changing them, the selected preset saved its values as the current values when starting.
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

// Selecting another preset still uses the values of that preset after a restart.
TEST_F(EqualizerTest, SelectedPresetValuesAreUsedAfterRestart) {

  {
    Equalizer equalizer;
    SelectPreset(&equalizer, u"Zero"_s);
    SelectPreset(&equalizer, u"Rock"_s);
  }

  Equalizer equalizer;
  EXPECT_EQ(u"Rock"_s, PresetComboBox(&equalizer)->currentText());
  EXPECT_EQ(PresetParams(&equalizer, u"Rock"_s).gain[0], equalizer.current_params().gain[0]);

}

}  // namespace
