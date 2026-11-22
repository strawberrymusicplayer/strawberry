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

// Android-only parts of SettingsDialog, kept apart from settingsdialog.cpp to reduce conflicts when merging upstream.

#include <algorithm>
#include <functional>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSize>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "widgets/actiondialog.h"
#include "widgets/androidwidgets.h"
#include "widgets/scrollarea_android.h"
#include "settingsdialog.h"
#include "settingspage.h"
#include "streamingautosave_android.h"
#include "ui_settingsdialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kAndroidCategoryButtonHeight = 64;
constexpr int kAndroidHelpButtonHeight = 56;
// Groups slider drags and quick successive taps into one save.
constexpr int kAndroidSaveDelayMs = 300;

}  // namespace

void SettingsDialog::SetupAndroidAutoSave() {

  android_save_timer_ = new QTimer(this);
  android_save_timer_->setSingleShot(true);
  android_save_timer_->setInterval(kAndroidSaveDelayMs);
  QObject::connect(android_save_timer_, &QTimer::timeout, this, &SettingsDialog::FlushAndroidSettings);
  const auto schedule = [this]() { ScheduleAndroidSave(); };
  for (auto it = pages_.cbegin(); it != pages_.cend(); ++it) {
    // Streaming pages save through SetupAndroidStreamingSettings() or their own page code.
    if (IsAndroidStreamingPage(it.key())) continue;
    ConnectSettingsPageAutoSave(this, it->page_, schedule);
  }

  // Leaving the Settings tab or the app flushes pending text edits.
  android_hide_filter_ = InstallFlushOnHide(this, [this]() { FlushAndroidSettings(); });
  QObject::connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive) FlushAndroidSettings();
  });

}

void SettingsDialog::ScheduleAndroidSave() {

  if (loading_settings_ || !android_settings_loaded_) return;
  android_save_timer_->start();

}

void SettingsDialog::FlushAndroidSettings() {

  android_save_timer_->stop();
  if (loading_settings_ || !android_settings_loaded_) return;
  const QScopedValueRollback<bool> saving(loading_settings_, true);
  bool saved = false;
  for (const PageData &data : std::as_const(pages_)) {
    if (data.page_->isEnabled() && data.page_->Apply()) saved = true;
  }
  if (!saved) return;
  Settings settings;
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    const QString message = tr("Could not save settings. Please check storage access and try again.");
    qLog(Error) << message;
    auto *notification = new ActionDialog(this, tr("Settings could not be saved"), message, ActionDialog::TitlePosition::Top);
    notification->AddCancelButton(tr("Close"));
    notification->Open();
  }
  Q_EMIT ReloadSettings();

}

void SettingsDialog::SetupAndroidNavigation() {

  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  android_navigation_ = new QStackedWidget(this);
  android_navigation_->setMinimumWidth(0);
  android_navigation_->setBackgroundRole(QPalette::Base);
  android_navigation_->setAutoFillBackground(true);
  const AndroidWidgets::ScrollPage categories_scroll_page = AndroidWidgets::CreateScrollPage(android_navigation_);
  android_categories_page_ = categories_scroll_page.area;
  QWidget *categories_contents = categories_scroll_page.contents;
  android_categories_layout_ = categories_scroll_page.layout;
  android_navigation_->addWidget(android_categories_page_);
  const QStringList category_icon_names = {u"strawberry"_s, u"view-choose"_s, u"applications-internet"_s};

  QTreeWidgetItem *root = ui_->list->invisibleRootItem();
  for (int i = 0; i < root->childCount(); ++i) {
    QTreeWidgetItem *category = root->child(i);
    const AndroidWidgets::ScrollPage scroll_page = AndroidWidgets::CreateScrollPage(android_navigation_);
    QScrollArea *category_page = scroll_page.area;
    QWidget *category_contents = scroll_page.contents;
    QVBoxLayout *category_layout = scroll_page.layout;

    QPushButton *category_button = new QPushButton(category->text(0), categories_contents);
    if (i < category_icon_names.size()) {
      category_button->setIcon(IconLoader::Load(category_icon_names.at(i)));
      category_button->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
    }
    AndroidWidgets::StyleNavigationButton(category_button, kAndroidCategoryButtonHeight);
    android_categories_layout_->addWidget(category_button);
    QObject::connect(category_button, &QPushButton::clicked, this, [this, category_page, category]() {
      android_current_category_page_ = category_page;
      SetAndroidTitle(category->text(0));
      SetAndroidNavigationPage(category_page);
      ui_->buttonBox->hide();
    });

    for (int j = 0; j < category->childCount(); ++j) {
      QTreeWidgetItem *item = category->child(j);
      auto page_it = std::find_if(pages_.cbegin(), pages_.cend(), [item](const PageData &page) { return page.item_ == item; });
      if (page_it == pages_.cend()) continue;

      QPushButton *page_button = new QPushButton(item->icon(0), item->text(0), category_contents);
      page_button->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
      AndroidWidgets::StyleNavigationButton(page_button, kAndroidCategoryButtonHeight);
      if (IsAndroidStreamingPage(page_it.key()) && page_it.key() != Page::Radio) {
        auto *status = new QLabel(page_it.key() == Page::Spotify ? tr("Tested but not working") : tr("Not tested"), category_contents);
        status->setWordWrap(true);
        status->setMinimumWidth(0);
        QFont status_font = status->font();
        status_font.setPointSizeF(std::max(8.0, status_font.pointSizeF() * 0.85));
        status->setFont(status_font);
        page_button->setAccessibleDescription(status->text());
        auto *row = new QHBoxLayout;
        row->addWidget(page_button, 2);
        row->addWidget(status, 1);
        category_layout->addLayout(row);
      }
      else {
        category_layout->addWidget(page_button);
      }
      android_category_pages_.insert(page_it.key(), category_page);
      QObject::connect(page_button, &QPushButton::clicked, this, [this, item, category_page]() {
        android_current_category_page_ = category_page;
        ui_->list->setCurrentItem(item);
        CurrentItemChanged(item);
        SetAndroidNavigationPage(ui_->stacked_widget);
        UpdateAndroidPageButtons();
      });
    }

    category_layout->addStretch();
    android_navigation_->addWidget(category_page);
  }
  android_categories_layout_->addStretch();

  QPushButton *back_button = AndroidWidgets::CreateBackButton(this);
  QHBoxLayout *header = AndroidWidgets::CreatePageHeader(ui_->title, back_button);
  ui_->layout->removeWidget(ui_->title);
  ui_->layout->removeWidget(ui_->line_2);
  ui_->line_2->hide();
  ui_->layout->removeWidget(ui_->stacked_widget);
  android_navigation_->addWidget(ui_->stacked_widget);
  ui_->layout->removeWidget(ui_->line_1);
  ui_->line_1->hide();
  ui_->layout->insertLayout(0, header);
  ui_->layout->insertWidget(1, android_navigation_);
  ui_->list->hide();
  ui_->stacked_widget->setMinimumWidth(0);
  ui_->stacked_widget->setBackgroundRole(QPalette::Base);
  ui_->stacked_widget->setAutoFillBackground(true);
  ui_->buttonBox->setMinimumWidth(0);
  ui_->buttonBox->hide();
  setMinimumSize(0, 0);
  ui_->layout_dialog->setSizeConstraint(QLayout::SetNoConstraint);

  QObject::connect(back_button, &QPushButton::clicked, this, &SettingsDialog::AndroidBack);

}

void SettingsDialog::OpenOverview() {

  while (!android_subpages_.isEmpty()) CloseAndroidSubpage();
  SetAndroidTitle();
  SetAndroidNavigationPage(android_categories_page_);
  ui_->buttonBox->hide();
  show();

}

void SettingsDialog::AddAndroidHelpCategory(const QList<QAction*> &actions) {

  const AndroidWidgets::ScrollPage scroll_page = AndroidWidgets::CreateScrollPage(android_navigation_);
  QScrollArea *help_page = scroll_page.area;

  for (QAction *action : actions) {
    QPushButton *button = new QPushButton(action->icon(), action->iconText(), scroll_page.contents);
    AndroidWidgets::StyleNavigationButton(button, kAndroidHelpButtonHeight);
    scroll_page.layout->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
  }
  scroll_page.layout->addStretch();
  android_navigation_->addWidget(help_page);

  QPushButton *help_button = new QPushButton(IconLoader::Load(u"help-hint"_s), tr("Help"), android_categories_layout_->parentWidget());
  help_button->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
  AndroidWidgets::StyleNavigationButton(help_button, kAndroidCategoryButtonHeight);
  // Insert above the trailing stretch of the overview.
  android_categories_layout_->insertWidget(android_categories_layout_->count() - 1, help_button);
  QObject::connect(help_button, &QPushButton::clicked, this, [this, help_page]() {
    android_current_category_page_ = help_page;
    SetAndroidTitle(tr("Help"));
    SetAndroidNavigationPage(help_page);
    ui_->buttonBox->hide();
  });

  SetAndroidNavigationPage(android_navigation_->currentWidget());

}
bool SettingsDialog::IsAndroidPageActive(const SettingsPage *page) const {

  if (!isVisible() || android_subpages_.isEmpty()) return false;
  for (const PageData &data : pages_) {
    if (data.page_ == page) return data.item_ == ui_->list->currentItem();
  }
  return false;

}

QWidgetList SettingsDialog::AndroidSubpageWidgets(const QWidget *ui_widget) const {

  QWidgetList widgets;
  for (const AndroidSubpage &subpage : android_subpages_) {
    QWidget *content = subpage.area->widget();
    // Transient subpages are deleted on close and hold no page settings.
    if (!content || content->property("androidTransientSubpage").toBool()) continue;
    if (subpage.content_parent != ui_widget && !ui_widget->isAncestorOf(subpage.content_parent)) continue;
    widgets << content << content->findChildren<QWidget*>();
  }
  return widgets;

}

void SettingsDialog::UpdateAndroidPageButtons() {
  // Android settings save as they change, so Apply, OK and Cancel are never shown.
  ui_->buttonBox->hide();
}

void SettingsDialog::OpenAndroidSubpage(QWidget *content, const QString &title) {

  auto *parent_area = android_subpages_.isEmpty() ? qobject_cast<QScrollArea*>(ui_->stacked_widget->currentWidget()) : android_subpages_.last().area;
  auto *area = new QScrollArea;
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  AndroidWidgets::SetPageBackground(area);
  content->setBackgroundRole(QPalette::Base);
  android_subpages_.append({area, content->parentWidget(), parent_area, parent_area->verticalScrollBar()->value(), title});
  area->setWidget(content);
  android_navigation_->addWidget(area);
  QTreeWidgetItem *item = ui_->list->currentItem();
  QStringList titles;
  for (const AndroidSubpage &subpage : android_subpages_) titles << subpage.title;
  SetAndroidTitle(item->parent()->text(0), item->text(0), titles.join(u" > "_s));
  ui_->buttonBox->hide();
  SetAndroidNavigationPage(area);
  QTimer::singleShot(0, area, [area]() {
    if (!area->widget()) return;
    for (QRadioButton *choice : area->widget()->findChildren<QRadioButton*>()) {
      if (choice->isVisible() && choice->isChecked()) {
        area->ensureWidgetVisible(choice);
        break;
      }
    }
  });

}

void SettingsDialog::CloseAndroidSubpage() {

  if (android_subpages_.isEmpty()) return;
  FlushAndroidSettings();
  const AndroidSubpage subpage = android_subpages_.takeLast();
  // Move focus before takeWidget() clears QScrollArea's internal widget pointer.
  setFocus(Qt::OtherFocusReason);
  subpage.area->widget()->hide();
  QWidget *content = subpage.area->takeWidget();
  content->setParent(subpage.content_parent);
  content->hide();
  android_navigation_->removeWidget(subpage.area);
  subpage.area->deleteLater();
  if (android_subpages_.isEmpty()) {
    CurrentItemChanged(ui_->list->currentItem());
    UpdateAndroidPageButtons();
    SetAndroidNavigationPage(ui_->stacked_widget);
  }
  else {
    QTreeWidgetItem *item = ui_->list->currentItem();
    QStringList titles;
    for (const AndroidSubpage &parent : android_subpages_) titles << parent.title;
    SetAndroidTitle(item->parent()->text(0), item->text(0), titles.join(u" > "_s));
    SetAndroidNavigationPage(subpage.parent_area);
  }
  QTimer::singleShot(0, subpage.parent_area, [parent_area = subpage.parent_area, value = subpage.parent_scroll_value]() {
    parent_area->verticalScrollBar()->setValue(value);
  });
  if (content->property("androidTransientSubpage").toBool()) content->deleteLater();

}

void SettingsDialog::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    AndroidBack();
    event->accept();
    return;
  }
  QDialog::keyPressEvent(event);

}

void SettingsDialog::AndroidBack() {

  if (!android_subpages_.isEmpty()) {
    CloseAndroidSubpage();
  }
  else if (android_navigation_->currentWidget() == ui_->stacked_widget) {
    FlushAndroidSettings();
    SetAndroidNavigationPage(android_current_category_page_);
    ui_->buttonBox->hide();
    QTreeWidgetItem *item = ui_->list->currentItem();
    if (item && item->parent()) {
      SetAndroidTitle(item->parent()->text(0));
    }
  }
  else if (android_navigation_->currentWidget() != android_categories_page_) {
    SetAndroidTitle();
    SetAndroidNavigationPage(android_categories_page_);
    ui_->buttonBox->hide();
  }
  else {
    reject();
  }

}

void SettingsDialog::SetAndroidNavigationPage(QWidget *page) {

  if (!page) return;

  if (page == ui_->stacked_widget) {
    if (QWidget *current_page = ui_->stacked_widget->currentWidget()) {
      current_page->show();
      if (QScrollArea *scroll_area = qobject_cast<QScrollArea*>(current_page)) {
        if (QWidget *page_widget = scroll_area->widget()) {
          page_widget->show();
        }
      }
    }
  }
  else {
    for (const PageData &page_data : pages_) {
      page_data.scroll_area_->hide();
      page_data.page_->hide();
    }
  }

  android_navigation_->setCurrentWidget(page);
  for (int i = 0; i < android_navigation_->count(); ++i) {
    QWidget *widget = android_navigation_->widget(i);
    if (widget != page) {
      widget->hide();
      if (QScrollArea *scroll_area = qobject_cast<QScrollArea*>(widget)) {
        if (QWidget *page_widget = scroll_area->widget()) {
          page_widget->hide();
        }
      }
    }
  }
  page->show();
  page->setGeometry(android_navigation_->rect());
  page->raise();
  if (QScrollArea *scroll_area = qobject_cast<QScrollArea*>(page)) {
    if (QWidget *page_widget = scroll_area->widget()) {
      page_widget->show();
    }
  }

  // Pages and subpages scroll by finger; enabling it again for a page that was shown before does nothing.
  if (auto *scroll_area = qobject_cast<QScrollArea*>(page == ui_->stacked_widget ? ui_->stacked_widget->currentWidget() : page)) {
    AndroidScrollArea::EnableForContents(scroll_area);
  }

}

void SettingsDialog::SetAndroidTitle(const QString &category, const QString &page, const QString &subpage) {

  QStringList parts{tr("Settings")};
  if (!category.isEmpty()) {
    parts.append(category);
  }
  if (!page.isEmpty()) {
    parts.append(page);
  }

  QStringList escaped_parts;
  escaped_parts.reserve(parts.size());
  for (const QString &part : parts) {
    escaped_parts.append(part.toHtmlEscaped());
  }
  QString title = escaped_parts.join(u" &gt; "_s);
  if (!subpage.isEmpty()) {
    title += u"<br/>"_s + subpage.toHtmlEscaped();
  }
  ui_->title->setText(u"<b>"_s + title + u"</b>"_s);

}
