/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

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

#ifndef ANALYZERCONTAINER_H
#define ANALYZERCONTAINER_H

#include "config.h"

#include <QWidget>
#include <QList>
#include <QString>
#include <QPoint>
#include <QMenu>
#include <QAction>
#include <QActionGroup>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"

class QTimer;
class QMouseEvent;
class QWheelEvent;

class AnalyzerBase;

class AnalyzerContainer : public QWidget {
  Q_OBJECT

 public:
  explicit AnalyzerContainer(QWidget *parent);

  void SetEngine(SharedPtr<EngineBase> engine);

  static const char *kSettingsGroup;
  static const char *kSettingsFramerate;

 Q_SIGNALS:
  void WheelEvent(const int delta);

 protected:
  void mouseReleaseEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;

 private Q_SLOTS:
  void ChangeAnalyzer(const int id);
  void ChangeFramerate(int new_framerate);
  void DisableAnalyzer();
  void ShowPopupMenu();

 private:
  void Load();
  void Save();
  void SaveFramerate(const int framerate);
  template<typename T>
  void AddAnalyzerType();
  void AddFramerate(const QString &name, const int framerate);

 private:
  int current_framerate_;  // fps
  QMenu *context_menu_;
  QMenu *context_menu_framerate_;
  QActionGroup *group_;
  QActionGroup *group_framerate_;

  QList<const QMetaObject*> analyzer_types_;
  QList<int> framerate_list_;
  QList<QAction*> actions_;
  QAction *disable_action_;

  QTimer *double_click_timer_;
  QPoint last_click_pos_;
  bool ignore_next_click_;

  AnalyzerBase *current_analyzer_;
  SharedPtr<EngineBase> engine_;
};

template<typename T>
void AnalyzerContainer::AddAnalyzerType() {

  int id = analyzer_types_.count();
  analyzer_types_ << &T::staticMetaObject;

  QAction *action = context_menu_->addAction(tr(T::kName));
  group_->addAction(action);
  action->setCheckable(true);
  actions_ << action;
  QObject::connect(action, &QAction::triggered, [this, id]() { ChangeAnalyzer(id); } );

}

#endif  // ANALYZERCONTAINER_H
