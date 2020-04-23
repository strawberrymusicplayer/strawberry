/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2011, Tyler Rhodes <tyler.s.rhodes@gmail.com>
   Copyright 2011-2012, 2014, David Sansome <me@davidsansome.com>
   Copyright 2011, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2014, Alibek Omarov <a1ba.omarov@gmail.com>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
   Copyright 2014-2015, Mark Furneaux <mark@furneaux.ca>
   Copyright 2015, Arun Narayanankutty <n.arun.lifescience@gmail.com>

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

#ifndef RAINBOWANALYZER_H
#define RAINBOWANALYZER_H

#include "analyzerbase.h"

#include <QObject>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRect>

class QWidget;
class QTimerEvent;
class QResizeEvent;

namespace Rainbow {
class RainbowAnalyzer : public Analyzer::Base {
  Q_OBJECT

 public:
  enum RainbowType {
    Nyancat = 0,
    Dash = 1 
  };

  RainbowAnalyzer(const RainbowType& rbtype, QWidget* parent);

 protected:
  void transform(Analyzer::Scope&);
  void analyze(QPainter& p, const Analyzer::Scope&, bool new_frame);

  void timerEvent(QTimerEvent* e);
  void resizeEvent(QResizeEvent* e);

 private:
  static const int kHeight[];
  static const int kWidth[];
  static const int kFrameCount[];
  static const int kRainbowHeight[];
  static const int kRainbowOverlap[];
  static const int kSleepingHeight[];

  static const int kHistorySize = 128;
  static const int kRainbowBands = 6;
  static const float kPixelScale;

  static const int kFrameIntervalMs = 150;

  static RainbowType rainbowtype;

  inline QRect SourceRect(RainbowType _rainbowtype) const {
    return QRect(0, kHeight[_rainbowtype] * frame_, kWidth[_rainbowtype], kHeight[_rainbowtype]);
  }

  inline QRect SleepingSourceRect(RainbowType _rainbowtype) const {
    return QRect(0, kHeight[_rainbowtype] * kFrameCount[_rainbowtype], kWidth[_rainbowtype], kSleepingHeight[_rainbowtype]);
  }

  inline QRect DestRect(RainbowType _rainbowtype) const {
    return QRect(width() - kWidth[_rainbowtype], (height() - kHeight[_rainbowtype]) / 2, kWidth[_rainbowtype], kHeight[_rainbowtype]);
  }

  inline QRect SleepingDestRect(RainbowType _rainbowtype) const {
    return QRect(width() - kWidth[_rainbowtype], (height() - kSleepingHeight[_rainbowtype]) / 2, kWidth[_rainbowtype], kSleepingHeight[_rainbowtype]);
  }

 private:
  // "constants" that get initialised in the constructor
  float band_scale_[kRainbowBands];
  QPen colors_[kRainbowBands];

  // Rainbow Nyancat & Dash
  QPixmap cat_dash_[2];

  // For the cat or dash animation
  int timer_id_;
  int frame_;

  // The y positions of each point on the rainbow.
  float history_[kHistorySize * kRainbowBands];

  // A cache of the last frame's rainbow, 
  // so it can be used in the next frame.
  QPixmap buffer_[2];
  int current_buffer_;

  // Geometry information that's updated on resize:
  // The width of the widget minus the space for the cat
  int available_rainbow_width_;

  // X spacing between each point in the polyline.
  int px_per_frame_;

  // Amount the buffer_ is shifted to the left (off the edge of the widget) 
  // to make the rainbow extend from 0 to available_rainbow_width_.
  int x_offset_;

  QBrush background_brush_;
};

class NyanCatAnalyzer : public RainbowAnalyzer {
  Q_OBJECT

 public:
  Q_INVOKABLE NyanCatAnalyzer(QWidget* parent);
  
  static const char* kName;
};

class RainbowDashAnalyzer : public RainbowAnalyzer {
  Q_OBJECT

 public:
  Q_INVOKABLE RainbowDashAnalyzer(QWidget* parent);

  static const char* kName;
};
}

#endif  // RAINBOWANALYZER_H
