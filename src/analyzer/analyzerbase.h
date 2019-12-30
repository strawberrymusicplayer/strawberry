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

#ifndef ANALYZERBASE_H
#define ANALYZERBASE_H

#include "config.h"

#ifdef __FreeBSD__
#  include <sys/types.h>
#endif

#include <vector>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QBasicTimer>
#include <QString>
#include <QPainter>
#include <QtEvents>

#include "analyzer/fht.h"
#include "engine/engine_fwd.h"
#include "engine/enginebase.h"

class QHideEvent;
class QShowEvent;
class QTimerEvent;
class QPaintEvent;

namespace Analyzer {

typedef std::vector<float> Scope;

class Base : public QWidget {
  Q_OBJECT

 public:
  ~Base() { delete fht_; }

  uint timeout() const { return timeout_; }

  void set_engine(EngineBase *engine) { engine_ = engine; }

  void changeTimeout(uint newTimeout) {
    timeout_ = newTimeout;
    if (timer_.isActive()) {
      timer_.stop();
      timer_.start(timeout_, this);
    }
  }

  virtual void framerateChanged() {}

 protected:
  explicit Base(QWidget*, uint scopeSize = 7);

  void hideEvent(QHideEvent*);
  void showEvent(QShowEvent*);
  void paintEvent(QPaintEvent*);
  void timerEvent(QTimerEvent*);

  void polishEvent();

  int resizeExponent(int);
  int resizeForBands(int);
  virtual void init() {}
  virtual void transform(Scope&);
  virtual void analyze(QPainter& p, const Scope&, bool new_frame) = 0;
  virtual void demo(QPainter& p);

 protected:
  QBasicTimer timer_;
  uint timeout_;
  FHT *fht_;
  EngineBase *engine_;
  Scope lastscope_;

  bool new_frame_;
  bool is_playing_;
};

void interpolate(const Scope&, Scope&);
void initSin(Scope&, const uint = 6000);

}  //  namespace Analyzer

#endif  // ANALYZERBASE_H

