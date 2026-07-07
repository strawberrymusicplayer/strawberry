/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include <cmath>
#include <algorithm>

#include <QtGlobal>
#include <QByteArray>
#include <QDataStream>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QPalette>
#include <QSize>
#include <QVector>

#include "waveform/waveformbuilder.h"
#include "waveformrenderer.h"

namespace {
// Full-scale value of the stored int8 amplitudes; the body holds qint8 in [-127, 127] so 127 maps to unity before the curve is applied.
constexpr float kInt8FullScale = 127.0F;

// Render-time amplitude shaping exponent.
// A gentle power-law (0.65, not a full square root) lifts quiet passages enough to stay visible while preserving the loud/quiet contrast that a more aggressive curve would flatten.
// Storage stays linear; this curve is applied only at paint time.
constexpr float kWaveformCurveExponent = 0.65F;
}  // namespace

QPixmap WaveformRenderer::RenderToPixmap(const QByteArray &data, const QSize size, const QPalette &palette, const QColor &bar_color) {

  // Zero-size groove: nothing to render (guards QPixmap allocation and div-by-W).
  if (size.width() <= 0 || size.height() <= 0) {
    return QPixmap();
  }

  // Reject malformed/truncated/future-version blobs before any byte access.
  if (!WaveformBuilder::IsValidBlob(data)) {
    return QPixmap();
  }

  // Parse the SWVF header once here, never inside the paint path.
  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  char magic[4];
  stream.readRawData(magic, 4);
  quint8 version = 0;
  stream >> version;
  quint32 base_count = 0;
  stream >> base_count;
  float peak = 0.0F;
  stream >> peak;
  // The header peak is consumed to advance the stream to the column body.
  // It is not used to normalize: bars are scaled against the fixed int8 full scale so genuinely quiet tracks render proportionally short (the contrast Bug 2 wants).
  Q_UNUSED(peak)

  struct Col {
    qint8 mn;
    qint8 mx;
  };
  QVector<Col> base(static_cast<int>(base_count));
  for (Col &c : base) {
    stream >> c.mn >> c.mx;
  }
  // A short or corrupt body leaves the stream in a non-Ok state.
  if (stream.status() != QDataStream::Ok) {
    return QPixmap();
  }

  QPixmap ret(size);
  ret.fill(palette.color(QPalette::Active, QPalette::Window));
  QPainter p(&ret);
  p.setPen(bar_color);

  const int cy = size.height() / 2;
  const int W = size.width();

  for (int x = 0; x < W; ++x) {

    // Reduce the base envelope to this column via min/max, never averaging, so transients survive.
    // qsizetype arithmetic avoids index overflow.
    qsizetype start = static_cast<qsizetype>(x) * static_cast<qsizetype>(base_count) / static_cast<qsizetype>(W);
    qsizetype end = static_cast<qsizetype>(x + 1) * static_cast<qsizetype>(base_count) / static_cast<qsizetype>(W);
    if (end <= start) end = start + 1;
    end = std::min(end, static_cast<qsizetype>(base_count));

    qint8 col_mn = base[static_cast<int>(start)].mn;
    qint8 col_mx = base[static_cast<int>(start)].mx;
    for (qsizetype j = start + 1; j < end; ++j) {
      col_mn = std::min(col_mn, base[static_cast<int>(j)].mn);
      col_mx = std::max(col_mx, base[static_cast<int>(j)].mx);
    }

    // Normalize to [0, 1] then apply the perceptual curve.
    const float amp_above = std::clamp(static_cast<float>(col_mx) / kInt8FullScale, 0.0F, 1.0F);
    const float amp_below = std::clamp(std::abs(static_cast<float>(col_mn)) / kInt8FullScale, 0.0F, 1.0F);
    const int h_above = static_cast<int>(std::pow(amp_above, kWaveformCurveExponent) * static_cast<float>(cy));
    const int h_below = static_cast<int>(std::pow(amp_below, kWaveformCurveExponent) * static_cast<float>(cy));

    p.drawLine(x, cy - h_above, x, cy + h_below);
  }

  p.end();
  return ret;
}
