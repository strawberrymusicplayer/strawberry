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
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QVariant>

#include "core/settings.h"
#include "core/settingsmigration.h"
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

// The AAC options look up the encoder in the GStreamer registry when they're created, like in Strawberry, where GStreamer is initialized at startup.
class GstEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    gst_init(nullptr, nullptr);
  }
};

[[maybe_unused]] ::testing::Environment *const kGstEnvironment = ::testing::AddGlobalTestEnvironment(new GstEnvironment);

// Changes the rank of an element, so the transcoder prefers it, the rank is restored when it goes out of scope.
class ScopedElementRank {
 public:
  explicit ScopedElementRank(const QString &name, const guint rank) : feature_(gst_registry_lookup_feature(gst_registry_get(), name.toUtf8().constData())), previous_rank_(0) {
    if (feature_) {
      previous_rank_ = gst_plugin_feature_get_rank(feature_);
      gst_plugin_feature_set_rank(feature_, rank);
    }
  }

  ~ScopedElementRank() {
    if (feature_) {
      gst_plugin_feature_set_rank(feature_, previous_rank_);
      gst_object_unref(feature_);
    }
  }

  bool found() const { return feature_ != nullptr; }

 private:
  Q_DISABLE_COPY_MOVE(ScopedElementRank)

  GstPluginFeature *feature_;
  guint previous_rank_;
};

// Returns the names of the installed AAC encoders.
QStringList AACEncoderNames() {

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

  return encoder_names;

}

// Returns an AAC encoder other than faac which the transcoder can prefer, the FFmpeg encoders always have the lowest rank.
QString OtherAACEncoderName() {

  const QStringList encoder_names = AACEncoderNames();
  for (const QString &encoder_name : encoder_names) {
    if (encoder_name != "faac"_L1 && !encoder_name.startsWith("avenc"_L1)) return encoder_name;
  }

  return QString();

}

// Saves the bitrate like the options dialog.
void SaveBitrate(TranscoderOptionsInterface *options, const int kbps) {

  options->Load();
  QSlider *bitrate_slider = options->findChild<QSlider*>(u"bitrate_slider"_s);
  ASSERT_NE(nullptr, bitrate_slider);
  bitrate_slider->setValue(kbps);
  options->Save();

}

// Integer properties have different types in different encoders and GStreamer versions, like gint, guint or gint64, so they're read as their own type and converted, enums are read as their value.
qint64 IntProperty(GstElement *element, const char *name) {

  GParamSpec *property = g_object_class_find_property(G_OBJECT_GET_CLASS(element), name);
  if (!property) return -1;

  GValue value = G_VALUE_INIT;
  g_value_init(&value, property->value_type);
  g_object_get_property(G_OBJECT(element), name, &value);

  qint64 result = -1;
  if (G_VALUE_HOLDS_ENUM(&value)) {
    result = g_value_get_enum(&value);
  }
  else {
    GValue int64_value = G_VALUE_INIT;
    g_value_init(&int64_value, G_TYPE_INT64);
    if (g_value_transform(&value, &int64_value)) result = g_value_get_int64(&int64_value);
    g_value_unset(&int64_value);
  }

  g_value_unset(&value);

  return result;

}

qint64 EncoderBitrate(GstElement *encoder) {
  return IntProperty(encoder, "bitrate");
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

// Selects the faac rate control like the options dialog, the first item is VBR and the second is ABR.
void SelectAACRateControl(TranscoderOptionsInterface *options, const int index) {

  QComboBox *rate_control = options->findChild<QComboBox*>(u"rate_control"_s);
  ASSERT_NE(nullptr, rate_control);
  rate_control->setCurrentIndex(index);

}

// Regression test: faac uses VBR by default, which ignores the bitrate, and the rate control was never set, so the bitrate had no effect.
TEST(TranscoderOptionsTest, AACBitrateIsUsedByFaacWithABR) {

  Settings().remove(u"Transcoder"_s);

  // ABR is the default, so the bitrate is used without changing the rate control.
  TranscoderOptionsAAC options(nullptr);
  SaveBitrate(&options, 96);
  if (::testing::Test::HasFatalFailure()) return;

  GstElement *encoder = TranscoderTest::CreateElement(u"faac"_s);
  if (!encoder) {
    GTEST_SKIP() << "faac is not installed.";
  }

  EXPECT_EQ(2, IntProperty(encoder, "rate-control"));
  EXPECT_EQ(96000, EncoderBitrate(encoder));
  gst_object_unref(encoder);

}

// With VBR, faac uses the quality.
TEST(TranscoderOptionsTest, AACQualityIsUsedByFaacWithVBR) {

  Settings().remove(u"Transcoder"_s);

  const ScopedElementRank faac_rank(u"faac"_s, GST_RANK_PRIMARY + 100);
  if (!faac_rank.found()) {
    GTEST_SKIP() << "faac is not installed.";
  }

  TranscoderOptionsAAC options(nullptr);
  options.Load();
  SelectAACRateControl(&options, 0);
  if (::testing::Test::HasFatalFailure()) return;
  QSlider *quality_slider = options.findChild<QSlider*>(u"quality_slider"_s);
  ASSERT_NE(nullptr, quality_slider);
  quality_slider->setValue(150);
  options.Save();

  GstElement *encoder = TranscoderTest::CreateElement(u"faac"_s);
  if (!encoder) {
    GTEST_SKIP() << "faac is not installed.";
  }

  EXPECT_EQ(1, IntProperty(encoder, "rate-control"));
  EXPECT_EQ(150, IntProperty(encoder, "quality"));
  gst_object_unref(encoder);

}

// Only the setting used by the rate control is enabled, also when the rate control is loaded.
TEST(TranscoderOptionsTest, AACRateControlEnablesBitrateOrQuality) {

  Settings().remove(u"Transcoder"_s);

  const ScopedElementRank faac_rank(u"faac"_s, GST_RANK_PRIMARY + 100);
  if (!faac_rank.found()) {
    GTEST_SKIP() << "faac is not installed.";
  }

  TranscoderOptionsAAC options(nullptr);
  options.Load();

  // All options are shown when faac is the encoder.
  EXPECT_FALSE(options.findChild<QComboBox*>(u"rate_control"_s)->isHidden());
  EXPECT_FALSE(options.findChild<QComboBox*>(u"shortctl"_s)->isHidden());
  EXPECT_FALSE(options.findChild<QCheckBox*>(u"tns"_s)->isHidden());
  EXPECT_FALSE(options.findChild<QCheckBox*>(u"midside"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QLabel*>(u"encoder_info"_s)->isHidden());

  QSlider *bitrate_slider = options.findChild<QSlider*>(u"bitrate_slider"_s);
  QSpinBox *bitrate_spinbox = options.findChild<QSpinBox*>(u"bitrate_spinbox"_s);
  QSlider *quality_slider = options.findChild<QSlider*>(u"quality_slider"_s);
  QSpinBox *quality_spinbox = options.findChild<QSpinBox*>(u"quality_spinbox"_s);
  ASSERT_NE(nullptr, bitrate_slider);
  ASSERT_NE(nullptr, bitrate_spinbox);
  ASSERT_NE(nullptr, quality_slider);
  ASSERT_NE(nullptr, quality_spinbox);

  EXPECT_TRUE(bitrate_slider->isEnabled());
  EXPECT_TRUE(bitrate_spinbox->isEnabled());
  EXPECT_FALSE(quality_slider->isEnabled());
  EXPECT_FALSE(quality_spinbox->isEnabled());

  SelectAACRateControl(&options, 0);
  if (::testing::Test::HasFatalFailure()) return;
  EXPECT_FALSE(bitrate_slider->isEnabled());
  EXPECT_FALSE(bitrate_spinbox->isEnabled());
  EXPECT_TRUE(quality_slider->isEnabled());
  EXPECT_TRUE(quality_spinbox->isEnabled());
  options.Save();

  TranscoderOptionsAAC loaded_options(nullptr);
  loaded_options.Load();
  EXPECT_FALSE(loaded_options.findChild<QSlider*>(u"bitrate_slider"_s)->isEnabled());
  EXPECT_TRUE(loaded_options.findChild<QSlider*>(u"quality_slider"_s)->isEnabled());

}

// Review: other AAC encoders only use the bitrate from the faac options, so the faac options are hidden, and the bitrate is used also when VBR was selected for faac.
TEST(TranscoderOptionsTest, AACFaacOptionsAreHiddenForOtherEncoders) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/faac/rate-control"_s, 1);
  }

  const QString encoder_name = OtherAACEncoderName();
  if (encoder_name.isEmpty()) {
    GTEST_SKIP() << "No AAC encoder other than faac and FFmpeg is installed.";
  }
  const ScopedElementRank encoder_rank(encoder_name, GST_RANK_PRIMARY + 100);

  TranscoderOptionsAAC options(nullptr);
  options.Load();

  EXPECT_TRUE(options.findChild<QComboBox*>(u"rate_control"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QSlider*>(u"quality_slider"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QSpinBox*>(u"quality_spinbox"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QComboBox*>(u"shortctl"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QCheckBox*>(u"tns"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QCheckBox*>(u"midside"_s)->isHidden());

  EXPECT_FALSE(options.findChild<QSlider*>(u"bitrate_slider"_s)->isHidden());
  EXPECT_TRUE(options.findChild<QSlider*>(u"bitrate_slider"_s)->isEnabled());
  EXPECT_TRUE(options.findChild<QSpinBox*>(u"bitrate_spinbox"_s)->isEnabled());

  QLabel *encoder_info = options.findChild<QLabel*>(u"encoder_info"_s);
  ASSERT_NE(nullptr, encoder_info);
  EXPECT_FALSE(encoder_info->isHidden());
  EXPECT_TRUE(encoder_info->text().contains(encoder_name)) << encoder_info->text().toStdString();

}

// The options page and the transcoder agree on the AAC encoder.
TEST(TranscoderOptionsTest, BestElementIsTheEncoderUsed) {

  const QString aac_caps = Transcoder::PresetForFileType(Song::FileType::MP4).codec_mimetype_;

  GstElement *encoder = TranscoderTest::CreateEncoder(Song::FileType::MP4);
  if (!encoder) {
    GTEST_SKIP() << "No AAC encoder is installed.";
  }
  EXPECT_EQ(ElementName(encoder), Transcoder::BestElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, aac_caps));
  gst_object_unref(encoder);

  const ScopedElementRank faac_rank(u"faac"_s, GST_RANK_PRIMARY + 100);
  if (faac_rank.found()) {
    int rank = 0;
    EXPECT_EQ(u"faac"_s, Transcoder::BestElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, aac_caps, &rank));
    EXPECT_EQ(GST_RANK_PRIMARY + 100, rank);
  }

  EXPECT_TRUE(Transcoder::BestElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, QString()).isEmpty());

}

// The AAC profile was saved, but faac has no profile property, so it's removed.
TEST(TranscoderOptionsTest, LegacyAACProfileIsRemoved) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/faac/profile"_s, 2);
    s.setValue(u"Transcoder/faac/bitrate"_s, 96000);
  }

  SettingsMigration::Migrate();

  Settings s;
  EXPECT_FALSE(s.contains(u"Transcoder/faac/profile"_s));
  EXPECT_EQ(96000, s.value(u"Transcoder/faac/bitrate"_s).toInt());

}

// A bitrate saved before the rate control was saved is used by faac after upgrading, like the options dialog shows.
TEST(TranscoderOptionsTest, LegacyAACBitrateIsUsedByFaac) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/faac/bitrate"_s, 96000);
  }

  SettingsMigration::Migrate();

  GstElement *encoder = TranscoderTest::CreateElement(u"faac"_s);
  if (!encoder) {
    GTEST_SKIP() << "faac is not installed.";
  }

  EXPECT_EQ(2, IntProperty(encoder, "rate-control"));
  EXPECT_EQ(96000, EncoderBitrate(encoder));
  gst_object_unref(encoder);

}

// Saved AAC options are kept when upgrading and when the transcoder saves the defaults.
TEST(TranscoderOptionsTest, SavedAACRateControlIsKept) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/faac/bitrate"_s, 96000);
    s.setValue(u"Transcoder/faac/rate-control"_s, 1);
  }

  SettingsMigration::Migrate();
  const Transcoder transcoder;

  Settings s;
  EXPECT_EQ(1, s.value(u"Transcoder/faac/rate-control"_s).toInt());
  EXPECT_EQ(96000, s.value(u"Transcoder/faac/bitrate"_s).toInt());

}

// Without saved AAC options, faac uses the defaults the options show.
TEST(TranscoderOptionsTest, AACDefaultsAreUsedByFaac) {

  Settings().remove(u"Transcoder"_s);

  // Review: without saved AAC options, faac used VBR, while the options show ABR with 320 kbps.
  GstElement *encoder = TranscoderTest::CreateElement(u"faac"_s);
  if (!encoder) {
    GTEST_SKIP() << "faac is not installed.";
  }

  const ScopedElementRank faac_rank(u"faac"_s, GST_RANK_PRIMARY + 100);
  TranscoderOptionsAAC options(nullptr);
  options.Load();
  QComboBox *rate_control = options.findChild<QComboBox*>(u"rate_control"_s);
  QSlider *bitrate_slider = options.findChild<QSlider*>(u"bitrate_slider"_s);
  ASSERT_NE(nullptr, rate_control);
  ASSERT_NE(nullptr, bitrate_slider);

  EXPECT_EQ(1, rate_control->currentIndex());
  EXPECT_EQ(2, IntProperty(encoder, "rate-control"));
  EXPECT_EQ(320, bitrate_slider->value());
  EXPECT_EQ(320000, EncoderBitrate(encoder));
  gst_object_unref(encoder);

}

// Regression test: the transcoder saved the faac bitrate the AAC options show by default, and the saved faac bitrate is used by the other AAC encoders, so they used 320 kbps instead of their own default without the AAC options being saved.
TEST(TranscoderOptionsTest, AACDefaultBitrateIsNotUsedByOtherEncoders) {

  Settings().remove(u"Transcoder"_s);

  const QString encoder_name = OtherAACEncoderName();
  if (encoder_name.isEmpty()) {
    GTEST_SKIP() << "No AAC encoder other than faac and FFmpeg is installed.";
  }
  const ScopedElementRank encoder_rank(encoder_name, GST_RANK_PRIMARY + 100);

  GstElementFactory *factory = gst_element_factory_find(encoder_name.toUtf8().constData());
  ASSERT_NE(nullptr, factory);
  GstElement *default_encoder = gst_element_factory_create(factory, nullptr);
  gst_object_unref(factory);
  ASSERT_NE(nullptr, default_encoder);
  gst_object_ref_sink(default_encoder);
  const qint64 default_bitrate = EncoderBitrate(default_encoder);
  gst_object_unref(default_encoder);
  if (default_bitrate == 320000) {
    GTEST_SKIP() << "The default bitrate of " << encoder_name.toStdString() << " is the AAC options default.";
  }

  GstElement *encoder = TranscoderTest::CreateEncoder(Song::FileType::MP4);
  ASSERT_NE(nullptr, encoder);
  EXPECT_EQ(encoder_name, ElementName(encoder));
  EXPECT_EQ(default_bitrate, EncoderBitrate(encoder));
  gst_object_unref(encoder);

  EXPECT_FALSE(Settings().contains(u"Transcoder/faac/bitrate"_s));

}

// The WMA bitrate saved for the GStreamer 0.10 encoder name is kept when upgrading.
TEST(TranscoderOptionsTest, LegacyWMABitrateIsMigrated) {

  Settings().remove(u"Transcoder"_s);
  {
    Settings s;
    s.setValue(u"Transcoder/ffenc_wmav2/bitrate"_s, 64000);
  }

  SettingsMigration::Migrate();

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

  SettingsMigration::Migrate();

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

  const QStringList encoder_names = AACEncoderNames();

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

  SettingsMigration::Migrate();

  TranscodeDialog dialog(nullptr, nullptr);
  QComboBox *combobox_format = dialog.findChild<QComboBox*>(u"format"_s);
  ASSERT_NE(nullptr, combobox_format);
  ASSERT_NE(-1, combobox_format->currentIndex());
  EXPECT_EQ(Song::FileType::ASF, combobox_format->currentData().value<TranscoderPreset>().filetype_);
  EXPECT_EQ(Transcoder::PresetForFileType(Song::FileType::ASF).codec_mimetype_, Settings().value(u"Transcoder/last_output_format"_s).toString());

}

}  // namespace
