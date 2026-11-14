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

}  // namespace
