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

#ifndef WAVEFORMRENDERER_H
#define WAVEFORMRENDERER_H

#include <QByteArray>
#include <QSize>
#include <QPixmap>
#include <QPalette>
#include <QColor>

// Render-time amplitude shaping exponent. A gentle power-law (0.65, not a full
// square root) lifts quiet passages enough to stay visible while preserving the
// loud/quiet contrast that a more aggressive curve would flatten. Storage stays
// linear; this curve is applied only at paint time. Not user-exposed.
constexpr float kWaveformCurveExponent = 0.65f;

// Stateless utility that converts a cached SWVF blob into a seekbar pixmap:
// mirrored per-pixel min/max amplitude bars around a center line with a gentle
// perceptual curve. The result is position-independent — it carries no
// played/unplayed split and no cursor line. The caller (WaveformProxyStyle)
// owns the playhead position by rendering two pixmaps in different colors and
// compositing them around the live split.
//
// The renderer receives an already device-pixel-ratio-scaled size from the
// caller; HiDPI scaling is the caller's (WaveformProxyStyle) responsibility.
class WaveformRenderer {
 public:
  // Renders the SWVF blob to a pixmap of size, drawing every bar in bar_color.
  // palette supplies only the Window background fill. Returns a null QPixmap
  // when size is zero/negative or the blob is invalid.
  static QPixmap RenderToPixmap(const QByteArray &data, const QSize size, const QPalette &palette, const QColor &bar_color);

 private:
  WaveformRenderer() = delete;
};

#endif  // WAVEFORMRENDERER_H
