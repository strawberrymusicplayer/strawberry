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

#include <QCoreApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QString>

#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

namespace AndroidWidgets {

namespace {
constexpr int kPageTitlePointSize = 18;
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

}  // namespace AndroidWidgets
