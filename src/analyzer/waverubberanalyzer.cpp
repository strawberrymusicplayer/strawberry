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
#include "waverubberanalyzer.h"

const char *WaveRubberAnalyzer::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "WaveRubber");

WaveRubberAnalyzer::WaveRubberAnalyzer(QWidget *parent)
    : AnalyzerBase(parent, 9) {}

void WaveRubberAnalyzer::resizeEvent(QResizeEvent *e) {

  Q_UNUSED(e)

  canvas_ = QPixmap(size());
  canvas_.fill(palette().color(QPalette::AlternateBase));

}

void WaveRubberAnalyzer::analyze(QPainter &p, const Scope &s, const bool new_frame) {

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

  const int mid_y = height() / 4;
  const size_t num_samples = static_cast<size_t>(s.size());

  const float x_scale = static_cast<float>(width()) / static_cast<float>(num_samples);
  float prev_y = static_cast<float>(mid_y);

  // Draw the waveform
  for (size_t i = 0; i < num_samples; ++i) {

    // Normalize amplitude to 0-1 range
    const float color_factor = amplitude_data[i] / 2.0F + 0.5F;
    const int rgb_value = static_cast<int>(255 - color_factor * 255);
    QColor highlight_color = palette().color(QPalette::Highlight);
    // Blend blue and green with highlight color from QT palette based on amplitude
    QColor blended_color = QColor(rgb_value, highlight_color.green(), highlight_color.blue());
    canvas_painter.setPen(blended_color);

    const int x = static_cast<int>(static_cast<float>(i) * x_scale);
    const int y = static_cast<int>(static_cast<float>(mid_y) - (s[i] * static_cast<float>(mid_y)));

    canvas_painter.drawLine(x, static_cast<int>(prev_y + static_cast<float>(mid_y)), static_cast<int>(static_cast<float>(x) + x_scale), static_cast<int>(static_cast<float>(y + mid_y)));  // Draw
    prev_y = static_cast<float>(y);
  }

  canvas_painter.end();
  p.drawPixmap(0, 0, canvas_);

}

void WaveRubberAnalyzer::transform(Scope &scope) {
  // No need transformation for waveform analyzer
  Q_UNUSED(scope);
}

void WaveRubberAnalyzer::demo(QPainter &p) {
  analyze(p, Scope(static_cast<size_t>(fht_->size()), 0), new_frame_);
}
