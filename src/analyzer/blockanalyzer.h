// Maintainer: Max Howell <mac.howell@methylblue.com>, (C) 2003-5
// Copyright:  See COPYING file that comes with this distribution
//

#ifndef BLOCKANALYZER_H
#define BLOCKANALYZER_H

#include <stdbool.h>
#include <vector>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QtEvents>

#include "analyzerbase.h"

class QResizeEvent;

/**
 * @author Max Howell
 */

class BlockAnalyzer : public Analyzer::Base {
  Q_OBJECT
 public:
  Q_INVOKABLE BlockAnalyzer(QWidget*);
  ~BlockAnalyzer();

  static const uint HEIGHT;
  static const uint WIDTH;
  static const uint MIN_ROWS;
  static const uint MIN_COLUMNS;
  static const uint MAX_COLUMNS;
  static const uint FADE_SIZE;

  static const char *kName;

 protected:
  virtual void transform(Analyzer::Scope&);
  virtual void analyze(QPainter &p, const Analyzer::Scope&, bool new_frame);
  virtual void resizeEvent(QResizeEvent*);
  virtual void paletteChange(const QPalette&);
  virtual void framerateChanged();

  void drawBackground();
  void determineStep();

 private:
  QPixmap *bar() { return &barpixmap_; }

  uint columns_, rows_;   // number of rows and columns of blocks
  uint y_;                // y-offset from top of widget
  QPixmap barpixmap_;
  QPixmap topbarpixmap_;
  QPixmap background_;
  QPixmap canvas_;
  Analyzer::Scope scope_;     // so we don't create a vector every frame
  std::vector<float> store_;  // current bar heights
  std::vector<float> yscale_;

  // FIXME why can't I namespace these? c++ issue?
  std::vector<QPixmap> fade_bars_;
  std::vector<uint> fade_pos_;
  std::vector<int> fade_intensity_;

  float step_;  // rows to fall per frame
};

#endif
