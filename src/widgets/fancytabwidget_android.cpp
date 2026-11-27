// SPDX-License-Identifier: GPL-3.0-or-later

#include "fancytabwidget.h"

#include <algorithm>
#include <utility>

#include <QMap>
#include <QSet>
#include <QSignalBlocker>
#include <QTabBar>

#include "core/logging.h"
#include "core/settings.h"
#include "fancytabdata.h"

using namespace Qt::Literals::StringLiterals;

QList<FancyTabWidget::SidebarTab> FancyTabWidget::AndroidSidebarTabs(const QString &settings_group) const {

  Settings settings;
  settings.beginGroup(settings_group);
  QList<FancyTabData*> added = tabs_.values();
  std::sort(added.begin(), added.end(), [](const FancyTabData *a, const FancyTabData *b) { return a->index() < b->index(); });
  QMap<int, FancyTabData*> ordered;
  for (FancyTabData *tab : std::as_const(added)) {
    int position = settings.value(u"tab_"_s + tab->name(), tab->index()).toInt();
    while (ordered.contains(position)) ++position;
    ordered.insert(position, tab);
  }
  QList<SidebarTab> result;
  for (FancyTabData *tab : std::as_const(ordered)) {
    const bool visible = tab->name() == u"settings"_s || !settings.value(u"tab_hidden_"_s + tab->name(), false).toBool();
    result.append({tab->name(), tab->label(), visible, indexOf(tab->page()) >= 0});
  }
  return result;

}

void FancyTabWidget::LoadAndroidSidebarSettings(const QString &settings_group) {

  QWidget *previous_page = currentWidget();
  const QList<SidebarTab> ordered = AndroidSidebarTabs(settings_group);
  {
    const QSignalBlocker blocker(this);
    for (FancyTabData *tab : std::as_const(tabs_)) {
      if (tab->name() == u"settings"_s && indexOf(tab->page()) >= 0) setTabVisible(indexOf(tab->page()), true);
    }
    int position = 0;
    for (const SidebarTab &entry : ordered) {
      for (FancyTabData *tab : std::as_const(tabs_)) {
        if (tab->name() != entry.name) continue;
        const int index = indexOf(tab->page());
        if (index < 0) continue;
        tabBar()->moveTab(index, position);
        setTabVisible(position, entry.visible);
        ++position;
      }
    }
    const int previous_index = indexOf(previous_page);
    if (previous_index >= 0 && isTabVisible(previous_index)) setCurrentIndex(previous_index);
  }
  // Moving tabs must not reopen an embedded editor or reset the active page's navigation.
  if (currentWidget() != previous_page) {
    Q_EMIT currentChanged(currentIndex());
    CurrentTabChangedSlot(currentIndex());
  }

}

void FancyTabWidget::SaveAndroidSidebarSettings(const QString &settings_group, const QList<SidebarTab> &tabs) {

  QSet<QString> names;
  for (FancyTabData *tab : std::as_const(tabs_)) names.insert(tab->name());
  for (const SidebarTab &entry : tabs) {
    if (!names.remove(entry.name)) {
      qLog(Error) << "Invalid or duplicate sidebar tab" << entry.name;
      return;
    }
  }
  if (!names.isEmpty()) {
    qLog(Error) << "Incomplete sidebar tab arrangement";
    return;
  }
  {
    Settings settings;
    settings.beginGroup(settings_group);
    for (int position = 0; position < tabs.count(); ++position) {
      const SidebarTab &entry = tabs.at(position);
      settings.setValue(u"tab_"_s + entry.name, position);
      settings.setValue(u"tab_hidden_"_s + entry.name, entry.name != u"settings"_s && !entry.visible);
    }
  }
  LoadAndroidSidebarSettings(settings_group);

}
