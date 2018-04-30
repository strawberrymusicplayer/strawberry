#include "config.h"

#include <QWidget>
#include <QGLWidget>
#include <QVector>

#include "analyzer.h"

#include "engine/enginebase.h"

AnalyzerBase::AnalyzerBase(QWidget *parent)
    : QGLWidget(parent), engine_(nullptr) {}

void AnalyzerBase::set_engine(Engine::Base *engine) {
  disconnect(engine_);
  engine_ = engine;
  if (engine_) {
    connect(engine_, SIGNAL(SpectrumAvailable(const QVector<float>&)), SLOT(SpectrumAvailable(const QVector<float>&)));
  }
}
