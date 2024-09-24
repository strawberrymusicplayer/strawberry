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
#include <QList>
#include <QString>
#include <QPixmap>
#include <QPainter>
#include <QPalette>

#include "analyzerbase.h"

class QWidget;
class QResizeEvent;

class BlockAnalyzer : public AnalyzerBase {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit BlockAnalyzer(QWidget*);

  static const char *kName;

 protected:
  void transform(Scope&) override;
  void analyze(QPainter &p, const Scope &s, const bool new_frame) override;
  void resizeEvent(QResizeEvent*) override;
  virtual void paletteChange(const QPalette &_palette);
  void framerateChanged() override;

  void drawBackground();
  void determineStep();

 private:
  QPixmap *bar() { return &barpixmap_; }

  int columns_, rows_;  // number of rows and columns of blocks
  int y_;               // y-offset from top of widget
  QPixmap barpixmap_;
  QPixmap topbarpixmap_;
  QPixmap background_;
  QPixmap canvas_;
  Scope scope_;  // so we don't create a vector every frame
  QList<double> store_;  // current bar heights
  QList<double> yscale_;

  QList<QPixmap> fade_bars_;
  QList<int> fade_pos_;
  QList<int> fade_intensity_;

  double step_;  // rows to fall per frame
};

#endif  // BLOCKANALYZER_H
