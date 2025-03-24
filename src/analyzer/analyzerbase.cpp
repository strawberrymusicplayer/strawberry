/*
   Strawberry Music Player
   This file was part of Amarok.
   Copyright 2003-2004, Max Howell <max.howell@methylblue.com>
   Copyright 2009-2012, David Sansome <me@davidsansome.com>
   Copyright 2010, 2012, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2017, Santiago Gil

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

#include "config.h"

#include "analyzerbase.h"

#include <cstdint>
#include <cmath>
#include <algorithm>

#include <QWidget>
#include <QList>
#include <QPainter>
#include <QPalette>
#include <QBasicTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimerEvent>

#include "engine/enginebase.h"

// INSTRUCTIONS Base2D
// 1. do anything that depends on height() in init(), Base2D will call it before you are shown
// 2. otherwise you can use the constructor to initialize things
// 3. reimplement analyze(), and paint to canvas(), Base2D will update the widget when you return control to it
// 4. if you want to manipulate the scope, reimplement transform()
// 5. for convenience <vector> <qpixmap.h> <qwdiget.h> are pre-included
//
// TODO:
// Make an INSTRUCTIONS file
// can't mod scope in analyze you have to use transform for 2D use setErasePixmap Qt function insetead of m_background

AnalyzerBase::AnalyzerBase(QWidget *parent, const uint scope_size)
    : QWidget(parent),
      fht_(new FHT(scope_size)),
      engine_(nullptr),
      lastscope_(512),
      new_frame_(false),
      is_playing_(false),
      timeout_(40) {

  setAttribute(Qt::WA_OpaquePaintEvent, true);

}

AnalyzerBase::~AnalyzerBase() {
  delete fht_;
}

void AnalyzerBase::showEvent(QShowEvent *e) {
  Q_UNUSED(e)
  timer_.start(timeout(), this);
}

void AnalyzerBase::hideEvent(QHideEvent *e) {
  Q_UNUSED(e)
  timer_.stop();
}

void AnalyzerBase::ChangeTimeout(const int timeout) {

  timeout_ = timeout;
  if (timer_.isActive()) {
    timer_.stop();
    timer_.start(timeout_, this);
  }

}

void AnalyzerBase::transform(Scope &scope) {

  QList<float> aux(fht_->size());
  if (static_cast<quint64>(aux.size()) >= scope.size()) {
    std::copy(scope.begin(), scope.end(), aux.begin());
  }
  else {
    std::copy(scope.begin(), scope.begin() + aux.size(), aux.begin());
  }

  fht_->logSpectrum(scope.data(), aux.data());
  fht_->scale(scope.data(), 1.0F / 20);

  scope.resize(static_cast<size_t>(fht_->size() / 2));  // second half of values are rubbish

}

void AnalyzerBase::paintEvent(QPaintEvent *e) {

  QPainter p(this);
  p.fillRect(e->rect(), palette().color(QPalette::Window));

  switch (engine_->state()) {
    case EngineBase::State::Playing:{
      const EngineBase::Scope &thescope = engine_->scope(timeout_);
      size_t i = 0;

      // convert to mono here - our built in analyzers need mono, but the engines provide interleaved pcm
      for (uint x = 0; static_cast<int>(x) < fht_->size(); ++x) {
        lastscope_[x] = static_cast<float>(thescope[i] + thescope[i + 1]) / (2 * (1U << 15U));
        i += 2;
      }

      is_playing_ = true;
      transform(lastscope_);
      analyze(p, lastscope_, new_frame_);

      lastscope_.resize(static_cast<size_t>(fht_->size()));

      break;
    }
    case EngineBase::State::Paused:
      is_playing_ = false;
      analyze(p, lastscope_, new_frame_);
      break;

    default:
      is_playing_ = false;
      demo(p);
  }

  new_frame_ = false;

}

int AnalyzerBase::resizeExponent(int exp) {

  if (exp < 3) {
    exp = 3;
  }
  else if (exp > 9) {
    exp = 9;
  }

  if (exp != fht_->sizeExp()) {
    delete fht_;
    fht_ = new FHT(static_cast<uint>(exp));
  }
  return exp;

}

int AnalyzerBase::resizeForBands(const int bands) {

  int exp = 0;
  if (bands <= 8) {
    exp = 4;
  }
  else if (bands <= 16) {
    exp = 5;
  }
  else if (bands <= 32) {
    exp = 6;
  }
  else if (bands <= 64) {
    exp = 7;
  }
  else if (bands <= 128) {
    exp = 8;
  }
  else {
    exp = 9;
  }

  resizeExponent(exp);
  return fht_->size() / 2;

}

void AnalyzerBase::demo(QPainter &p) {

  static int t = 201;  // FIXME make static to namespace perhaps

  if (t > 999) {
    t = 1;  // 0 = wasted calculations
  }
  if (t < 201) {
    Scope s(32);

    const double dt = static_cast<double>(t) / 200;
    for (uint i = 0; i < s.size(); ++i) {
      s[i] = static_cast<float>(dt * (sin(M_PI + (i * M_PI) / static_cast<double>(s.size())) + 1.0));
    }

    analyze(p, s, new_frame_);
  }
  else {
    analyze(p, Scope(32, 0), new_frame_);
  }

  ++t;

}

void AnalyzerBase::interpolate(const Scope &in_scope, Scope &out_scope) {

  double pos = 0.0;
  const double step = static_cast<double>(in_scope.size()) / static_cast<double>(out_scope.size());

  for (uint i = 0; i < out_scope.size(); ++i, pos += step) {
    const double error = pos - std::floor(pos);
    const uint64_t offset = static_cast<uint64_t>(pos);

    uint64_t indexLeft = offset + 0;

    if (indexLeft >= in_scope.size()) {
      indexLeft = in_scope.size() - 1;
    }

    uint64_t indexRight = offset + 1;

    if (indexRight >= in_scope.size()) {
      indexRight = in_scope.size() - 1;
    }

    out_scope[i] = in_scope[indexLeft] * (1.0F - static_cast<float>(error)) + in_scope[indexRight] * static_cast<float>(error);
  }

}

void AnalyzerBase::initSin(Scope &v, const uint size) {

  double step = (M_PI * 2) / size;
  double radian = 0;

  for (uint i = 0; i < size; i++) {
    v.push_back(static_cast<float>(sin(radian)));
    radian += step;
  }

}

void AnalyzerBase::timerEvent(QTimerEvent *e) {

  QWidget::timerEvent(e);
  if (e->timerId() != timer_.timerId()) {
    return;
  }

  new_frame_ = true;
  update();

}
