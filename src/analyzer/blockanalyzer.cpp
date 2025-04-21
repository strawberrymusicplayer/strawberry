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

namespace {
constexpr int kHeight = 2;
constexpr int kWidth = 4;
constexpr int kMinRows = 3;       // arbitrary
constexpr int kMinColumns = 32;   // arbitrary
constexpr int kMaxColumns = 256;  // must be 2**n
constexpr int kFadeSize = 90;
}  // namespace

const char *BlockAnalyzer::kName = QT_TRANSLATE_NOOP("AnalyzerContainer", "Block analyzer");

BlockAnalyzer::BlockAnalyzer(QWidget *parent)
    : AnalyzerBase(parent, 9),
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
  std::fill(fade_bars_.begin(), fade_bars_.end(), QPixmap(1, 1));
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

  scope_.resize(static_cast<size_t>(columns_));

  if (rows_ != oldRows) {
    barpixmap_ = QPixmap(kWidth, rows_ * (kHeight + 1));

    std::fill(fade_bars_.begin(), fade_bars_.end(), QPixmap(kWidth, rows_ * (kHeight + 1)));

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

  step_ = static_cast<double>(rows_ * timeout()) / fallTime;

}

void BlockAnalyzer::framerateChanged() {
  determineStep();
}

void BlockAnalyzer::transform(Scope &s) {

  for (uint x = 0; x < s.size(); ++x) s[x] *= 2;

  fht_->spectrum(s.data());
  fht_->scale(s.data(), 1.0F / 20);

  // the second half is pretty dull, so only show it if the user has a large analyzer by setting to scope_.size() if large we prevent interpolation of large analyzers, this is good!
  s.resize(scope_.size() <= kMaxColumns / 2 ? kMaxColumns / 2 : scope_.size());

}

void BlockAnalyzer::analyze(QPainter &p, const Scope &s, const bool new_frame) {

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

  interpolate(s, scope_);

  // Paint the background
  canvas_painter.drawPixmap(0, 0, background_);

  for (qint64 x = 0, y = 0; x < static_cast<qint64>(scope_.size()); ++x) {
    // determine y
    for (y = 0; scope_[static_cast<quint64>(x)] < yscale_.at(y); ++y);

    // This is opposite to what you'd think, higher than y means the bar is lower than y (physically)
    if (static_cast<double>(y) > store_.at(x)) {
      store_[x] += step_;
      y = static_cast<int>(store_.value(x));
    }
    else {
      store_[x] = static_cast<double>(y);
    }

    // If y is lower than fade_pos_, then the bar has exceeded the height of the fadeout
    // if the fadeout is quite faded now, then display the new one
    if (y <= fade_pos_.at(x) /*|| fade_intensity_[x] < kFadeSize / 3*/) {
      fade_pos_[x] = static_cast<int>(y);
      fade_intensity_[x] = kFadeSize;
    }

    if (fade_intensity_.at(x) > 0) {
      --fade_intensity_[x];
      const int offset = fade_intensity_.value(x);
      const int y2 = y_ + (fade_pos_.value(x) * (kHeight + 1));
      canvas_painter.drawPixmap(static_cast<int>(x) * (kWidth + 1), y2, fade_bars_[offset], 0, 0, kWidth, height() - y2);
    }

    if (fade_intensity_.at(x) == 0) fade_pos_[x] = rows_;

    // REMEMBER: y is a number from 0 to rows_, 0 means all blocks are glowing, rows_ means none are
    canvas_painter.drawPixmap(static_cast<int>(x) * (kWidth + 1), static_cast<int>(y) * (kHeight + 1) + y_, *bar(), 0, static_cast<int>(y) * (kHeight + 1), bar()->width(), bar()->height());
  }

  for (int x = 0; x < store_.size(); ++x) {
    canvas_painter.drawPixmap(x * (kWidth + 1), static_cast<int>(store_[x]) * (kHeight + 1) + y_, topbarpixmap_);
  }

  p.drawPixmap(0, 0, canvas_);

}

static inline void adjustToLimits(const int b, int &f, int &amount) {

  // with a range of 0-255 and maximum adjustment of amount, maximise the difference between f and b

  if (b < f) {
    if (b > 255 - f) {
      amount -= f;
      f = 0;
    }
    else {
      amount -= (255 - f);
      f = 255;
    }
  }
  else {
    if (f > 255 - b) {
      amount -= f;
      f = 0;
    }
    else {
      amount -= (255 - f);
      f = 255;
    }
  }

}

/**
 * Clever contrast function
 *
 * It will try to adjust the foreground color such that it contrasts well with
 *the background
 * It won't modify the hue of fg unless absolutely necessary
 * @return the adjusted form of fg
 */
QColor ensureContrast(const QColor &bg, const QColor &fg, int amount = 150);
QColor ensureContrast(const QColor &bg, const QColor &fg, int amount) {

  class OutputOnExit {
   public:
    explicit OutputOnExit(const QColor &color) : c(color) {}
    ~OutputOnExit() {
      int h = 0, s = 0, v = 0;
      c.getHsv(&h, &s, &v);
    }

   private:
    const QColor &c;

    Q_DISABLE_COPY(OutputOnExit)
  };

  OutputOnExit allocateOnTheStack(fg);

  int bh = 0, bs = 0, bv = 0;
  int fh = 0, fs = 0, fv = 0;

  bg.getHsv(&bh, &bs, &bv);
  fg.getHsv(&fh, &fs, &fv);

  int dv = abs(bv - fv);

  // value is the best measure of contrast
  // if there is enough difference in value already, return fg unchanged
  if (dv > amount) return fg;

  int ds = abs(bs - fs);

  // saturation is good enough too. But not as good. TODO adapt this a little
  if (ds > amount) return fg;

  int dh = abs(bh - fh);

  if (dh > 120) {
    // a third of the colour wheel automatically guarantees contrast
    // but only if the values are high enough and saturations significant enough
    // to allow the colours to be visible and not be shades of grey or black

    // check the saturation for the two colours is sufficient that hue alone can
    // provide sufficient contrast
    if (ds > amount / 2 && (bs > 125 && fs > 125)) {
      return fg;
    }
    if (dv > amount / 2 && (bv > 125 && fv > 125)) {
      return fg;
    }
  }

  if (fs < 50 && ds < 40) {
    // low saturation on a low saturation is sad
    const int tmp = 50 - fs;
    fs = 50;
    if (amount > tmp) {
      amount -= tmp;
    }
    else {
      amount = 0;
    }
  }

  // test that there is available value to honor our contrast requirement
  if (255 - dv < amount) {
    // we have to modify the value and saturation of fg
    // adjustToLimits( bv, fv, amount );
    // see if we need to adjust the saturation
    if (amount > 0) adjustToLimits(bs, fs, amount);

    // see if we need to adjust the hue
    if (amount > 0)
      fh += amount;  // cycles around;

    return QColor::fromHsv(fh, fs, fv);
  }

  if (fv > bv && bv > amount) {
    return QColor::fromHsv(fh, fs, bv - amount);
  }

  if (fv < bv && fv > amount) {
    return QColor::fromHsv(fh, fs, fv - amount);
  }

  if (fv > bv && (255 - fv > amount)) {
    return QColor::fromHsv(fh, fs, fv + amount);
  }

  if (fv < bv && (255 - bv > amount)) {
    return QColor::fromHsv(fh, fs, bv + amount);
  }

  return Qt::blue;

}

void BlockAnalyzer::paletteChange(const QPalette &_palette) {

  Q_UNUSED(_palette)

  const QColor bg = palette().color(QPalette::Window);
  const QColor fg = ensureContrast(bg, palette().color(QPalette::Highlight));

  topbarpixmap_.fill(fg);

  const double dr = 15 * static_cast<double>(bg.red() - fg.red()) / (rows_ * 16);
  const double dg = 15 * static_cast<double>(bg.green() - fg.green()) / (rows_ * 16);
  const double db = 15 * static_cast<double>(bg.blue() - fg.blue()) / (rows_ * 16);
  const int r = fg.red(), g = fg.green(), b = fg.blue();

  bar()->fill(bg);

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

  const QColor bg = palette().color(QPalette::Window);
  const QColor bgdark = bg.darker(112);

  background_.fill(bg);

  QPainter p(&background_);

  if (!p.paintEngine()) return;

  for (int x = 0; x < columns_; ++x) {
    for (int y = 0; y < rows_; ++y) {
      p.fillRect(x * (kWidth + 1), y * (kHeight + 1) + y_, kWidth, kHeight, bgdark);
    }
  }

}
