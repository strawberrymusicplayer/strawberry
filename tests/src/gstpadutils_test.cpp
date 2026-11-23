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

#include <glib.h>
#include <gst/gst.h>

#include <QtGlobal>

#include "engine/gstpadutils.h"

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

// Counts the GStreamer criticals, a call with wrong arguments logs a critical and returns without doing anything.
int critical_count = 0;

void CountCriticals(const gchar *log_domain, const GLogLevelFlags log_level, const gchar *message, gpointer user_data) {

  Q_UNUSED(log_domain)
  Q_UNUSED(log_level)
  Q_UNUSED(message)
  Q_UNUSED(user_data)

  ++critical_count;

}

class GstPadUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {

    gst_init(nullptr, nullptr);
    critical_count = 0;
    handler_id_ = g_log_set_handler("GStreamer", static_cast<GLogLevelFlags>(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION), CountCriticals, nullptr);

    first_source_ = gst_element_factory_make("identity", "first_source");
    second_source_ = gst_element_factory_make("identity", "second_source");
    sink_ = gst_element_factory_make("identity", "sink");
    ASSERT_NE(nullptr, first_source_);
    ASSERT_NE(nullptr, second_source_);
    ASSERT_NE(nullptr, sink_);

    first_srcpad_ = gst_element_get_static_pad(first_source_, "src");
    second_srcpad_ = gst_element_get_static_pad(second_source_, "src");
    sinkpad_ = gst_element_get_static_pad(sink_, "sink");

  }

  void TearDown() override {

    gst_object_unref(first_srcpad_);
    gst_object_unref(second_srcpad_);
    gst_object_unref(sinkpad_);
    gst_object_unref(first_source_);
    gst_object_unref(second_source_);
    gst_object_unref(sink_);
    g_log_remove_handler("GStreamer", handler_id_);

  }

  guint handler_id_ = 0;
  GstElement *first_source_ = nullptr;
  GstElement *second_source_ = nullptr;
  GstElement *sink_ = nullptr;
  GstPad *first_srcpad_ = nullptr;
  GstPad *second_srcpad_ = nullptr;
  GstPad *sinkpad_ = nullptr;
};

// Regression test: the sink pad was passed to gst_pad_unlink() as the source pad, so the old pad was never unlinked and the new pad could not be linked.
TEST_F(GstPadUtilsTest, UnlinkSinkPadAllowsLinkingAnotherPad) {

  ASSERT_EQ(GST_PAD_LINK_OK, gst_pad_link(first_srcpad_, sinkpad_));

  EXPECT_TRUE(GstUnlinkSinkPad(sinkpad_));
  EXPECT_FALSE(gst_pad_is_linked(sinkpad_));
  EXPECT_FALSE(gst_pad_is_linked(first_srcpad_));

  EXPECT_EQ(GST_PAD_LINK_OK, gst_pad_link(second_srcpad_, sinkpad_));
  EXPECT_EQ(second_srcpad_, GST_PAD_PEER(sinkpad_));
  EXPECT_EQ(0, critical_count);

}

TEST_F(GstPadUtilsTest, UnlinkSinkPadWhichIsNotLinked) {

  EXPECT_FALSE(GstUnlinkSinkPad(sinkpad_));
  EXPECT_EQ(0, critical_count);

}

}  // namespace
