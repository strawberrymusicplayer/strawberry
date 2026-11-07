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

class QHBoxLayout;
class QLabel;
class QPushButton;
class QWidget;

// Shared building blocks for the Android page layouts.
namespace AndroidWidgets {

// Minimum height or width of touch targets, in device-independent pixels.
constexpr int kMinimumTouchSize = 48;
constexpr int kNavigationIconSize = 32;

void SetPageTitleFont(QWidget *widget);
QPushButton *CreateBackButton(QWidget *parent);
// Returns a row with the breadcrumb title, which takes the free space, followed by the Back button.
QHBoxLayout *CreatePageHeader(QLabel *title, QPushButton *back);

}  // namespace AndroidWidgets

#endif  // Q_OS_ANDROID

#endif  // ANDROIDWIDGETS_H
