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

#include "fancytabwidget.h"
#include "core/stylehelper.h"
#include "core/logging.h"

#include <QDebug>

#include <QObject>
#include <QTabBar>
#include <QWidget>
#include <QMap>
#include <QHash>
#include <QString>
#include <QIcon>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QFont>
#include <QStylePainter>
#include <QTimer>
#include <QLayout>
#include <QVBoxLayout>
#include <QSize>
#include <QPoint>
#include <QTransform>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QMouseEvent>

const QSize FancyTabWidget::IconSize_LargeSidebar = QSize(24, 24);
const QSize FancyTabWidget::IconSize_SmallSidebar = QSize(22, 22);
const QSize FancyTabWidget::TabSize_LargeSidebar = QSize(70, 47);

class FancyTabBar: public QTabBar {

 private:
  int mouseHoverTabIndex = -1;
  QMap<QWidget*, QString> labelCache;
  QMap<int, QWidget*> spacers;

 public:
  explicit FancyTabBar(QWidget* parent=0) : QTabBar(parent) {
    setMouseTracking(true);
  }

  QSize sizeHint() const {

    QSize size(QTabBar::sizeHint());

    FancyTabWidget *tabWidget = (FancyTabWidget*) parentWidget();
    if (tabWidget->mode() == FancyTabWidget::Mode_Tabs || tabWidget->mode() == FancyTabWidget::Mode_IconOnlyTabs) return size;

    QSize tabSize(tabSizeHint(0));
    size.setWidth(tabSize.width());
    int guessHeight = tabSize.height()*count();
    if (guessHeight > size.height())
      size.setHeight(guessHeight);
    return size;

  }

  int width() {
    return tabSizeHint(0).width();
  }

 protected:
  QSize tabSizeHint(int index) const {

    FancyTabWidget *tabWidget = (FancyTabWidget*) parentWidget();
    QSize size = FancyTabWidget::TabSize_LargeSidebar;

    if (tabWidget->mode() != FancyTabWidget::Mode_LargeSidebar) {
      size = QTabBar::tabSizeHint(index);
    }
    size.setWidth(std::max(size.width(), 37));

    return size;

  }

  void leaveEvent(QEvent *event) {
    mouseHoverTabIndex = -1;
    update();
  }

  void mouseMoveEvent(QMouseEvent *event) {

    QPoint pos = event->pos();

    mouseHoverTabIndex = tabAt(pos);
    if (mouseHoverTabIndex > -1)
      update();
    QTabBar::mouseMoveEvent(event);

  }

  void paintEvent(QPaintEvent *pe) {

    FancyTabWidget *tabWidget = (FancyTabWidget*) parentWidget();

    bool verticalTextTabs = false;

    if (tabWidget->mode() == FancyTabWidget::Mode_SmallSidebar)
      verticalTextTabs = true;

    // if LargeSidebar, restore spacers
    if (tabWidget->mode() == FancyTabWidget::Mode_LargeSidebar && spacers.count() > 0) {
      for (int index : spacers.keys()) {
        tabWidget->insertTab(index, spacers[index], QIcon(), QString());
        tabWidget->setTabEnabled(index, false);
      }
      spacers.clear();
    }
    else if (tabWidget->mode() != FancyTabWidget::Mode_LargeSidebar) {
      // traverse in the opposite order to save indices of spacers
      for (int i = count() - 1; i >= 0; --i) {
        // spacers are disabled tabs
        if (!isTabEnabled(i) && !spacers.contains(i)) {
          spacers[i] = tabWidget->widget(i);
          tabWidget->removeTab(i);
          --i;
        }
      }
    }

    // Restore any label text that was hidden/cached for the IconOnlyTabs mode
    if (labelCache.count() > 0 && tabWidget->mode() != FancyTabWidget::Mode_IconOnlyTabs) {
      for (int i =0; i < count(); i++) {
        setTabText(i, labelCache[tabWidget->widget(i)]);
      }
      labelCache.clear();
    }
    if (tabWidget->mode() != FancyTabWidget::Mode_LargeSidebar && tabWidget->mode() != FancyTabWidget::Mode_SmallSidebar) {
      // Cache and hide label text for IconOnlyTabs mode
      if (tabWidget->mode() == FancyTabWidget::Mode_IconOnlyTabs && labelCache.count() == 0) {
        for(int i =0; i < count(); i++) {
          labelCache[tabWidget->widget(i)] = tabText(i);
          setTabText(i, "");
        }
      }
      QTabBar::paintEvent(pe);
      return;
    }

    QStylePainter p(this);

    for (int index = 0; index < count(); index++) {
      const bool selected = tabWidget->currentIndex() == index;
      QRect tabrect = tabRect(index);
      QRect selectionRect = tabrect;
      if (selected) {
        // Selection highlight
        p.save();
        QLinearGradient grad(selectionRect.topLeft(), selectionRect.topRight());
        grad.setColorAt(0, QColor(255, 255, 255, 140));
        grad.setColorAt(1, QColor(255, 255, 255, 210));
        p.fillRect(selectionRect.adjusted(0,0,0,-1), grad);
        p.restore();

        // shadow lines
        p.setPen(QColor(0, 0, 0, 110));
        p.drawLine(selectionRect.topLeft()    + QPoint(1, -1), selectionRect.topRight()    - QPoint(0, 1));
        p.drawLine(selectionRect.bottomLeft(), selectionRect.bottomRight());
        p.setPen(QColor(0, 0, 0, 40));
        p.drawLine(selectionRect.topLeft(),    selectionRect.bottomLeft());

        // highlights
        p.setPen(QColor(255, 255, 255, 50));
        p.drawLine(selectionRect.topLeft()    + QPoint(0, -2), selectionRect.topRight()    - QPoint(0, 2));
        p.drawLine(selectionRect.bottomLeft() + QPoint(0, 1),  selectionRect.bottomRight() + QPoint(0, 1));
        p.setPen(QColor(255, 255, 255, 40));
        p.drawLine(selectionRect.topLeft()    + QPoint(0, 0),  selectionRect.topRight());
        p.drawLine(selectionRect.topRight()   + QPoint(0, 1),  selectionRect.bottomRight() - QPoint(0, 1));
        p.drawLine(selectionRect.bottomLeft() + QPoint(0, -1), selectionRect.bottomRight() - QPoint(0, 1));

      }

      // Mouse hover effect
      if (!selected && index == mouseHoverTabIndex && isTabEnabled(index)) {
        p.save();
        QLinearGradient grad(selectionRect.topLeft(),  selectionRect.topRight());
        grad.setColorAt(0, Qt::transparent);
        grad.setColorAt(0.5, QColor(255, 255, 255, 40));
        grad.setColorAt(1, Qt::transparent);
        p.fillRect(selectionRect, grad);
        p.setPen(QPen(grad, 1.0));
        p.drawLine(selectionRect.topLeft(),     selectionRect.topRight());
        p.drawLine(selectionRect.bottomRight(), selectionRect.bottomLeft());
        p.restore();
      }

      // Label (Icon and Text)
      {
        p.save();
        QTransform m;
        int textFlags;
        Qt::Alignment iconFlags;

        QRect tabrectText;
        QRect tabrectLabel;

        if (verticalTextTabs) {
          m = QTransform::fromTranslate(tabrect.left(), tabrect.bottom());
          m.rotate(-90);
          textFlags = Qt::AlignLeft | Qt::AlignVCenter ;
          iconFlags = Qt::AlignLeft  | Qt::AlignVCenter;

          tabrectLabel = QRect(QPoint(0, 0), m.mapRect(tabrect).size());

          tabrectText = tabrectLabel;
          tabrectText.translate(30,0);
        }
        else {
          m = QTransform::fromTranslate(tabrect.left(), tabrect.top());
          textFlags = Qt::AlignHCenter | Qt::AlignBottom ;
          iconFlags = Qt::AlignHCenter | Qt::AlignTop;

          tabrectLabel = QRect(QPoint(0, 0), m.mapRect(tabrect).size());

          tabrectText = tabrectLabel;
          tabrectText.translate(0,-5);
        }

        p.setTransform(m);

        QFont boldFont(p.font());
        boldFont.setPointSizeF(StyleHelper::sidebarFontSize());
        boldFont.setBold(true);
        p.setFont(boldFont);

        // Text drop shadow color
        p.setPen(selected ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 110) );
        p.translate(0, 3);
        p.drawText(tabrectText, textFlags, tabText(index));

        // Text foreground color
        p.translate(0, -1);
        p.setPen(selected ? QColor(60, 60, 60) : StyleHelper::panelTextColor());
        p.drawText(tabrectText, textFlags, tabText(index));


        // Draw the icon
        QRect tabrectIcon;
        const int PADDING = 5;
        if (verticalTextTabs) {
          tabrectIcon = tabrectLabel;
          tabrectIcon.setSize(FancyTabWidget::IconSize_SmallSidebar);
          tabrectIcon.translate(PADDING,PADDING);
        }
        else {
          tabrectIcon = tabrectLabel;
          tabrectIcon.setSize(FancyTabWidget::IconSize_LargeSidebar);
          // Center the icon
          const int moveRight = (FancyTabWidget::TabSize_LargeSidebar.width() -  FancyTabWidget::IconSize_LargeSidebar.width() -1)/2;
          tabrectIcon.translate(moveRight,PADDING);
        }
        tabIcon(index).paint(&p, tabrectIcon, iconFlags);
        p.restore();
      }
    }
  }

};

class TabData : public QObject {

 public:
  TabData(QWidget *widget_view, const QString name, const QIcon icon, const QString label, const int idx, QWidget *parent) :
  QObject(parent),
  widget_view_(widget_view),
  name_(name), icon_(icon),
  label_(label),
  index_(idx),
  page_(new QWidget()) {
    // In order to achieve the same effect as the "Bottom Widget" of the old Nokia based FancyTabWidget a VBoxLayout is used on each page
    QVBoxLayout *layout = new QVBoxLayout(page_);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget_view_);
    page_->setLayout(layout);
  }
  ~TabData() {
    //delete page_;
  }

  QWidget *widget_view() { return widget_view_; }
  QString name() { return name_; }
  QIcon icon() { return icon_; }
  QString label() { return label_; }
  QWidget *page() { return page_; }
  int index() { return index_; }

 private:
  QWidget *widget_view_;
  QString name_;
  QIcon icon_;
  QString label_;
  int index_;
  QWidget *page_;

};

FancyTabWidget::~FancyTabWidget() {}

// Spacers are just disabled pages
void FancyTabWidget::addSpacer() {

  QWidget *spacer = new QWidget(this);
  const int idx = insertTab(count(), spacer, QIcon(), QString());
  setTabEnabled(idx, false);

}

void FancyTabWidget::setBackgroundPixmap(const QPixmap& pixmap) {

  background_pixmap_ = pixmap;
  update();

}

void FancyTabWidget::setCurrentIndex(int idx) {

  Q_ASSERT(count() > 0);

  if (idx >= count() || idx < 0) idx = 0;

  QWidget *currentPage = widget(idx);
  QLayout *layout = currentPage->layout();
  if (bottom_widget_) layout->addWidget(bottom_widget_);
  QTabWidget::setCurrentIndex(idx);

}

void FancyTabWidget::currentTabChanged(int idx) {

  QWidget *currentPage = currentWidget();
  QLayout *layout = currentPage->layout();
  if (bottom_widget_) layout->addWidget(bottom_widget_);
  emit CurrentChanged(idx);

}

FancyTabWidget::FancyTabWidget(QWidget* parent) : QTabWidget(parent),
      menu_(nullptr),
      mode_(Mode_None),
      bottom_widget_(nullptr)
  {

  FancyTabBar *tabBar = new FancyTabBar(this);
  setTabBar(tabBar);
  setTabPosition(QTabWidget::West);
  setMovable(true);

  connect(tabBar, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

}

void FancyTabWidget::Load(const QString &kSettingsGroup) {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  QMultiMap <int, TabData*> tabs;
  for (TabData *tab : tabs_) {
    int idx = s.value("tab_" + tab->name(), tab->index()).toInt();
    while (tabs.contains(idx)) { ++idx; }
    tabs.insert(idx, tab);
  }
  s.endGroup();

  QMultiMap <int, TabData*> ::iterator i;
  for (i = tabs.begin() ; i != tabs.end() ; ++i) {
    TabData *tab = i.value();
    const int idx = insertTab(i.key(), tab->page(), tab->icon(), tab->label());
    tabBar()->setTabData(idx, QVariant(tab->name()));
  }

}

int FancyTabWidget::insertTab(int idx, QWidget *page, const QIcon &icon, const QString &label) {
  return QTabWidget::insertTab(idx, page, icon, label);
}

void FancyTabWidget::SaveSettings(const QString &kSettingsGroup) {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("tab_mode", mode_);
  s.setValue("current_tab", currentIndex());

  for (TabData *tab : tabs_) {
    QString k = "tab_" + tab->name();
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

void FancyTabWidget::addBottomWidget(QWidget *widget_view) {
  bottom_widget_ = widget_view;
}

void FancyTabWidget::AddTab(QWidget *widget_view, const QString &name, const QIcon &icon, const QString &label) {

  TabData *tab = new TabData(widget_view, name, icon, label, tabs_.count(), this);
  tabs_.insert(widget_view, tab);

}

bool FancyTabWidget::EnableTab(QWidget *widget_view) {

  if (!tabs_.contains(widget_view)) return false;
  TabData *tab = tabs_.value(widget_view);

  if (QTabWidget::indexOf(tab->page()) >= 0) return true;
  const int idx = QTabWidget::insertTab(count(), tab->page(), tab->icon(), tab->label());
  tabBar()->setTabData(idx, QVariant(tab->name()));

  return true;

}

bool FancyTabWidget::DisableTab(QWidget *widget_view) {

  if (!tabs_.contains(widget_view)) return false;
  TabData *tab = tabs_.value(widget_view);

  int idx = QTabWidget::indexOf(tab->page());
  if (idx < 0) return false;

  removeTab(idx);

  return true;

}

void FancyTabWidget::paintEvent(QPaintEvent *pe) {

  if (mode() != FancyTabWidget::Mode_LargeSidebar && mode() != FancyTabWidget::Mode_SmallSidebar) {
    QTabWidget::paintEvent(pe);
    return;
  }
  QStylePainter p(this);

  // The brown color (Ubuntu) you see on the background gradient
  QColor baseColor = StyleHelper::baseColor();

  QRect backgroundRect = rect();
  backgroundRect.setWidth(tabBar()->width());
  p.fillRect(backgroundRect, baseColor);

  // Horizontal gradient over the sidebar from transparent to dark
  StyleHelper::verticalGradient(&p, backgroundRect, backgroundRect, false);

  // Draw the translucent png graphics over the gradient fill
  {
    if (!background_pixmap_.isNull()) {
      QRect pixmap_rect(background_pixmap_.rect());
      pixmap_rect.moveTo(backgroundRect.topLeft());

      while (pixmap_rect.top() < backgroundRect.bottom()) {
        QRect source_rect(pixmap_rect.intersected(backgroundRect));
        source_rect.moveTo(0, 0);
        p.drawPixmap(pixmap_rect.topLeft(), background_pixmap_,source_rect);
        pixmap_rect.moveTop(pixmap_rect.bottom() - 10);
      }
    }
  }

  // Shadow effect of the background
  {
    QColor light(255, 255, 255, 80);
    p.setPen(light);
    p.drawLine(backgroundRect.topRight() - QPoint(1, 0),  backgroundRect.bottomRight() - QPoint(1, 0));
    QColor dark(0, 0, 0, 90);
    p.setPen(dark);
    p.drawLine(backgroundRect.topLeft(), backgroundRect.bottomLeft());

    p.setPen(StyleHelper::borderColor());
    p.drawLine(backgroundRect.topRight(), backgroundRect.bottomRight());
  }

}

void FancyTabWidget::tabBarUpdateGeometry() {
  tabBar()->updateGeometry();
}

void FancyTabWidget::SetMode(FancyTabWidget::Mode mode) {

  mode_ = mode;

  if (mode == FancyTabWidget::Mode_Tabs ||  mode == FancyTabWidget::Mode_IconOnlyTabs) {
    setTabPosition(QTabWidget::North);
  }
  else {
    setTabPosition(QTabWidget::West);
  }

  tabBar()->updateGeometry();
  updateGeometry();

  // There appears to be a bug in QTabBar which causes tabSizeHint to be ignored thus the need for this second shot repaint
  QTimer::singleShot(1, this, SLOT(tabBarUpdateGeometry()));

  emit ModeChanged(mode);

}

void FancyTabWidget::addMenuItem(QActionGroup* group, const QString& text, Mode mode) {

  QAction* action = group->addAction(text);
  action->setCheckable(true);
  connect(action, &QAction::triggered, [this, mode]() { SetMode(mode); } );

  if (mode == mode_) action->setChecked(true);

}

void FancyTabWidget::contextMenuEvent(QContextMenuEvent* e) {

  if (!menu_) {
    menu_ = new QMenu(this);
    QActionGroup* group = new QActionGroup(this);
    addMenuItem(group, tr("Large sidebar"), Mode_LargeSidebar);
    addMenuItem(group, tr("Small sidebar"), Mode_SmallSidebar);
    addMenuItem(group, tr("Plain sidebar"), Mode_PlainSidebar);
    addMenuItem(group, tr("Tabs on top"), Mode_Tabs);
    addMenuItem(group, tr("Icons on top"), Mode_IconOnlyTabs);
    menu_->addActions(group->actions());
  }

  menu_->popup(e->globalPos());

}
