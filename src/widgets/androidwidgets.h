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

#ifndef ANDROIDWIDGETS_H
#define ANDROIDWIDGETS_H

#include "config.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include <QEvent>
#include <QLayout>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QString>

class QAbstractButton;
class QBoxLayout;
class QHBoxLayout;
class QLabel;
class QRadioButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

// Shared building blocks for the Android page layouts.
namespace AndroidWidgets {

// Minimum height or width of touch targets, in device-independent pixels.
constexpr int kMinimumTouchSize = 48;
constexpr int kNavigationIconSize = 32;

// A button sized by the layout of its child labels, so that wrapped text grows its height; the whole area is clickable.
template<typename Base>
class LayoutButton : public Base {
 public:
  explicit LayoutButton(QWidget *parent) : Base(parent) {}

  QSize sizeHint() const override { return this->layout()->totalSizeHint().expandedTo(QSize(0, kMinimumTouchSize)); }
  QSize minimumSizeHint() const override { return this->layout()->totalMinimumSize().expandedTo(QSize(0, kMinimumTouchSize)); }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override { return qMax(kMinimumTouchSize, this->layout()->totalHeightForWidth(width)); }

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

// A compact button that opens a choice or editor subpage.
// It shows an optional "title >" caption followed by the current value in bold, in the theme's link color.
class ValueButton : public LayoutButton<QPushButton> {
 public:
  // Without a title only the value is shown; the caller then labels the button, for example with a buddy label.
  explicit ValueButton(const QString &title, QWidget *parent);

  // Shows the value, or hides it when empty; the description, which defaults to the value, is exposed to accessibility.
  void SetValue(const QString &value, const QString &description = QString());

 private:
  QLabel *value_;
};

// Adds a wrapping, plain-text label that lets clicks pass through to the button containing it.
QLabel *AddButtonLabel(QBoxLayout *layout, const QString &text);

// Shows a radio button as a full-width choice row without an indicator.
// While checked, the row uses the theme's highlight colors and bold text.
// The label is the row's text label; without one the button's own text is styled.
void StyleChoiceRow(QRadioButton *button, QLabel *label);

// Creates a choice row whose wrapping label shows the text.
QRadioButton *CreateChoiceRow(const QString &text, QWidget *parent);

void SetPageTitleFont(QWidget *widget);
QPushButton *CreateBackButton(QWidget *parent);
// Returns a row with the breadcrumb title, which takes the free space, followed by the Back button.
QHBoxLayout *CreatePageHeader(QLabel *title, QPushButton *back);

// Styles a full-width navigation button: larger, left-aligned text with a fixed height.
void StyleNavigationButton(QAbstractButton *button, int minimum_height);

// Fills the scroll area and its viewport with the window color.
void SetWindowBackground(QScrollArea *area);

struct ScrollPage {
  QScrollArea *area;
  QWidget *contents;
  QVBoxLayout *layout;
};

// Returns a vertically scrolling, window-colored page whose contents use a margin-free vertical layout.
ScrollPage CreateScrollPage(QWidget *parent);

}  // namespace AndroidWidgets

#endif  // Q_OS_ANDROID

#endif  // ANDROIDWIDGETS_H
