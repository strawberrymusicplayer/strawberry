/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2018, Vikram Ambrose <ambroseworks@gmail.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <chrono>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QVariant>
#include <QString>
#include <QIcon>
#include <QPainter>
#include <QStylePainter>
#include <QColor>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QPixmap>
#include <QPixmapCache>
#include <QLayout>
#include <QContextMenuEvent>
#include <QPaintEvent>

#include "fancytabwidget.h"
#include "fancytabbar.h"
#include "fancytabdata.h"
#include "utilities/colorutils.h"
#include "core/stylehelper.h"
#include "core/settings.h"
#include "constants/appearancesettings.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int IconSize_LargeSidebar = 40;
constexpr int IconSize_SmallSidebar = 32;
} // namespace

FancyTabWidget::FancyTabWidget(QWidget *parent)
    : QTabWidget(parent),
      menu_(nullptr),
      mode_(Mode::None),
      bottom_widget_(nullptr),
      bg_color_system_(true),
      bg_gradient_(true),
      iconsize_smallsidebar_(IconSize_SmallSidebar),
      iconsize_largesidebar_(IconSize_LargeSidebar) {

  FancyTabBar *tabBar = new FancyTabBar(this);
  setTabBar(tabBar);
  setTabPosition(QTabWidget::West);
  setMovable(true);
  setElideMode(Qt::ElideNone);
  setUsesScrollButtons(true);

  QObject::connect(tabBar, &FancyTabBar::currentChanged, this, &FancyTabWidget::CurrentTabChangedSlot);

}

FancyTabWidget::~FancyTabWidget() = default;

void FancyTabWidget::AddTab(QWidget *widget_view, const QString &name, const QIcon &icon, const QString &label) {

  FancyTabData *tab = new FancyTabData(widget_view, name, icon, label, static_cast<int>(tabs_.count()), this);
  tabs_.insert(widget_view, tab);

}

void FancyTabWidget::LoadSettings(const QString &settings_group) {

  Settings s;
  s.beginGroup(settings_group);
  QMultiMap <int, FancyTabData*> tabs;
  for (FancyTabData *tab : std::as_const(tabs_)) {
    int idx = s.value(u"tab_"_s + tab->name(), tab->index()).toInt();
    while (tabs.contains(idx)) { ++idx; }
    tabs.insert(idx, tab);
  }
  s.endGroup();

  for (QMultiMap <int, FancyTabData*> ::iterator it = tabs.begin(); it != tabs.end(); ++it) {
    (void)InsertTab(it.key(), it.value());
  }

}

void FancyTabWidget::SaveSettings(const QString &settings_group) {

  Settings s;
  s.beginGroup(settings_group);

  s.setValue("tab_mode", static_cast<int>(mode_));
  s.setValue("current_tab", currentIndex());

  for (FancyTabData *tab : std::as_const(tabs_)) {
    QString k = u"tab_"_s + tab->name();
    int idx = QTabWidget::indexOf(tab->page());
    if (idx < 0) {
      if (s.contains(k)) s.remove(k);
    }
    else {
      s.setValue(k, idx);
    }
  }

  s.endGroup();

}

void FancyTabWidget::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  bg_color_system_ = s.value(AppearanceSettings::kTabBarSystemColor, false).toBool();
  bg_gradient_ = s.value(AppearanceSettings::kTabBarGradient, true).toBool();
  bg_color_ = DefaultTabbarBgColor();
  if (!bg_color_system_) {
    bg_color_ = s.value(AppearanceSettings::kTabBarColor, bg_color_).value<QColor>();
  }
  iconsize_smallsidebar_ = s.value(AppearanceSettings::kIconSizeTabbarSmallMode, IconSize_SmallSidebar).toInt();
  iconsize_largesidebar_ = s.value(AppearanceSettings::kIconSizeTabbarLargeMode, IconSize_LargeSidebar).toInt();
  s.endGroup();

#ifndef Q_OS_MACOS
  if (mode() == Mode::LargeSidebar) {
    setIconSize(QSize(iconsize_largesidebar_, iconsize_largesidebar_));
  }
  else {
    setIconSize(QSize(iconsize_smallsidebar_, iconsize_smallsidebar_));
  }
#endif

  update();
  TabBarUpdateGeometry();

}

void FancyTabWidget::SetMode(const Mode mode) {

  const Mode previous_mode = mode_;

  mode_ = mode;

  if (mode == Mode::Tabs || mode == Mode::IconOnlyTabs) {
    setTabPosition(QTabWidget::North);
  }
  else {
    setTabPosition(QTabWidget::West);
  }

#ifndef Q_OS_MACOS
  if (mode_ == Mode::LargeSidebar || mode_ == Mode::IconsSidebar) {
    setIconSize(QSize(iconsize_largesidebar_, iconsize_largesidebar_));
  }
  else {
    setIconSize(QSize(iconsize_smallsidebar_, iconsize_smallsidebar_));
  }
#endif

  if ((previous_mode == Mode::IconOnlyTabs || previous_mode == Mode::IconsSidebar) && (mode != Mode::IconOnlyTabs && mode != Mode::IconsSidebar)) {
    for (int i = 0; i < count(); ++i) {
      tabBar()->setTabText(i, tabBar()->tabData(i).value<FancyTabData*>()->label());
      tabBar()->setTabToolTip(i, ""_L1);
    }
  }
  else if ((previous_mode != Mode::IconOnlyTabs && previous_mode != Mode::IconsSidebar) && (mode == Mode::IconOnlyTabs || mode == Mode::IconsSidebar)) {
    for (int i = 0; i < count(); ++i) {
      tabBar()->setTabText(i, ""_L1);
      tabBar()->setTabToolTip(i, tabBar()->tabData(i).value<FancyTabData*>()->label());
    }
  }

  tabBar()->updateGeometry();
  updateGeometry();

  // There appears to be a bug in QTabBar which causes tabSizeHint to be ignored thus the need for this second shot repaint
  QTimer::singleShot(1ms, this, &FancyTabWidget::TabBarUpdateGeometry);

  Q_EMIT ModeChanged(mode);

}

int FancyTabWidget::InsertTab(const int preffered_index, FancyTabData *tab) {

  const int actual_index = InsertTab(preffered_index, tab->page(), tab->icon(), QString());
  tabBar()->setTabData(actual_index, QVariant::fromValue<FancyTabData*>(tab));

  if (mode_ == Mode::IconOnlyTabs || mode_ == Mode::IconsSidebar) {
    tabBar()->setTabText(actual_index, ""_L1);
    tabBar()->setTabToolTip(actual_index, tab->label());
  }
  else {
    tabBar()->setTabText(actual_index, tab->label());
    tabBar()->setTabToolTip(actual_index, ""_L1);
  }

  return actual_index;

}

int FancyTabWidget::InsertTab(const int idx, QWidget *page, const QIcon &icon, const QString &label) {
  return QTabWidget::insertTab(idx, page, icon, label);
}

bool FancyTabWidget::EnableTab(QWidget *widget_view) {

  if (!tabs_.contains(widget_view)) return false;
  FancyTabData *tab = tabs_.value(widget_view);

  if (QTabWidget::indexOf(tab->page()) >= 0) return true;

  (void)InsertTab(count(), tab);

  return true;

}

bool FancyTabWidget::DisableTab(QWidget *widget_view) {

  if (!tabs_.contains(widget_view)) return false;
  FancyTabData *tab = tabs_.value(widget_view);

  int idx = QTabWidget::indexOf(tab->page());
  if (idx < 0) return false;

  removeTab(idx);

  return true;

}

void FancyTabWidget::AddSpacer() {

  QWidget *spacer = new QWidget(this);
  const int idx = insertTab(count(), spacer, QIcon(), QString());
  setTabEnabled(idx, false);

}

void FancyTabWidget::AddBottomWidget(QWidget *widget_view) {
  bottom_widget_ = widget_view;
}

void FancyTabWidget::SetBackgroundPixmap(const QPixmap &pixmap) {

  background_pixmap_ = pixmap;
  update();

}

void FancyTabWidget::SetCurrentIndex(int idx) {

  Q_ASSERT(count() > 0);

  if (idx >= count() || idx < 0) idx = 0;

  QWidget *currentPage = widget(idx);
  QLayout *layout = currentPage->layout();
  if (bottom_widget_) layout->addWidget(bottom_widget_);
  QTabWidget::setCurrentIndex(idx);

}

void FancyTabWidget::CurrentTabChangedSlot(const int idx) {

  QWidget *currentPage = currentWidget();
  QLayout *layout = currentPage->layout();
  if (bottom_widget_) layout->addWidget(bottom_widget_);

  Q_EMIT CurrentTabChanged(idx);

}

int FancyTabWidget::IndexOfTab(QWidget *widget) {

  if (!tabs_.contains(widget)) return -1;
  QWidget *page = tabs_.value(widget)->page();
  return QTabWidget::indexOf(page);

}

void FancyTabWidget::paintEvent(QPaintEvent *pe) {

  if (mode() != Mode::LargeSidebar && mode() != Mode::SmallSidebar && mode() != Mode::IconsSidebar) {
    QTabWidget::paintEvent(pe);
    return;
  }

  QStylePainter painter(this);
  QRect backgroundRect = rect();
  backgroundRect.setWidth(tabBar()->width());

  QString key = QString::asprintf("mh_vertical %d %d %d %d %d", backgroundRect.width(), backgroundRect.height(), bg_color_.rgb(), (bg_gradient_ ? 1 : 0), (background_pixmap_.isNull() ? 0 : 1));

  QPixmap pixmap;
  if (!QPixmapCache::find(key, &pixmap)) {

    pixmap = QPixmap(backgroundRect.size());
    QPainter p(&pixmap);
    p.fillRect(backgroundRect, bg_color_);

    // Draw the gradient fill.
    if (bg_gradient_) {

      QRect rect(0, 0, backgroundRect.width(), backgroundRect.height());

      QColor shadow = StyleHelper::shadowColor(false);
      QLinearGradient grad(backgroundRect.topRight(), backgroundRect.topLeft());
      grad.setColorAt(0, bg_color_.lighter(117));
      grad.setColorAt(1, shadow.darker(109));
      p.fillRect(rect, grad);

      QColor light(255, 255, 255, 80);
      p.setPen(light);
      p.drawLine(rect.topRight() - QPoint(1, 0), rect.bottomRight() - QPoint(1, 0));
      QColor dark(0, 0, 0, 90);
      p.setPen(dark);
      p.drawLine(rect.topLeft(), rect.bottomLeft());

    }

    // Draw the translucent png graphics over the gradient fill
    if (!background_pixmap_.isNull()) {
      QRect pixmap_rect(background_pixmap_.rect());
      pixmap_rect.moveTo(backgroundRect.topLeft());

      while (pixmap_rect.top() < backgroundRect.bottom()) {
        QRect source_rect(pixmap_rect.intersected(backgroundRect));
        source_rect.moveTo(0, 0);
        p.drawPixmap(pixmap_rect.topLeft(), background_pixmap_, source_rect);
        pixmap_rect.moveTop(pixmap_rect.bottom() - 10);
      }
    }

    // Shadow effect of the background
    QColor light(255, 255, 255, 80);
    p.setPen(light);
    p.drawLine(backgroundRect.topRight() - QPoint(1, 0), backgroundRect.bottomRight() - QPoint(1, 0));
    QColor dark(0, 0, 0, 90);
    p.setPen(dark);
    p.drawLine(backgroundRect.topLeft(), backgroundRect.bottomLeft());

    p.setPen(StyleHelper::borderColor());
    p.drawLine(backgroundRect.topRight(), backgroundRect.bottomRight());

    p.end();

    QPixmapCache::insert(key, pixmap);

  }

  painter.drawPixmap(backgroundRect.topLeft(), pixmap);

}

void FancyTabWidget::TabBarUpdateGeometry() {
  tabBar()->updateGeometry();
}

void FancyTabWidget::addMenuItem(QActionGroup *group, const QString &text, Mode mode) {

  QAction *action = group->addAction(text);
  action->setCheckable(true);
  QObject::connect(action, &QAction::triggered, this, [this, mode]() { SetMode(mode); });

  if (mode == mode_) action->setChecked(true);

}

void FancyTabWidget::contextMenuEvent(QContextMenuEvent *e) {

  if (!QRect(mapToGlobal(pos()), tabBar()->size()).contains(e->globalPos())) {
    QTabWidget::contextMenuEvent(e);
    return;
  }

  if (!menu_) {
    menu_ = new QMenu(this);
    QActionGroup *group = new QActionGroup(this);
    addMenuItem(group, tr("Large sidebar"), Mode::LargeSidebar);
    addMenuItem(group, tr("Icons sidebar"), Mode::IconsSidebar);
    addMenuItem(group, tr("Small sidebar"), Mode::SmallSidebar);
    addMenuItem(group, tr("Plain sidebar"), Mode::PlainSidebar);
    addMenuItem(group, tr("Tabs on top"), Mode::Tabs);
    addMenuItem(group, tr("Icons on top"), Mode::IconOnlyTabs);
    menu_->addActions(group->actions());
  }

  menu_->popup(e->globalPos());

}

QColor FancyTabWidget::DefaultTabbarBgColor() {

  QColor color = StyleHelper::highlightColor();
  if (Utilities::IsColorDark(color)) {
    color = color.lighter(130);
  }
  return color;

}
