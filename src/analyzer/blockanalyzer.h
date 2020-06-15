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

#ifndef BLOCKANALYZER_H
#define BLOCKANALYZER_H

#include <QtGlobal>
#include <QObject>
#include <QVector>
#include <QString>
#include <QPixmap>
#include <QPainter>
#include <QPalette>

#include "analyzerbase.h"

class QWidget;
class QResizeEvent;

class BlockAnalyzer : public Analyzer::Base {
  Q_OBJECT

 public:
  Q_INVOKABLE BlockAnalyzer(QWidget*);

  static const uint kHeight;
  static const uint kWidth;
  static const uint kMinRows;
  static const uint kMinColumns;
  static const uint kMaxColumns;
  static const uint kFadeSize;

  static const char *kName;

 protected:
  void transform(Analyzer::Scope&) override;
  void analyze(QPainter &p, const Analyzer::Scope&, bool new_frame) override;
  void resizeEvent(QResizeEvent*) override;
  virtual void paletteChange(const QPalette&);
  void framerateChanged() override;

  void drawBackground();
  void determineStep();

 private:
  QPixmap *bar() { return &barpixmap_; }

  uint columns_, rows_;      // number of rows and columns of blocks
  uint y_;                   // y-offset from top of widget
  QPixmap barpixmap_;
  QPixmap topbarpixmap_;
  QPixmap background_;
  QPixmap canvas_;
  Analyzer::Scope scope_;    // so we don't create a vector every frame
  QVector<float> store_;     // current bar heights
  QVector<float> yscale_;

  QVector<QPixmap> fade_bars_;
  QVector<uint> fade_pos_;
  QVector<int> fade_intensity_;

  float step_;  // rows to fall per frame
};

#endif  // BLOCKANALYZER_H
