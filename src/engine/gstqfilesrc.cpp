// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits>

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>

#include "gstqfilesrc.h"

namespace {

struct GstQFileSource {
  GstBaseSrc parent;
  QFile *file;
  QMutex *mutex;
  gchar *uri;
};

struct GstQFileSourceClass {
  GstBaseSrcClass parent;
};

void UriInterfaceInit(gpointer interface, gpointer data);

// GObject's type macros expand to C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
G_DEFINE_TYPE_WITH_CODE(GstQFileSource, gst_qfile_source, GST_TYPE_BASE_SRC,
                       G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, UriInterfaceInit))

gboolean Start(GstBaseSrc *source) {
  auto *self = reinterpret_cast<GstQFileSource*>(source);
  QMutexLocker locker(self->mutex);
  self->file->setFileName(QUrl(QString::fromUtf8(self->uri)).toLocalFile());
  if (!self->file->open(QIODevice::ReadOnly)) {
    GST_ELEMENT_ERROR(source, RESOURCE, OPEN_READ, ("Could not open audio file"), ("%s", qPrintable(self->file->errorString())));
    return FALSE;
  }
  if (self->file->isSequential()) {
    GST_ELEMENT_ERROR(source, RESOURCE, SEEK, ("Audio document does not support seeking"), (nullptr));
    self->file->close();
    return FALSE;
  }
  return TRUE;
}

gboolean Stop(GstBaseSrc *source) {
  auto *self = reinterpret_cast<GstQFileSource*>(source);
  QMutexLocker locker(self->mutex);
  self->file->close();
  return TRUE;
}

gboolean GetSize(GstBaseSrc *source, guint64 *size) {
  auto *self = reinterpret_cast<GstQFileSource*>(source);
  QMutexLocker locker(self->mutex);
  const qint64 length = self->file->size();
  if (length < 0) {
    GST_ELEMENT_ERROR(source, RESOURCE, READ, ("Could not read audio document size"), (nullptr));
    return FALSE;
  }
  *size = static_cast<guint64>(length);
  return TRUE;
}

gboolean IsSeekable(GstBaseSrc *) { return TRUE; }

GstFlowReturn Create(GstBaseSrc *source, guint64 offset, guint count, GstBuffer **buffer) {
  auto *self = reinterpret_cast<GstQFileSource*>(source);
  QMutexLocker locker(self->mutex);
  QFile *file = self->file;
  if (offset > static_cast<guint64>(std::numeric_limits<qint64>::max()) || !file->seek(static_cast<qint64>(offset))) {
    GST_ELEMENT_ERROR(source, RESOURCE, SEEK, ("Could not seek audio document"), ("%s", qPrintable(file->errorString())));
    return GST_FLOW_ERROR;
  }
  GstBuffer *result = gst_buffer_new_allocate(nullptr, count, nullptr);
  GstMapInfo map;
  if (!result || !gst_buffer_map(result, &map, GST_MAP_WRITE)) {
    if (result) gst_buffer_unref(result);
    GST_ELEMENT_ERROR(source, RESOURCE, NO_SPACE_LEFT, ("Could not allocate audio read buffer"), (nullptr));
    return GST_FLOW_ERROR;
  }
  const qint64 read = file->read(reinterpret_cast<char*>(map.data), count);
  gst_buffer_unmap(result, &map);
  if (read <= 0) {
    gst_buffer_unref(result);
    if (read == 0) return GST_FLOW_EOS;
    GST_ELEMENT_ERROR(source, RESOURCE, READ, ("Could not read audio document"), ("%s", qPrintable(file->errorString())));
    return GST_FLOW_ERROR;
  }
  gst_buffer_resize(result, 0, read);
  GST_BUFFER_OFFSET(result) = offset;
  GST_BUFFER_OFFSET_END(result) = offset + static_cast<guint64>(read);
  *buffer = result;
  return GST_FLOW_OK;
}

void Finalize(GObject *object) {
  auto *self = reinterpret_cast<GstQFileSource*>(object);
  delete self->file;
  delete self->mutex;
  g_free(self->uri);
  G_OBJECT_CLASS(gst_qfile_source_parent_class)->finalize(object);
}

GstURIType UriType(GType) { return GST_URI_SRC; }

const gchar *const *UriProtocols(GType) {
  static const gchar *protocols[] = {"file", nullptr};
  return protocols;
}

gchar *GetUri(GstURIHandler *handler) {
  auto *self = reinterpret_cast<GstQFileSource*>(handler);
  QMutexLocker locker(self->mutex);
  return g_strdup(self->uri);
}

gboolean SetUri(GstURIHandler *handler, const gchar *uri, GError **error) {
  auto *self = reinterpret_cast<GstQFileSource*>(handler);
  QMutexLocker locker(self->mutex);
  if (GST_STATE(self) != GST_STATE_NULL) {
    g_set_error_literal(error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE, "Cannot change an open file URI");
    return FALSE;
  }
  const QUrl url(QString::fromUtf8(uri));
  if (!url.isLocalFile() || !url.host().isEmpty() || url.toLocalFile().isEmpty()) {
    g_set_error_literal(error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI, "Expected a local file URI");
    return FALSE;
  }
  g_free(self->uri);
  self->uri = g_strdup(uri);
  return TRUE;
}

void UriInterfaceInit(gpointer interface, gpointer) {
  auto *uri_interface = static_cast<GstURIHandlerInterface*>(interface);
  uri_interface->get_type = UriType;
  uri_interface->get_protocols = UriProtocols;
  uri_interface->get_uri = GetUri;
  uri_interface->set_uri = SetUri;
}

void gst_qfile_source_class_init(GstQFileSourceClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = Finalize;
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseSrcClass *source_class = GST_BASE_SRC_CLASS(klass);
  source_class->start = Start;
  source_class->stop = Stop;
  source_class->get_size = GetSize;
  source_class->is_seekable = IsSeekable;
  source_class->create = Create;
  gst_element_class_set_static_metadata(element_class, "Qt document file source", "Source/File", "Read seekable files through Qt's file engines", "Strawberry");
  GstCaps *caps = gst_caps_new_any();
  GstPadTemplate *pad = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template(element_class, pad);
  gst_caps_unref(caps);
}

void gst_qfile_source_init(GstQFileSource *self) {
  self->file = new QFile;
  self->mutex = new QMutex;
  self->uri = nullptr;
  gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_BYTES);
}

#pragma GCC diagnostic pop
}  // namespace

bool RegisterGstQFileSource() {
  return gst_element_register(nullptr, "strawberryqfilesrc", GST_RANK_PRIMARY + 1, gst_qfile_source_get_type()) != FALSE;
}
