// Maintainer: Max Howell <max.howell@methylblue.com>, (C) 2004
// Copyright:  See COPYING file that comes with this distribution

#ifndef ANALYZERBASE_H
#define ANALYZERBASE_H

#include "config.h"

#ifdef __FreeBSD__
#include <sys/types.h>
#endif

#include <stdbool.h>
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
  Base(QWidget*, uint scopeSize = 7);

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
  int current_chunk_;
  bool new_frame_;
  bool is_playing_;
};

void interpolate(const Scope&, Scope&);
void initSin(Scope&, const uint = 6000);

}  // END namespace Analyzer

#endif
