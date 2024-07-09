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


#include <QPixmap>
#include <QPainter>

#include "analyzerbase.h"

class WaveRubberAnalyzer : public AnalyzerBase {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit WaveRubberAnalyzer(QWidget *parent);

  static const char *kName;

 protected:
  void resizeEvent(QResizeEvent *e) override;
  void analyze(QPainter &p, const Scope &s, const bool new_frame) override;
  void transform(Scope &scope) override;
  void demo(QPainter &p) override;

 private:
  QPixmap canvas_;
};
