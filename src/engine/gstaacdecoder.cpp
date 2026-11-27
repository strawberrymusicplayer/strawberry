// SPDX-License-Identifier: GPL-3.0-or-later

#include <initializer_list>
#include <string>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include "gstaacdecoder.h"

namespace {

std::string decoder_factory;

struct StrawberryAacVersion {
  GstBaseTransform parent;
};
struct StrawberryAacVersionClass {
  GstBaseTransformClass parent;
};
struct StrawberryAdtsDecoder {
  GstBin parent;
  gboolean linked;
};
struct StrawberryAdtsDecoderClass {
  GstBinClass parent;
};

// GObject and GStreamer macros contain C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
G_DEFINE_TYPE(StrawberryAacVersion, aac_version, GST_TYPE_BASE_TRANSFORM)
G_DEFINE_TYPE(StrawberryAdtsDecoder, aac_decoder, GST_TYPE_BIN)

constexpr char kRawInput[] = "audio/mpeg, mpegversion=(int){2,4}, stream-format=(string)raw, framed=(boolean)true";
constexpr char kRawOutput[] = "audio/mpeg, mpegversion=(int)4, stream-format=(string)raw, framed=(boolean)true";
constexpr char kAdtsInput[] = "audio/mpeg, mpegversion=(int){2,4}, stream-format=(string)adts, framed=(boolean)true";

GstCaps *TransformCaps(GstBaseTransform *, GstPadDirection direction, GstCaps *caps, GstCaps *filter) {
  GstCaps *result = gst_caps_copy(caps);
  for (guint i = 0; i < gst_caps_get_size(result); ++i) {
    GstStructure *structure = gst_caps_get_structure(result, i);
    if (direction == GST_PAD_SINK) {
      gst_structure_set(structure, "mpegversion", G_TYPE_INT, 4, nullptr);
    }
    else {
      // ADTS's MPEG-2 AAC Main/LC/SSR payload and AudioSpecificConfig can be decoded as MPEG-4 AAC.
      GValue versions = G_VALUE_INIT;
      GValue version = G_VALUE_INIT;
      g_value_init(&versions, GST_TYPE_LIST);
      g_value_init(&version, G_TYPE_INT);
      for (int number : {2, 4}) {
        g_value_set_int(&version, number);
        gst_value_list_append_value(&versions, &version);
      }
      gst_structure_set_value(structure, "mpegversion", &versions);
      g_value_unset(&version);
      g_value_unset(&versions);
    }
  }
  if (filter) {
    GstCaps *intersection = gst_caps_intersect_full(filter, result, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref(result);
    result = intersection;
  }
  return result;
}

GstFlowReturn PassFrame(GstBaseTransform *, GstBuffer *) { return GST_FLOW_OK; }

void AddPad(GstElementClass *klass, const char *name, GstPadDirection direction, const char *caps_string) {
  GstCaps *caps = gst_caps_from_string(caps_string);
  gst_element_class_add_pad_template(klass, gst_pad_template_new(name, direction, GST_PAD_ALWAYS, caps));
  gst_caps_unref(caps);
}

void aac_version_class_init(StrawberryAacVersionClass *klass) {
  GstElementClass *element = GST_ELEMENT_CLASS(klass);
  gst_element_class_set_static_metadata(element, "AAC version compatibility", "Filter/Audio", "Present raw MPEG-2 AAC as MPEG-4 AAC", "Strawberry");
  AddPad(element, "sink", GST_PAD_SINK, kRawInput);
  AddPad(element, "src", GST_PAD_SRC, kRawOutput);
  GstBaseTransformClass *transform = GST_BASE_TRANSFORM_CLASS(klass);
  transform->transform_caps = TransformCaps;
  transform->transform_ip = PassFrame;
}

void aac_version_init(StrawberryAacVersion *self) {
  gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
}

GstStateChangeReturn ChangeState(GstElement *element, GstStateChange transition) {
  auto *self = reinterpret_cast<StrawberryAdtsDecoder*>(element);
  if (transition == GST_STATE_CHANGE_NULL_TO_READY && !self->linked) {
    GST_ELEMENT_ERROR(element, CORE, NEGOTIATION, ("Could not construct raw AAC conversion pipeline"), (nullptr));
    return GST_STATE_CHANGE_FAILURE;
  }
  return GST_ELEMENT_CLASS(aac_decoder_parent_class)->change_state(element, transition);
}

void aac_decoder_class_init(StrawberryAdtsDecoderClass *klass) {
  GstElementClass *element = GST_ELEMENT_CLASS(klass);
  gst_element_class_set_static_metadata(element, "Android ADTS AAC decoder", "Codec/Decoder/Audio", "Convert ADTS to raw AAC before decoding", "Strawberry");
  AddPad(element, "sink", GST_PAD_SINK, kAdtsInput);
  AddPad(element, "src", GST_PAD_SRC, "audio/x-raw");
  element->change_state = ChangeState;
}

void aac_decoder_init(StrawberryAdtsDecoder *self) {
  GstElement *parser = gst_element_factory_make("aacparse", nullptr);
  GstElement *version = GST_ELEMENT(g_object_new(aac_version_get_type(), nullptr));
  GstElement *decoder = gst_element_factory_make(decoder_factory.c_str(), nullptr);
  if (!parser || !decoder) {
    if (parser) gst_object_unref(parser);
    if (decoder) gst_object_unref(decoder);
    gst_object_unref(version);
    GST_ELEMENT_ERROR(self, CORE, MISSING_PLUGIN, ("AAC parser or platform decoder is unavailable"), (nullptr));
    return;
  }
  gst_bin_add_many(GST_BIN(self), parser, version, decoder, nullptr);
  // The version element accepts raw AAC only, forcing aacparse to strip ADTS and generate codec_data.
  if (!gst_element_link_many(parser, version, decoder, nullptr)) {
    GST_ELEMENT_ERROR(self, CORE, NEGOTIATION, ("Could not link raw AAC conversion pipeline"), (nullptr));
    return;
  }
  GstPad *sink = gst_element_get_static_pad(parser, "sink");
  GstPad *src = gst_element_get_static_pad(decoder, "src");
  gst_element_add_pad(GST_ELEMENT(self), gst_ghost_pad_new("sink", sink));
  gst_element_add_pad(GST_ELEMENT(self), gst_ghost_pad_new("src", src));
  gst_object_unref(sink);
  gst_object_unref(src);
  self->linked = TRUE;
}
#pragma GCC diagnostic pop

}  // namespace

bool RegisterGstAacDecoder(const char *decoder) {
  GstElementFactory *factory = gst_element_factory_find(decoder);
  if (!factory) return false;
  gst_object_unref(factory);
  decoder_factory = decoder;
  return gst_element_register(nullptr, "strawberryadtsdecoder", GST_RANK_PRIMARY + 1, aac_decoder_get_type()) != FALSE;
}
