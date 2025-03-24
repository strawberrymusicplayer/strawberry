/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2004, Melchior FRANZ <mfranz@kde.org>
   Copyright 2009-2010, David Sansome <davidsansome@gmail.com>
   Copyright 2010, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2014-2015, Mark Furneaux <mark@furneaux.ca>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>

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

#include "sonogramanalyzer.h"

const char *SonogramAnalyzer::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "Sonogram");

SonogramAnalyzer::SonogramAnalyzer(QWidget *parent)
    : AnalyzerBase(parent, 9) {}

void SonogramAnalyzer::resizeEvent(QResizeEvent *e) {

  Q_UNUSED(e)

  canvas_ = QPixmap(size());
  canvas_.fill(palette().color(QPalette::Window));

}

void SonogramAnalyzer::analyze(QPainter &p, const Scope &s, const bool new_frame) {

  if (!new_frame || engine_->state() == EngineBase::State::Paused) {
    p.drawPixmap(0, 0, canvas_);
    return;
  }

  QPainter canvas_painter(&canvas_);
  canvas_painter.drawPixmap(0, 0, canvas_, 1, 0, width() - 1, -1);

  Scope::const_iterator it = s.begin(), end = s.end();

  for (int y = height() - 1; y;) {
    QColor c;
    if (it >= end || *it < .005) {
      c = palette().color(QPalette::Window);
    }
    else if (*it < .05) {
      c.setHsv(95, 255, 255 - static_cast<int>(*it * 4000.0));
    }
    else if (*it < 1.0) {
      c.setHsv(95 - static_cast<int>(*it * 90.0), 255, 255);
    }
    else {
      c = Qt::red;
    }

    canvas_painter.setPen(c);
    canvas_painter.drawPoint(width() - 1, y--);

    if (it < end) ++it;
  }

  canvas_painter.end();

  p.drawPixmap(0, 0, canvas_);

}

void SonogramAnalyzer::transform(Scope &scope) {

  fht_->power2(scope.data());
  fht_->scale(scope.data(), 1.0 / 256);
  scope.resize(static_cast<size_t>(fht_->size() / 2));

}

void SonogramAnalyzer::demo(QPainter &p) {
  analyze(p, Scope(static_cast<size_t>(fht_->size()), 0), new_frame_);
}
