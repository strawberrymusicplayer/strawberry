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

#ifndef ANDROIDSETTINGSPAGE_H
#define ANDROIDSETTINGSPAGE_H

#include "config.h"
#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QEvent>
#include <QLayout>
#include <QPushButton>
#include <QSize>

class SettingsPage;

template<typename Base>
class AndroidSettingsButton : public Base {
 public:
  explicit AndroidSettingsButton(QWidget *parent) : Base(parent) {}

  QSize sizeHint() const override { return this->layout()->totalSizeHint().expandedTo(QSize(0, 48)); }
  QSize minimumSizeHint() const override { return this->layout()->totalMinimumSize().expandedTo(QSize(0, 48)); }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override { return qMax(48, this->layout()->totalHeightForWidth(width)); }

 protected:
  bool hitButton(const QPoint &position) const override { return this->rect().contains(position); }

  bool event(QEvent *event) override {
    const bool handled = Base::event(event);
    if (this->layout() && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize || event->type() == QEvent::FontChange)) {
      this->setMinimumHeight(heightForWidth(this->width()));
    }
    return handled;
  }
};

namespace AndroidSettingsPage {
void Adapt(SettingsPage *page);
}
#endif

#endif  // ANDROIDSETTINGSPAGE_H
