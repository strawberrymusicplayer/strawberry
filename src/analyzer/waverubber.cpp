/*
   Strawberry Music Player
   Copyright 2024, Gustavo L Conte <suporte@gu.pro.br>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QPainter>
#include <QResizeEvent>
#include "engine/enginebase.h"
#include "waverubber.h"

const char *WaveRubber::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "WaveRubber");

WaveRubber::WaveRubber(QWidget *parent)
    : AnalyzerBase(parent, 9) {}

void WaveRubber::resizeEvent(QResizeEvent *e) {

  Q_UNUSED(e)

  canvas_ = QPixmap(size());
  canvas_.fill(palette().color(QPalette::AlternateBase));

}

void WaveRubber::analyze(QPainter &p, const Scope &s, bool new_frame) {

  if (!new_frame || engine_->state() == EngineBase::State::Paused) {
    p.drawPixmap(0, 0, canvas_);
    return;
  }

  // Clear the canvas
  canvas_ = QPixmap(size());
  canvas_.fill(palette().color(QPalette::Window));

  QPainter canvas_painter(&canvas_);

  // Set the pen color to the QT palette highlight color
  canvas_painter.setPen(palette().color(QPalette::Highlight));
  // Get pointer to amplitude data
  const float *amplitude_data = s.data();

  int mid_y = height() / 4;
  int num_samples = s.size();

  float x_scale = static_cast<float>(width()) / num_samples;
  float prev_y = mid_y;

  // Draw the waveform
  for (int i = 0; i < num_samples; ++i) {

    // Normalize amplitude to 0-1 range
    float color_factor = amplitude_data[i] / 2.0f + 0.5f;
    int rgb_value = static_cast<int>(255 - color_factor * 255);
    QColor highlight_color = palette().color(QPalette::Highlight);
    // Blend blue and green with highlight color from QT palette based on amplitude
    QColor blended_color = QColor(rgb_value, highlight_color.green(), highlight_color.blue());
    canvas_painter.setPen(blended_color);

    int x = static_cast<int>(i * x_scale);
    int y = static_cast<int>(mid_y - (s[i] * mid_y));

    canvas_painter.drawLine(x, prev_y + mid_y, x + x_scale, y + mid_y);  // Draw
    prev_y = y;
  }

  canvas_painter.end();
  p.drawPixmap(0, 0, canvas_);

}

void WaveRubber::transform(Scope &s) {
  // No need transformation for waveform analyzer
  Q_UNUSED(s);
}

void WaveRubber::demo(QPainter &p) {
  analyze(p, Scope(fht_->size(), 0), new_frame_);
}
