/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2003, Stanislav Karchebny <berkus@users.sf.net>
   Copyright 2003, Max Howell <max.howell@methylblue.com>
   Copyright 2009-2010, David Sansome <davidsansome@gmail.com>
   Copyright 2014-2015, Mark Furneaux <mark@furneaux.ca>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
   Copyright 2014, John Maguire <john.maguire@gmail.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <cmath>
#include <algorithm>

#include <QPainter>

#include "turbineanalyzer.h"
#include "engine/enginebase.h"

const char *TurbineAnalyzer::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "Turbine");

TurbineAnalyzer::TurbineAnalyzer(QWidget *parent) : BoomAnalyzer(parent) {}

void TurbineAnalyzer::analyze(QPainter &p, const Scope &scope, const bool new_frame) {

  if (!new_frame || engine_->state() == EngineBase::State::Paused) {
    p.drawPixmap(0, 0, canvas_);
    return;
  }

  const uint hd2 = static_cast<uint>(height() / 2);
  const uint kMaxHeight = hd2 - 1;

  QPainter canvas_painter(&canvas_);
  canvas_.fill(palette().color(QPalette::Window));

  AnalyzerBase::interpolate(scope, scope_);

  for (uint i = 0, x = 0, y = 0; i < static_cast<uint>(bands_); ++i, x += kColumnWidth + 1) {
    float h = static_cast<float>(std::min(log10(scope_[i] * 256.0) * F_ * 0.5, kMaxHeight * 1.0));

    if (h > bar_height_[i]) {
      bar_height_[i] = h;
      if (h > peak_height_[i]) {
        peak_height_[i] = h;
        peak_speed_[i] = 0.01;
      }
      else {
        goto peak_handling;
      }
    }
    else {
      if (bar_height_[i] > 0.0) {
        bar_height_[i] -= K_barHeight_;  // 1.4
        bar_height_[i] = std::max(0.0, bar_height_[i]);
      }

    peak_handling:
      if (peak_height_[i] > 0.0) {
        peak_height_[i] -= peak_speed_[i];
        peak_speed_[i] *= F_peakSpeed_;  // 1.12
        peak_height_[i] = std::max(0.0, std::max(bar_height_[i], peak_height_[i]));
      }
    }

    y = hd2 - static_cast<uint>(bar_height_[i]);
    canvas_painter.drawPixmap(static_cast<int>(x + 1), static_cast<int>(y), barPixmap_, 0, static_cast<int>(y), -1, -1);
    canvas_painter.drawPixmap(static_cast<int>(x + 1), static_cast<int>(hd2), barPixmap_, 0, static_cast<int>(bar_height_[i]), -1, -1);

    canvas_painter.setPen(fg_);
    if (bar_height_[i] > 0) {
      canvas_painter.drawRect(static_cast<int>(x), static_cast<int>(y), kColumnWidth - 1, static_cast<int>(bar_height_[i]) * 2 - 1);
    }

    const uint x2 = x + kColumnWidth - 1;
    canvas_painter.setPen(palette().color(QPalette::Midlight));
    y = hd2 - static_cast<uint>(peak_height_[i]);
    canvas_painter.drawLine(static_cast<int>(x), static_cast<int>(y), static_cast<int>(x2), static_cast<int>(y));
    y = hd2 + static_cast<uint>(peak_height_[i]);
    canvas_painter.drawLine(static_cast<int>(x), static_cast<int>(y), static_cast<int>(x2), static_cast<int>(y));
  }

  p.drawPixmap(0, 0, canvas_);

}
