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

#include "config.h"

#include <utility>

#include <gst/gst.h>

#include "gtest_include.h"

#include <QtGlobal>
#include <QString>
#include <QList>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

#include "core/settings.h"
#include "core/song.h"
#include "transcoder/transcoder.h"
#include "transcoder/transcoderoptionsopus.h"

using namespace Qt::Literals::StringLiterals;

class TranscoderTest {
 public:
  // Creates the encoder the transcoder uses for the format, with the saved options applied.
  static GstElement *CreateEncoder(const Song::FileType filetype) {
    gst_init(nullptr, nullptr);
    Transcoder transcoder;
    GstElement *encoder = transcoder.CreateElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, Transcoder::PresetForFileType(filetype).codec_mimetype_);
    if (encoder) gst_object_ref_sink(encoder);
    return encoder;
  }
};

namespace {

// Enums are read as their value, booleans as 0 or 1.
int IntProperty(GstElement *element, const char *name) {

  GParamSpec *property = g_object_class_find_property(G_OBJECT_GET_CLASS(element), name);
  if (!property) return -1;

  GValue value = G_VALUE_INIT;
  g_value_init(&value, property->value_type);
  g_object_get_property(G_OBJECT(element), name, &value);

  int result = -1;
  if (G_VALUE_HOLDS_ENUM(&value)) {
    result = g_value_get_enum(&value);
  }
  else if (G_VALUE_HOLDS_BOOLEAN(&value)) {
    result = g_value_get_boolean(&value) ? 1 : 0;
  }
  else if (G_VALUE_HOLDS_INT(&value)) {
    result = g_value_get_int(&value);
  }

  g_value_unset(&value);

  return result;

}

class TranscoderOptionsOpusTest : public ::testing::Test {
 protected:
  void SetUp() override {

    Settings().remove(u"Transcoder"_s);

    gst_init(nullptr, nullptr);
    GstElementFactory *factory = gst_element_factory_find("opusenc");
    if (!factory) {
      GTEST_SKIP() << "opusenc is not installed.";
    }
    gst_object_unref(factory);

  }

  void TearDown() override {
    Settings().remove(u"Transcoder"_s);
  }

  // Creates the Opus encoder with the saved options applied, like when transcoding.
  static GstElement *CreateEncoder() {
    return TranscoderTest::CreateEncoder(Song::FileType::OggOpus);
  }

  template<typename T>
  static T *Widget(TranscoderOptionsOpus *options, const QString &name) {
    return options->findChild<T*>(name);
  }

  // Selects the item with the encoder value, like the user selecting it.
  static void SelectData(QComboBox *combobox, const int value) {
    ASSERT_NE(nullptr, combobox);
    const int index = combobox->findData(value);
    ASSERT_NE(-1, index) << combobox->objectName().toStdString() << " has no item for " << value;
    combobox->setCurrentIndex(index);
  }
};

// Regression test for issue 2378: only the bitrate could be set for Opus.
TEST_F(TranscoderOptionsOpusTest, SettingsAreSetOnEncoder) {

  TranscoderOptionsOpus options(nullptr);
  options.Load();

  QSlider *bitrate_slider = Widget<QSlider>(&options, u"bitrate_slider"_s);
  QSpinBox *complexity = Widget<QSpinBox>(&options, u"complexity"_s);
  QCheckBox *dtx = Widget<QCheckBox>(&options, u"dtx"_s);
  ASSERT_NE(nullptr, bitrate_slider);
  ASSERT_NE(nullptr, complexity);
  ASSERT_NE(nullptr, dtx);

  bitrate_slider->setValue(96);
  SelectData(Widget<QComboBox>(&options, u"bitrate_type"_s), 0);
  SelectData(Widget<QComboBox>(&options, u"audio_type"_s), 2048);
  SelectData(Widget<QComboBox>(&options, u"bandwidth"_s), -1000);
  SelectData(Widget<QComboBox>(&options, u"frame_size"_s), 40);
  if (HasFatalFailure()) return;
  complexity->setValue(5);
  dtx->setChecked(true);
  options.Save();

  GstElement *encoder = CreateEncoder();
  ASSERT_NE(nullptr, encoder);

  EXPECT_EQ(96000, IntProperty(encoder, "bitrate"));
  EXPECT_EQ(0, IntProperty(encoder, "bitrate-type"));
  EXPECT_EQ(5, IntProperty(encoder, "complexity"));
  EXPECT_EQ(2048, IntProperty(encoder, "audio-type"));
  EXPECT_EQ(-1000, IntProperty(encoder, "bandwidth"));
  EXPECT_EQ(40, IntProperty(encoder, "frame-size"));
  EXPECT_EQ(1, IntProperty(encoder, "dtx"));
  gst_object_unref(encoder);

}

// The defaults are the opusenc defaults, so saving the options without changes encodes like before the settings were added.
TEST_F(TranscoderOptionsOpusTest, DefaultsAreEncoderDefaults) {

  TranscoderOptionsOpus options(nullptr);
  options.Load();
  options.Save();

  GstElement *encoder = CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  GstElement *default_encoder = gst_element_factory_make("opusenc", nullptr);
  ASSERT_NE(nullptr, default_encoder);
  gst_object_ref_sink(default_encoder);

  for (const char *property : {"bitrate-type", "complexity", "audio-type", "bandwidth", "frame-size", "dtx"}) {
    EXPECT_EQ(IntProperty(default_encoder, property), IntProperty(encoder, property)) << property;
  }

  gst_object_unref(default_encoder);
  gst_object_unref(encoder);

}

// Every item is a value the encoder accepts, otherwise GStreamer ignores it with a warning.
TEST_F(TranscoderOptionsOpusTest, ItemsAreEncoderValues) {

  GstElement *encoder = gst_element_factory_make("opusenc", nullptr);
  ASSERT_NE(nullptr, encoder);
  gst_object_ref_sink(encoder);

  TranscoderOptionsOpus options(nullptr);
  const QList<std::pair<QString, const char*>> comboboxes = { { u"bitrate_type"_s, "bitrate-type" }, { u"audio_type"_s, "audio-type" }, { u"bandwidth"_s, "bandwidth" }, { u"frame_size"_s, "frame-size" } };
  for (const std::pair<QString, const char*> &combobox_property : comboboxes) {
    QComboBox *combobox = Widget<QComboBox>(&options, combobox_property.first);
    ASSERT_NE(nullptr, combobox) << combobox_property.first.toStdString();
    GParamSpec *property = g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), combobox_property.second);
    ASSERT_NE(nullptr, property) << combobox_property.second;
    ASSERT_TRUE(G_IS_PARAM_SPEC_ENUM(property)) << combobox_property.second;
    GEnumClass *enum_class = G_PARAM_SPEC_ENUM(property)->enum_class;
    EXPECT_EQ(static_cast<int>(enum_class->n_values), combobox->count()) << combobox_property.second;
    for (int i = 0; i < combobox->count(); ++i) {
      const int value = combobox->itemData(i).toInt();
      EXPECT_NE(nullptr, g_enum_get_value(enum_class, value)) << combobox_property.second << " " << value;
    }
  }

  gst_object_unref(encoder);

}

// Saved settings are shown when the options are opened again.
TEST_F(TranscoderOptionsOpusTest, SavedSettingsAreLoaded) {

  {
    Settings s;
    s.beginGroup(u"Transcoder/opusenc"_s);
    s.setValue(u"bitrate"_s, 510000);
    s.setValue(u"bitrate-type"_s, 1);
    s.setValue(u"complexity"_s, 0);
    s.setValue(u"audio-type"_s, 2051);
    s.setValue(u"bandwidth"_s, 1101);
    s.setValue(u"frame-size"_s, 2);
    s.setValue(u"dtx"_s, true);
    s.endGroup();
  }

  TranscoderOptionsOpus options(nullptr);
  options.Load();

  // The spinbox was limited to 320 kbps, so a higher bitrate was lowered when the spinbox was synced.
  EXPECT_EQ(510, Widget<QSlider>(&options, u"bitrate_slider"_s)->value());
  EXPECT_EQ(510, Widget<QSpinBox>(&options, u"bitrate_spinbox"_s)->value());
  EXPECT_EQ(1, Widget<QComboBox>(&options, u"bitrate_type"_s)->currentData().toInt());
  EXPECT_EQ(0, Widget<QSpinBox>(&options, u"complexity"_s)->value());
  EXPECT_EQ(2051, Widget<QComboBox>(&options, u"audio_type"_s)->currentData().toInt());
  EXPECT_EQ(1101, Widget<QComboBox>(&options, u"bandwidth"_s)->currentData().toInt());
  EXPECT_EQ(2, Widget<QComboBox>(&options, u"frame_size"_s)->currentData().toInt());
  EXPECT_TRUE(Widget<QCheckBox>(&options, u"dtx"_s)->isChecked());

}

// A saved value with no item selects the default instead of an empty item, so saving again doesn't write an invalid value.
TEST_F(TranscoderOptionsOpusTest, UnknownSavedValueSelectsDefault) {

  {
    Settings s;
    s.beginGroup(u"Transcoder/opusenc"_s);
    s.setValue(u"bitrate-type"_s, 7);
    s.setValue(u"audio-type"_s, 1);
    s.setValue(u"bandwidth"_s, 1);
    s.setValue(u"frame-size"_s, 30);
    s.endGroup();
  }

  TranscoderOptionsOpus options(nullptr);
  options.Load();

  EXPECT_EQ(2, Widget<QComboBox>(&options, u"bitrate_type"_s)->currentData().toInt());
  EXPECT_EQ(2049, Widget<QComboBox>(&options, u"audio_type"_s)->currentData().toInt());
  EXPECT_EQ(1105, Widget<QComboBox>(&options, u"bandwidth"_s)->currentData().toInt());
  EXPECT_EQ(20, Widget<QComboBox>(&options, u"frame_size"_s)->currentData().toInt());

}

}  // namespace
