/*
   Strawberry Music Player
   This file was part of Amarok.
   Copyright 2003-2005, Max Howell <max.howell@methylblue.com>
   Copyright 2005, Mark Kretschmann <markey@web.de>
   Copyright 2009-2010, David Sansome <davidsansome@gmail.com>
   Copyright 2010, 2014, John Maguire <john.maguire@gmail.com>

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

#include "blockanalyzer.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>

#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QColor>

#include "analyzerbase.h"
#include "fht.h"

const int BlockAnalyzer::kHeight = 3;
const int BlockAnalyzer::kWidth = 5;
const int BlockAnalyzer::kMinRows = 3;       // arbitrary
const int BlockAnalyzer::kMinColumns = 32;   // arbitrary
const int BlockAnalyzer::kMaxColumns = 256;  // must be 2**n
const int BlockAnalyzer::kFadeSize = 90;

const char *BlockAnalyzer::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "Block analyzer");

BlockAnalyzer::BlockAnalyzer(QWidget *parent)
    : Analyzer::Base(parent, 9),
      bg_(palette().color(QPalette::Window)),
      fg_(palette().color(QPalette::Highlight)),
      columns_(0),
      rows_(0),
      y_(0),
      barpixmap_(1, 1),
      topbarpixmap_(kWidth, kHeight),
      scope_(kMinColumns),
      store_(1 << 8, 0),
      fade_bars_(kFadeSize),
      fade_pos_(1 << 8, 50),
      fade_intensity_(1 << 8, 32),
      step_(0) {

  setMinimumSize(kMinColumns * (kWidth + 1) - 1, kMinRows * (kHeight + 1) - 1);  //-1 is padding, no drawing takes place there
  setMaximumWidth(kMaxColumns * (kWidth + 1) - 1);

  // mxcl says null pixmaps cause crashes, so let's play it safe
  std::fill(fade_bars_.begin(), fade_bars_.end(),  QPixmap(1, 1));

}

void BlockAnalyzer::resizeEvent(QResizeEvent *e) {

  QWidget::resizeEvent(e);

  background_ = QPixmap(size());
  canvas_ = QPixmap(size());

  const int oldRows = rows_;

  // all is explained in analyze()..
  // +1 to counter -1 in maxSizes, trust me we need this!
  columns_ = qMin(static_cast<int>(static_cast<double>(width() + 1) / (kWidth + 1)) + 1, kMaxColumns);
  rows_ = static_cast<int>(static_cast<double>(height() + 1) / (kHeight + 1));

  // this is the y-offset for drawing from the top of the widget
  y_ = (height() - (rows_ * (kHeight + 1)) + 2) / 2;

  scope_.resize(columns_);

  if (rows_ != oldRows) {
    barpixmap_ = QPixmap(kWidth, rows_ * (kHeight + 1));

    std::fill(fade_bars_.begin(), fade_bars_.end(),  QPixmap(kWidth, rows_ * (kHeight + 1)));

    yscale_.resize(rows_ + 1);

    const int PRE = 1, PRO = 1;  // PRE and PRO allow us to restrict the range somewhat

    for (int z = 0; z < rows_; ++z) {
      yscale_[z] = 1 - (log10(PRE + z) / log10(PRE + rows_ + PRO));
    }

    yscale_[rows_] = 0;

    determineStep();
    paletteChange(palette());
  }

  drawBackground();

}

void BlockAnalyzer::determineStep() {

  // falltime is dependent on rowcount due to our digital resolution (ie we have boxes/blocks of pixels)
  // I calculated the value 30 based on some trial and error

  // the fall time of 30 is too slow on framerates above 50fps
  const double fallTime = static_cast<double>(timeout() < 20 ? 20 * rows_ : 30 * rows_);

  step_ = double(rows_ * timeout()) / fallTime;

}

void BlockAnalyzer::framerateChanged() {
  determineStep();
}

void BlockAnalyzer::transform(Analyzer::Scope &s) {

  for (uint x = 0; x < s.size(); ++x) s[x] *= 2;

  fht_->spectrum(s.data());
  fht_->scale(s.data(), 1.0 / 20);

  // the second half is pretty dull, so only show it if the user has a large analyzer by setting to scope_.size() if large we prevent interpolation of large analyzers, this is good!
  s.resize(scope_.size() <= kMaxColumns / 2 ? kMaxColumns / 2 : scope_.size());

}

void BlockAnalyzer::analyze(QPainter &p, const Analyzer::Scope &s, bool new_frame) {

  // y = 2 3 2 1 0 2
  //     . . . . # .
  //     . . . # # .
  //     # . # # # #
  //     # # # # # #
  //
  // visual aid for how this analyzer works.
  // y represents the number of blanks
  // y starts from the top and increases in units of blocks

  // yscale_ looks similar to: { 0.7, 0.5, 0.25, 0.15, 0.1, 0 }
  // if it contains 6 elements there are 5 rows in the analyzer

  if (!new_frame) {
    p.drawPixmap(0, 0, canvas_);
    return;
  }

  QPainter canvas_painter(&canvas_);

  Analyzer::interpolate(s, scope_);

  // Paint the background
  canvas_painter.drawPixmap(0, 0, background_);

  for (int x = 0, y = 0; x < static_cast<int>(scope_.size()); ++x) {
    // determine y
    for (y = 0; scope_[x] < yscale_[y]; ++y) continue;

    // This is opposite to what you'd think, higher than y means the bar is lower than y (physically)
    if (static_cast<double>(y) > store_[x]) {
      y = static_cast<int>(store_[x] += step_);
    }
    else {
      store_[x] = y;
    }

    // If y is lower than fade_pos_, then the bar has exceeded the height of the fadeout
    // if the fadeout is quite faded now, then display the new one
    if (y <= fade_pos_[x] /*|| fade_intensity_[x] < kFadeSize / 3*/) {
      fade_pos_[x] = y;
      fade_intensity_[x] = kFadeSize;
    }

    if (fade_intensity_[x] > 0) {
      const int offset = --fade_intensity_[x];
      const int y2 = y_ + (fade_pos_[x] * (kHeight + 1));
      canvas_painter.drawPixmap(x * (kWidth + 1), y2, fade_bars_[offset], 0, 0, kWidth, height() - y2);
    }

    if (fade_intensity_[x] == 0) fade_pos_[x] = rows_;

    // REMEMBER: y is a number from 0 to rows_, 0 means all blocks are glowing, rows_ means none are
    canvas_painter.drawPixmap(x * (kWidth + 1), y * (kHeight + 1) + y_, *bar(), 0, y * (kHeight + 1), bar()->width(), bar()->height());
  }

  for (int x = 0; x < store_.size(); ++x) {
    canvas_painter.drawPixmap(x * (kWidth + 1), static_cast<int>(store_[x]) * (kHeight + 1) + y_, topbarpixmap_);
  }

  p.drawPixmap(0, 0, canvas_);

}

void BlockAnalyzer::paletteChange(const QPalette&) {

  topbarpixmap_.fill(fg_);

  const double dr = 15 * static_cast<double>(bg_.red() - fg_.red()) / (rows_ * 16);
  const double dg = 15 * static_cast<double>(bg_.green() - fg_.green()) / (rows_ * 16);
  const double db = 15 * static_cast<double>(bg_.blue() - fg_.blue()) / (rows_ * 16);
  const int r = fg_.red(), g = fg_.green(), b = fg_.blue();

  bar()->fill(bg_);

  QPainter p(bar());
  for (int y = 0; y < rows_; ++y) {
    // graduate the fg color
    p.fillRect(0, y * (kHeight + 1), kWidth, kHeight, QColor(r + static_cast<int>(dr * y), g + static_cast<int>(dg * y), b + static_cast<int>(db * y)));
  }

  {
    const QColor bg2 = palette().color(QPalette::Window).darker(112);

    // make a complimentary fadebar colour
    // TODO dark is not always correct, dumbo!
    int h = 0, s = 0, v = 0;
    palette().color(QPalette::Window).darker(150).getHsv(&h, &s, &v);
    const QColor fg2(QColor::fromHsv(h + 120, s, v));

    const double dr2 = fg2.red() - bg2.red();
    const double dg2 = fg2.green() - bg2.green();
    const double db2 = fg2.blue() - bg2.blue();
    const int r2 = bg2.red(), g2 = bg2.green(), b2 = bg2.blue();

    // Precalculate all fade-bar pixmaps
    for (int y = 0; y < kFadeSize; ++y) {
      fade_bars_[y].fill(palette().color(QPalette::Window));
      QPainter f(&fade_bars_[y]);
      for (int z = 0; z < rows_; ++z) {
        const double Y = 1.0 - (log10(kFadeSize - y) / log10(kFadeSize));
        f.fillRect(0, z * (kHeight + 1), kWidth, kHeight, QColor(r2 + static_cast<int>(dr2 * Y), g2 + static_cast<int>(dg2 * Y), b2 + static_cast<int>(db2 * Y)));
      }
    }
  }

  drawBackground();

}

void BlockAnalyzer::drawBackground() {

  if (background_.isNull()) {
    return;
  }

  const QColor bgdark = bg_.darker(112);

  background_.fill(bg_);

  QPainter p(&background_);

  if (!p.paintEngine()) return;

  for (int x = 0; x < columns_; ++x) {
    for (int y = 0; y < rows_; ++y) {
      p.fillRect(x * (kWidth + 1), y * (kHeight + 1) + y_, kWidth, kHeight, bgdark);
    }
  }

}
