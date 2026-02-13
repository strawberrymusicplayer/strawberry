/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CHROMAPRINTER_H
#define CHROMAPRINTER_H

#include "config.h"

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <QtGlobal>
#include <QString>
#include <QUrl>
#include <QBuffer>
#include <QMutex>

class Chromaprinter {
  // Creates a Chromaprint fingerprint from a song.
  // Uses GStreamer to open and decode the file as PCM data and passes this to Chromaprint's code generator.
  // The generated code can be used to identify a song via Acoustid.
  // You should create one Chromaprinter for each file you want to fingerprint.
  // This class works well with QtConcurrentMap.

 public:
  explicit Chromaprinter(const QUrl &url);
  explicit Chromaprinter(const QString &filename);

  // Creates a fingerprint from the song using the same fixed 11025Hz mono, first-30-seconds algorithm the decode pipeline used before it was hardened.
  // This method is blocking, so you want to call it in another thread.
  // Returns an empty string if no fingerprint could be created.
  // Song tracking in CollectionWatcher matches moved/renamed files by comparing this fingerprint against values already stored in the collection database, so this must keep producing output identical to what existing libraries were fingerprinted with, independently of any future changes to CreateFullFingerprint().
  QString CreateFingerprint();

  // Creates a fingerprint using the hardened decode pipeline: native sample rate/channels and up to 120 seconds of audio.
  // This method is blocking, so you want to call it in another thread.
  // Returns an empty string if no fingerprint could be created.
  QString CreateFullFingerprint();

  QString LastError() const;

 private:
  // Shared by CreateFingerprint()/CreateFullFingerprint(). legacy switches the caps filter, adds the fixed-rate resample stage, applies the first-30-seconds seek, and uses the shorter timeout.
  QString CreateFingerprintInternal(const bool legacy);

  static QByteArray ToGstUrl(const QUrl &url);
  static GstElement *CreateElement(const QString &factory_name, GstElement *bin = nullptr);
  static void NewPadCallback(GstElement *element, GstPad *pad, gpointer data);
  static GstFlowReturn NewBufferCallback(GstAppSink *app_sink, gpointer self);

  // last_error_ is written from NewPadCallback/NewBufferCallback on the GStreamer streaming thread and read/written from the calling thread's bus-polling loop, so every access goes through these, each locking mutex_last_error_.
  void ClearLastError();
  void SetLastError(const QString &error);
  // Preserves first-error-wins: only stores error if last_error_ is currently empty.
  void SetLastErrorIfEmpty(const QString &error);
  QString TrimmedLastError() const;

 private:
  QUrl url_;
  QString last_error_;
  mutable QMutex mutex_last_error_;
  // mutex_state_ protects sample_rate_, channels_, max_pcm_bytes_, pcm_limit_reached_, convert_element_, and buffer_: written by NewPadCallback/NewBufferCallback on the GStreamer streaming thread and read by CreateFingerprintInternal on the calling thread.
  QMutex mutex_state_;
  int sample_rate_;
  int channels_;
  // Hard cap for decoded PCM retained in memory to avoid unbounded growth.
  qint64 max_pcm_bytes_;
  bool pcm_limit_reached_;

  GstElement *convert_element_;

  QBuffer buffer_;
};

#endif  // CHROMAPRINTER_H
