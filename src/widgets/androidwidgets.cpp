/*
 * Strawberry Music Player
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

#include "androidwidgets.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

namespace AndroidWidgets {

namespace {
constexpr int kPageTitlePointSize = 18;
constexpr qreal kNavigationTextScale = 1.15;
}

void SetPageTitleFont(QWidget *widget) {

  QFont font = widget->font();
  font.setPointSize(kPageTitlePointSize);
  font.setBold(true);
  widget->setFont(font);

}

QPushButton *CreateBackButton(QWidget *parent) {

  QPushButton *back = new QPushButton(QCoreApplication::translate("AndroidWidgets", "Back"), parent);
  back->setIcon(IconLoader::Load(u"go-previous"_s));
  back->setIconSize(QSize(kNavigationIconSize, kNavigationIconSize));
  back->setMinimumHeight(kMinimumTouchSize);
  return back;

}

QHBoxLayout *CreatePageHeader(QLabel *title, QPushButton *back) {

  SetPageTitleFont(title);
  title->setWordWrap(true);
  QHBoxLayout *header = new QHBoxLayout;
  header->addWidget(title, 1);
  header->addWidget(back);
  return header;

}

void StyleNavigationButton(QAbstractButton *button, const int minimum_height) {

  QFont font = button->font();
  font.setPointSizeF(font.pointSizeF() * kNavigationTextScale);
  button->setFont(font);
  button->setStyleSheet(u"text-align: left; padding-left: 24px;"_s);
  button->setMinimumHeight(minimum_height);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

}

void SetWindowBackground(QScrollArea *area) {

  area->setBackgroundRole(QPalette::Window);
  area->setAutoFillBackground(true);
  area->viewport()->setBackgroundRole(QPalette::Window);
  area->viewport()->setAutoFillBackground(true);

}

ScrollPage CreateScrollPage(QWidget *parent) {

  QScrollArea *area = new QScrollArea(parent);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  SetWindowBackground(area);

  QWidget *contents = new QWidget(area);
  contents->setBackgroundRole(QPalette::Window);
  contents->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  area->setWidget(contents);

  return {area, contents, layout};

}

}  // namespace AndroidWidgets
