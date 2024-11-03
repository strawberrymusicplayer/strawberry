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

#include <algorithm>

#include <QTabBar>
#include <QString>
#include <QFontMetrics>
#include <QFont>
#include <QStylePainter>
#include <QPaintEvent>
#include <QTransform>
#include <QLinearGradient>
#include <QColor>
#include <QPen>

#include "core/stylehelper.h"
#include "fancytabbar.h"
#include "fancytabwidget.h"
#include "fancytabdata.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int TabSize_LargeSidebarMinWidth = 70;
}  // namespace

FancyTabBar::FancyTabBar(QWidget *parent) :
    QTabBar(parent),
    mouseHoverTabIndex(-1) {

  setMouseTracking(true);

}

QString FancyTabBar::TabText(const int index) const {
  return tabText(index).isEmpty() || !tabData(index).isValid() ? ""_L1 : tabData(index).value<FancyTabData*>()->label();
}

QSize FancyTabBar::sizeHint() const {

  FancyTabWidget *tabWidget = qobject_cast<FancyTabWidget*>(parentWidget());
  if (tabWidget->mode() == FancyTabWidget::Mode::Tabs || tabWidget->mode() == FancyTabWidget::Mode::IconOnlyTabs) {
    return QTabBar::sizeHint();
  }

  QSize size;
  int h = 0;
  for (int i = 0; i < count(); ++i) {
    if (tabSizeHint(i).width() > size.width()) size.setWidth(tabSizeHint(i).width());
    h += tabSizeHint(i).height();
  }
  size.setHeight(h);

  return size;

}

int FancyTabBar::width() const {

  FancyTabWidget *tabWidget = qobject_cast<FancyTabWidget*>(parentWidget());
  if (tabWidget->mode() == FancyTabWidget::Mode::LargeSidebar || tabWidget->mode() == FancyTabWidget::Mode::SmallSidebar) {
    int w = 0;
    for (int i = 0; i < count(); ++i) {
      if (tabSizeHint(i).width() > w) w = tabSizeHint(i).width();
    }
    return w;
  }
  else {
    return QTabBar::width();
  }

}

QSize FancyTabBar::tabSizeHint(const int index) const {

  FancyTabWidget *tabWidget = qobject_cast<FancyTabWidget*>(parentWidget());

  QSize size;
  if (tabWidget->mode() == FancyTabWidget::Mode::LargeSidebar) {

    QFont bold_font(font());
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);

    // If the text of any tab is wider than the set width then use that instead.
    int w = std::max(TabSize_LargeSidebarMinWidth, tabWidget->iconsize_largesidebar() + 22);
    for (int i = 0; i < count(); ++i) {
      QRect rect = fm.boundingRect(QRect(0, 0, std::max(TabSize_LargeSidebarMinWidth, tabWidget->iconsize_largesidebar() + 22), height()), Qt::TextWordWrap, TabText(i));
      rect.setWidth(rect.width() + 10);
      if (rect.width() > w) w = rect.width();
    }

    QRect rect = fm.boundingRect(QRect(0, 0, w, height()), Qt::TextWordWrap, TabText(index));
    size = QSize(w, tabWidget->iconsize_largesidebar() + rect.height() + 10);
  }
  else if (tabWidget->mode() == FancyTabWidget::Mode::IconsSidebar) {
    size = QSize(tabWidget->iconsize_largesidebar() + 20, tabWidget->iconsize_largesidebar() + 20);
  }
  else if (tabWidget->mode() == FancyTabWidget::Mode::SmallSidebar) {

    QFont bold_font(font());
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);

    QRect rect = fm.boundingRect(QRect(0, 0, 100, tabWidget->height()), Qt::AlignHCenter, TabText(index));
    int w = std::max(tabWidget->iconsize_smallsidebar(), rect.height()) + 15;
    int h = tabWidget->iconsize_smallsidebar() + rect.width() + 20;
    size = QSize(w, h);
  }
  else {
    size = QTabBar::tabSizeHint(index);
  }

  return size;

}

void FancyTabBar::leaveEvent(QEvent *event) {

  Q_UNUSED(event);

  mouseHoverTabIndex = -1;
  update();

}

void FancyTabBar::mouseMoveEvent(QMouseEvent *event) {

  QPoint pos = event->pos();

  mouseHoverTabIndex = tabAt(pos);
  if (mouseHoverTabIndex > -1) {
    update();
  }
  QTabBar::mouseMoveEvent(event);

}

void FancyTabBar::paintEvent(QPaintEvent *pe) {

  FancyTabWidget *tabWidget = qobject_cast<FancyTabWidget*>(parentWidget());

  if (tabWidget->mode() != FancyTabWidget::Mode::LargeSidebar &&
      tabWidget->mode() != FancyTabWidget::Mode::SmallSidebar &&
      tabWidget->mode() != FancyTabWidget::Mode::IconsSidebar) {
    QTabBar::paintEvent(pe);
    return;
  }

  const bool vertical_text_tabs = tabWidget->mode() == FancyTabWidget::Mode::SmallSidebar;

  QStylePainter p(this);

  for (int index = 0; index < count(); ++index) {
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
      int textFlags = 0;
      Qt::Alignment iconFlags;

      QRect tabrectText;
      QRect tabrectLabel;

      if (vertical_text_tabs) {
        m = QTransform::fromTranslate(tabrect.left(), tabrect.bottom());
        m.rotate(-90);
        textFlags = Qt::AlignVCenter;
        iconFlags = Qt::AlignVCenter;

        tabrectLabel = QRect(QPoint(0, 0), m.mapRect(tabrect).size());

        tabrectText = tabrectLabel;
        tabrectText.translate(tabWidget->iconsize_smallsidebar() + 8, 0);
      }
      else {
        m = QTransform::fromTranslate(tabrect.left(), tabrect.top());
        textFlags = Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap;
        iconFlags = Qt::AlignHCenter | Qt::AlignTop;

        tabrectLabel = QRect(QPoint(0, 0), m.mapRect(tabrect).size());

        tabrectText = tabrectLabel;
        tabrectText.translate(0, -5);
      }

      p.setTransform(m);

      QFont boldFont(p.font());
      boldFont.setBold(true);
      p.setFont(boldFont);

      // Text drop shadow color
      p.setPen(selected ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 110));
      p.translate(0, 3);
      p.drawText(tabrectText, textFlags, TabText(index));

      // Text foreground color
      p.translate(0, -1);
      p.setPen(selected ? QColor(60, 60, 60) : StyleHelper::panelTextColor());
      p.drawText(tabrectText, textFlags, TabText(index));


      // Draw the icon
      QRect tabrectIcon;
      if (vertical_text_tabs) {
        tabrectIcon = tabrectLabel;
        tabrectIcon.setSize(QSize(tabWidget->iconsize_smallsidebar(), tabWidget->iconsize_smallsidebar()));
        // Center the icon
        const int moveRight = (QTabBar::width() - tabWidget->iconsize_smallsidebar()) / 2;
        tabrectIcon.translate(5, moveRight);
      }
      else {
        tabrectIcon = tabrectLabel;
        tabrectIcon.setSize(QSize(tabWidget->iconsize_largesidebar(), tabWidget->iconsize_largesidebar()));
        // Center the icon
        const int moveRight = (QTabBar::width() - tabWidget->iconsize_largesidebar() - 1) / 2;

        if (tabWidget->mode() == FancyTabWidget::Mode::IconsSidebar) {
          tabrectIcon.translate(moveRight, (tabSizeHint(0).height() - tabWidget->iconsize_largesidebar() - 5) / 2);
        }
        else {
          tabrectIcon.translate(moveRight, 5);
        }
      }
      tabIcon(index).paint(&p, tabrectIcon, iconFlags);
      p.restore();
    }
  }

}
