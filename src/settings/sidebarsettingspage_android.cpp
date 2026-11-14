// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarsettingspage_android.h"

#include <algorithm>
#include <utility>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "constants/mainwindowsettings.h"
#include "core/iconloader.h"
#include "settingsdialog.h"
#include "widgets/androidwidgets.h"

using namespace Qt::Literals::StringLiterals;

AndroidSidebarSettingsPage::AndroidSidebarSettingsPage(SettingsDialog *dialog, FancyTabWidget *sidebar, QWidget *parent)
    : SettingsPage(dialog, parent), sidebar_(sidebar) {

  setWindowTitle(tr("Sidebar"));
  setWindowIcon(IconLoader::Load(u"view-choose"_s));
  const auto create_button = [](const QString &text, QWidget *owner) {
    auto *result = new AndroidWidgets::LayoutButton<QPushButton>(owner);
    auto *labels = new QVBoxLayout(result);
    AndroidWidgets::AddButtonLabel(labels, text);
    result->setAccessibleName(text);
    result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return result;
  };
  auto *layout = new QVBoxLayout(this);
  auto *open = create_button(tr("Arrange sidebar tabs"), this);
  layout->addWidget(open);
  summary_ = new QLabel(this);
  summary_->setWordWrap(true);
  layout->addWidget(summary_);
  layout->addStretch();

  editor_ = new QWidget(this);
  editor_->setObjectName(u"sidebar_editor"_s);
  editor_->setMinimumWidth(0);
  auto *editor_layout = new QVBoxLayout(editor_);
  auto *instructions = new QLabel(tr("Select a tab to move it or change its visibility. Changes take effect only after Apply or OK."), editor_);
  instructions->setWordWrap(true);
  editor_layout->addWidget(instructions);
  list_ = new QListWidget(editor_);
  list_->setObjectName(u"sidebar_tabs"_s);
  list_->setMinimumSize(0, 224);
  list_->setWordWrap(true);
  list_->setTextElideMode(Qt::ElideNone);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setStyleSheet(u"QListWidget::item { min-height: 48px; padding: 4px; }"_s);
  editor_layout->addWidget(list_);
  auto *moves = new QHBoxLayout;
  move_up_ = create_button(tr("Move up"), editor_);
  move_up_->setObjectName(u"sidebar_move_up"_s);
  move_down_ = create_button(tr("Move down"), editor_);
  move_down_->setObjectName(u"sidebar_move_down"_s);
  for (QPushButton *button : {move_up_, move_down_}) {
    moves->addWidget(button);
  }
  editor_layout->addLayout(moves);
  visible_ = new QCheckBox(tr("Show this tab"), editor_);
  visible_->setObjectName(u"sidebar_visible"_s);
  AndroidWidgets::AdaptCheckBox(visible_);
  editor_layout->addWidget(visible_);
  hint_ = new QLabel(editor_);
  hint_->setWordWrap(true);
  editor_layout->addWidget(hint_);
  editor_->hide();

#ifdef Q_OS_ANDROID
  QObject::connect(open, &QPushButton::clicked, this, [this]() { this->dialog()->OpenAndroidSubpage(editor_, tr("Arrange tabs")); });
#endif
  QObject::connect(list_, &QListWidget::currentRowChanged, this, [this]() { UpdateSelection(); });
  QObject::connect(move_up_, &QPushButton::clicked, this, [this]() { MoveSelectedTab(-1); });
  QObject::connect(move_down_, &QPushButton::clicked, this, [this]() { MoveSelectedTab(1); });
  QObject::connect(visible_, &QCheckBox::toggled, this, [this](const bool checked) {
    const int row = list_->currentRow();
    if (row < 0) return;
    pending_[row].visible = checked;
    set_changed();
    UpdateSummary();
  });

}

void AndroidSidebarSettingsPage::Load() {

  pending_ = sidebar_->AndroidSidebarTabs(QLatin1String(MainWindowSettings::kSettingsGroup));
  {
    const QSignalBlocker blocker(list_);
    list_->clear();
    for (const FancyTabWidget::SidebarTab &tab : std::as_const(pending_)) {
      list_->addItem(tab.label);
    }
  }
  list_->setCurrentRow(0);
  UpdateSelection();
  UpdateSummary();
  Init(this);

}

void AndroidSidebarSettingsPage::Save() {

  sidebar_->SaveAndroidSidebarSettings(QLatin1String(MainWindowSettings::kSettingsGroup), pending_);

}

void AndroidSidebarSettingsPage::Cancel() {

  Load();

}

void AndroidSidebarSettingsPage::MoveSelectedTab(const int direction) {

  const int row = list_->currentRow();
  const int destination = row + direction;
  if (row < 0 || destination < 0 || destination >= pending_.count()) return;
  pending_.swapItemsAt(row, destination);
  const QSignalBlocker blocker(list_);
  QListWidgetItem *item = list_->takeItem(row);
  list_->insertItem(destination, item);
  list_->setCurrentRow(destination);
  list_->scrollToItem(item);
  UpdateSelection();
  set_changed();

}

void AndroidSidebarSettingsPage::UpdateSelection() {

  const int row = list_->currentRow();
  const bool selected = row >= 0;
  move_up_->setEnabled(selected && row > 0);
  move_down_->setEnabled(selected && row + 1 < pending_.count());
  const QSignalBlocker blocker(visible_);
  visible_->setEnabled(selected && pending_.at(row).name != u"settings"_s);
  visible_->setChecked(selected && pending_.at(row).visible);
  hint_->setText(!selected ? QString() : pending_.at(row).name == u"settings"_s ? tr("Settings always remains visible so hidden tabs can be restored.") :
                !pending_.at(row).available ? tr("This service is disabled. Enable it in Streaming settings before it can appear in the sidebar.") : QString());
  hint_->setVisible(!hint_->text().isEmpty());

}

void AndroidSidebarSettingsPage::UpdateSummary() {

  const int visible = std::count_if(pending_.cbegin(), pending_.cend(), [](const FancyTabWidget::SidebarTab &tab) { return tab.visible && tab.available; });
  summary_->setText(tr("%n visible tab(s)", nullptr, visible));
  for (int row = 0; row < pending_.count(); ++row) {
    const FancyTabWidget::SidebarTab &tab = pending_.at(row);
    list_->item(row)->setText(!tab.visible ? tr("%1 (hidden)").arg(tab.label) : !tab.available ? tr("%1 (disabled)").arg(tab.label) : tab.label);
  }

}
