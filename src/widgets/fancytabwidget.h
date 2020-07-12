/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2018, Vikram Ambrose <ambroseworks@gmail.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FANCYTABWIDGET_H
#define FANCYTABWIDGET_H

#include "config.h"

#include <QObject>
#include <QTabWidget>
#include <QMap>
#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QColor>

class QMenu;
class QActionGroup;
class QContextMenuEvent;
class QPaintEvent;
class TabData;

namespace Core {
namespace Internal {

class FancyTabWidget : public QTabWidget {
  Q_OBJECT

 public:
  explicit FancyTabWidget(QWidget *parent = nullptr);

  void AddTab(QWidget *widget_view, const QString &name, const QIcon &icon, const QString &label);
   bool EnableTab(QWidget *widget_view);
   bool DisableTab(QWidget *widget_view);
   int insertTab(const int idx, QWidget *page, const QIcon &icon, const QString &label);
   void addBottomWidget(QWidget* widget_view);

   void setBackgroundPixmap(const QPixmap& pixmap);
   void addSpacer();

   void Load(const QString &kSettingsGroup);
   void SaveSettings(const QString &kSettingsGroup);
   void ReloadSettings();

   // Values are persisted - only add to the end
  enum Mode {
    Mode_None = 0,
    Mode_LargeSidebar,
    Mode_SmallSidebar,
    Mode_Tabs,
    Mode_IconOnlyTabs,
    Mode_PlainSidebar,
   };

   static const int TabSize_LargeSidebarWidth;
   static const int IconSize_LargeSidebar;
   static const int IconSize_SmallSidebar;
   static const int IconSize_PlainSidebar;
   static const int IconSize_TabsSidebar;
   static const int IconSize_IconsSidebar;

   Mode mode() const { return mode_; }
   int iconsize_smallsidebar() const { return iconsize_smallsidebar_; }
   int iconsize_largesidebar() const { return iconsize_largesidebar_; }

  signals:
   void ModeChanged(FancyTabWidget::Mode mode);
   void CurrentChanged(int);

  public slots:
   void setCurrentIndex(int idx);
   void SetMode(Mode mode);
   // Mapper mapped signal needs this convenience function
   void SetMode(int mode) { SetMode(Mode(mode)); }

  private slots:
   void tabBarUpdateGeometry();
   void currentTabChanged(int);

  protected:
   void paintEvent(QPaintEvent*) override;
   void contextMenuEvent(QContextMenuEvent* e) override;

  private:
   void addMenuItem(QActionGroup* group, const QString& text, Mode mode);

   QPixmap background_pixmap_;
   QMenu* menu_;
   Mode mode_;
   QWidget *bottom_widget_;

   QMap <QWidget*, TabData*> tabs_;

   bool bg_color_system_;
   bool bg_gradient_;
   QColor bg_color_;
   int iconsize_smallsidebar_;
   int iconsize_largesidebar_;

};

}  // namespace Internal
}  // namespace Core

using Core::Internal::FancyTabWidget;

#endif  // FANCYTABWIDGET_H
