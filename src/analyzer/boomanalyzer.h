/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2004, Max Howell <max.howell@methylblue.com>
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

#ifndef BOOMANALYZER_H
#define BOOMANALYZER_H

#include "analyzerbase.h"

#include <QtGlobal>
#include <QObject>
#include <QPixmap>
#include <QPainter>
#include <QColor>

class QWidget;
class QResizeEvent;

class BoomAnalyzer : public AnalyzerBase {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit BoomAnalyzer(QWidget*);

  static const char *kName;

  void transform(Scope &s) override;
  void analyze(QPainter &p, const Scope &scope, const bool new_frame) override;

 public Q_SLOTS:
  void changeK_barHeight(int);
  void changeF_peakSpeed(int);

 protected:
  void resizeEvent(QResizeEvent *e) override;

  static const int kColumnWidth;
  static const int kMaxBandCount;
  static const int kMinBandCount;

  int bands_;
  Scope scope_;
  QColor fg_;

  double K_barHeight_, F_peakSpeed_, F_;

  std::vector<double> bar_height_;
  std::vector<double> peak_height_;
  std::vector<double> peak_speed_;

  QPixmap barPixmap_;
  QPixmap canvas_;

};

#endif  // BOOMANALYZER_H
