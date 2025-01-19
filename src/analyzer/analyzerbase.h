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

#include <QWidget>
#include <QBasicTimer>
#include <QString>
#include <QPainter>

#include "includes/shared_ptr.h"
#include "analyzer/fht.h"
#include "engine/enginebase.h"

class QHideEvent;
class QShowEvent;
class QPaintEvent;
class QTimerEvent;

class AnalyzerBase : public QWidget {
  Q_OBJECT

 public:
  ~AnalyzerBase() override;

  int timeout() const { return timeout_; }

  void set_engine(SharedPtr<EngineBase> engine) { engine_ = engine; }

  void ChangeTimeout(const int timeout);

  virtual void framerateChanged() {}

 protected:
  using Scope = std::vector<float>;
  explicit AnalyzerBase(QWidget *parent, const uint scope_size = 7);

  void hideEvent(QHideEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

  int resizeExponent(int exp);
  int resizeForBands(const int bands);
  virtual void init() {}
  virtual void transform(Scope &scope);
  virtual void analyze(QPainter &p, const Scope &s, const bool new_frame) = 0;
  virtual void demo(QPainter &p);

  void interpolate(const Scope &in_scope, Scope &out_scope);
  void initSin(Scope &v, const uint size = 6000);

 protected:
  QBasicTimer timer_;
  FHT *fht_;
  SharedPtr<EngineBase> engine_;
  Scope lastscope_;

  bool new_frame_;
  bool is_playing_;
  int timeout_;
};

#endif  // ANALYZERBASE_H

