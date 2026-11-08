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
#include <QStringList>
#include <QSlider>
#include <QComboBox>
#include <QVariant>

#include "core/settings.h"
#include "core/song.h"
#include "transcoder/transcoder.h"
#include "transcoder/transcodedialog.h"
#include "transcoder/transcoderoptionsinterface.h"
#include "transcoder/transcoderoptionsasf.h"
#include "transcoder/transcoderoptionsaac.h"

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

  // Creates the element with the saved options applied, like when the transcoder uses it.
  static GstElement *CreateElement(const QString &factory_name) {
    gst_init(nullptr, nullptr);
    Transcoder transcoder;
    GstElement *element = transcoder.CreateElement(factory_name);
    if (element) gst_object_ref_sink(element);
    return element;
  }
};

namespace {

// Saves the bitrate like the options dialog.
void SaveBitrate(TranscoderOptionsInterface *options, const int kbps) {

  options->Load();
  QSlider *bitrate_slider = options->findChild<QSlider*>(u"bitrate_slider"_s);
  ASSERT_NE(nullptr, bitrate_slider);
  bitrate_slider->setValue(kbps);
  options->Save();

}

// The bitrate property has different types in different encoders and GStreamer versions, like gint, guint or gint64, so it's read as its own type and converted.
qint64 EncoderBitrate(GstElement *encoder) {

  GParamSpec *property = g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "bitrate");
  if (!property) return -1;

  GValue value = G_VALUE_INIT;
  g_value_init(&value, property->value_type);
  g_object_get_property(G_OBJECT(encoder), "bitrate", &value);

  GValue int64_value = G_VALUE_INIT;
  g_value_init(&int64_value, G_TYPE_INT64);
  const qint64 bitrate = g_value_transform(&value, &int64_value) ? g_value_get_int64(&int64_value) : -1;

  g_value_unset(&int64_value);
  g_value_unset(&value);

  return bitrate;

}

QString ElementName(GstElement *element) {

  GstElementFactory *factory = gst_element_get_factory(element);
  return factory ? QString::fromUtf8(GST_OBJECT_NAME(factory)) : QString();

}

// Regression test: the WMA options were saved for the GStreamer 0.10 encoder "ffenc_wmav2", so the bitrate was never set on the encoder.
TEST(TranscoderOptionsTest, WMABitrateIsSetOnEncoder) {

  Settings().remove(u"Transcoder"_s);

  TranscoderOptionsASF options(nullptr);
  SaveBitrate(&options, 96);
  if (::testing::Test::HasFatalFailure()) return;

  GstElement *encoder = TranscoderTest::CreateEncoder(Song::FileType::ASF);
  if (!encoder) {
    GTEST_SKIP() << "No WMA encoder is installed.";
  }

  // The options are for WMA version 2, avenc_wmav1 has the same rank, but the preset asks for version 2.
  EXPECT_EQ(u"avenc_wmav2"_s, ElementName(encoder));
  EXPECT_EQ(96000, EncoderBitrate(encoder));
  gst_object_unref(encoder);

}

// Regression test: the AAC options were only applied to faac, but other AAC encoders, like fdkaacenc, are used when they have a higher rank.
TEST(TranscoderOptionsTest, AACBitrateIsSetOnEncoder) {

  Settings().remove(u"Transcoder"_s);

  TranscoderOptionsAAC options(nullptr);
  SaveBitrate(&options, 96);
  if (::testing::Test::HasFatalFailure()) return;

  GstElement *encoder = TranscoderTest::CreateEncoder(Song::FileType::MP4);
  if (!encoder) {
    GTEST_SKIP() << "No AAC encoder is installed.";
  }

  EXPECT_EQ(96000, EncoderBitrate(encoder)) << "Encoder: " << ElementName(encoder).toStdString();
  gst_object_unref(encoder);

}

// The WMA bitrate saved for the GStreamer 0.10 encoder name is kept when upgrading.
TEST(TranscoderOptionsTest, LegacyWMABitrateIsMigrated) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/ffenc_wmav2/bitrate"_s, 64000);
  }

  TranscoderOptionsASF options(nullptr);
  options.Load();
  QSlider *bitrate_slider = options.findChild<QSlider*>(u"bitrate_slider"_s);
  ASSERT_NE(nullptr, bitrate_slider);
  EXPECT_EQ(64, bitrate_slider->value());

  {
    Settings s;
    s.beginGroup(u"Transcoder"_s);
    EXPECT_FALSE(s.childGroups().contains(u"ffenc_wmav2"_s));
    EXPECT_EQ(64000, s.value(u"avenc_wmav2/bitrate"_s).toInt());
    s.endGroup();
  }

  GstElement *encoder = TranscoderTest::CreateEncoder(Song::FileType::ASF);
  if (!encoder) {
    GTEST_SKIP() << "No WMA encoder is installed.";
  }
  EXPECT_EQ(64000, EncoderBitrate(encoder));
  gst_object_unref(encoder);

}

// A bitrate already saved for the new encoder name is newer, it's kept when migrating.
TEST(TranscoderOptionsTest, LegacyWMABitrateDoesNotReplaceNewerBitrate) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/ffenc_wmav2/bitrate"_s, 64000);
    s.setValue(u"Transcoder/avenc_wmav2/bitrate"_s, 96000);
  }

  Transcoder::MigrateSettings();

  Settings s;
  s.beginGroup(u"Transcoder"_s);
  EXPECT_FALSE(s.childGroups().contains(u"ffenc_wmav2"_s));
  EXPECT_EQ(96000, s.value(u"avenc_wmav2/bitrate"_s).toInt());
  s.endGroup();

}

// The AAC bitrate is applied to all AAC encoders, which encoder is used depends on the platform and the installed plugins, like mfaacenc on Windows and atenc on macOS.
TEST(TranscoderOptionsTest, AACBitrateIsSetOnAllAACEncoders) {

  Settings().remove(u"Transcoder"_s);

  TranscoderOptionsAAC options(nullptr);
  SaveBitrate(&options, 96);
  if (::testing::Test::HasFatalFailure()) return;

  gst_init(nullptr, nullptr);
  GstCaps *aac_caps = gst_caps_from_string(Transcoder::PresetForFileType(Song::FileType::MP4).codec_mimetype_.toUtf8().constData());
  GList *factories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, GST_RANK_NONE);
  QStringList encoder_names;
  for (GList *f = factories; f; f = g_list_next(f)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(f->data);
    if (gst_element_factory_can_src_any_caps(factory, aac_caps)) {
      encoder_names << QString::fromUtf8(GST_OBJECT_NAME(factory));
    }
  }
  gst_plugin_feature_list_free(factories);
  gst_caps_unref(aac_caps);

  if (encoder_names.isEmpty()) {
    GTEST_SKIP() << "No AAC encoder is installed.";
  }

  for (const QString &encoder_name : std::as_const(encoder_names)) {
    GstElement *encoder = TranscoderTest::CreateElement(encoder_name);
    ASSERT_NE(nullptr, encoder) << encoder_name.toStdString();
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "bitrate")) {
      EXPECT_EQ(96000, EncoderBitrate(encoder)) << "Encoder: " << encoder_name.toStdString();
    }
    gst_object_unref(encoder);
  }

}

// Regression test: the WMA caps have the version now, so WMA saved as the last output format with the old caps was not selected, the first format was.
TEST(TranscoderOptionsTest, LegacyWMAOutputFormatIsSelected) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/last_output_format"_s, u"audio/x-wma"_s);
  }

  TranscodeDialog dialog(nullptr, nullptr);
  QComboBox *combobox_format = dialog.findChild<QComboBox*>(u"format"_s);
  ASSERT_NE(nullptr, combobox_format);
  ASSERT_NE(-1, combobox_format->currentIndex());
  EXPECT_EQ(Song::FileType::ASF, combobox_format->currentData().value<TranscoderPreset>().filetype_);
  EXPECT_EQ(Transcoder::PresetForFileType(Song::FileType::ASF).codec_mimetype_, Settings().value(u"Transcoder/last_output_format"_s).toString());

}

}  // namespace
