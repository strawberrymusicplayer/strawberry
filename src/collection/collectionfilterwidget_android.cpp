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

// Android-only parts of CollectionFilterWidget.

#include "config.h"

#include <QMenu>
#include <QToolButton>

#include "collectionfilterwidget.h"
#include "ui_collectionfilterwidget.h"
#include "widgets/androidwidgets.h"
#include "widgets/menupage_android.h"

void CollectionFilterWidget::SetupAndroidOptions() {

  // The display options open as a full page in the sidebar tab instead of a desktop popup menu.
  ui_->options->setMenu(nullptr);
  QObject::disconnect(ui_->options, &QToolButton::clicked, ui_->options, &QToolButton::showMenu);
  ui_->options->setPopupMode(QToolButton::DelayedPopup);
  ui_->options->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  ui_->options->setText(tr("Options"));
  ui_->options->setAccessibleName(collection_menu_->title());
  ui_->options->setMinimumSize(AndroidWidgets::kMinimumTouchSize, AndroidWidgets::kMinimumTouchSize);

  MenuPage *page = new MenuPage(collection_menu_, this);
  QObject::connect(ui_->options, &QToolButton::clicked, page, &MenuPage::Open);

}
