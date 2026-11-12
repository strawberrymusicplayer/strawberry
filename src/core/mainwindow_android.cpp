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

// Android-only parts of MainWindow, kept apart from mainwindow.cpp to reduce conflicts when merging upstream.

#include <functional>

#include <QAction>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QString>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "collection/collectionviewcontainer.h"
#include "playlist/playlist.h"
#include "playlist/playlistlistcontainer.h"
#include "playlist/playlistmanager.h"
#include "settings/settingsdialog.h"
#include "widgets/androidwidgets.h"
#include "widgets/fancytabwidget.h"
#include "widgets/playlisttitleheader.h"
#include "widgets/scantaskcontrols_android.h"

namespace {

constexpr int kAndroidToolButtonHeight = 72;

// Returns to the Tools list when an embedded tool closes, and closes the tool when Back or Escape reaches its page unhandled.
class AndroidToolPageFilter : public QObject {
 public:
  explicit AndroidToolPageFilter(QWidget *page, QWidget *tool, std::function<void()> closed) : QObject(page), page_(page), tool_(tool), closed_(std::move(closed)) {
    page_->installEventFilter(this);
    tool_->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    // Unlike Hide, HideToParent is sent only when the tool itself is hidden, not when the Tools tab is.
    if (object == tool_ && event->type() == QEvent::HideToParent) {
      closed_();
    }
    else if (object == page_ && event->type() == QEvent::KeyPress) {
      const int key = static_cast<QKeyEvent*>(event)->key();
      if (key == Qt::Key_Back || key == Qt::Key_Escape) {
        tool_->close();
        return true;
      }
    }
    return QObject::eventFilter(object, event);
  }

 private:
  QWidget *page_;
  QWidget *tool_;
  std::function<void()> closed_;
};

}  // namespace

void MainWindow::SetupAndroidLayout() {

  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniEnvironment env;
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();
    const QJniObject bar = activity.callObjectMethod("getActionBar", "()Landroid/app/ActionBar;");
    if (bar.isValid()) bar.callMethod<void>("hide");
    if (env.checkAndClearExceptions()) qLog(Error) << "Could not hide the Android action bar";
  });
  auto *header = new ScanTaskControls(app_->task_manager(), ui_->centralWidget);
  ui_->layout_centralWidget->insertWidget(0, ui_->tabs, 1);
  ui_->layout_centralWidget->insertWidget(0, header);
  ui_->layout_centralWidget->addWidget(ui_->player_controls_container);
  ui_->splitter->hide();
  ui_->line_6->hide();
  ui_->layout_right->setContentsMargins(0, 0, 0, 0);

  PlaylistTitleHeader *playlist_header = new PlaylistTitleHeader(ui_->playlist_layout);
  ui_->layout_right->insertWidget(0, playlist_header);
  QObject::connect(playlist_header, &PlaylistTitleHeader::BackClicked, this, [this]() {
    android_playlist_pages_->setCurrentWidget(playlist_list_);
  });
  QObject::connect(playlist_header, &PlaylistTitleHeader::RenameRequested, &*app_->playlist_manager(), &PlaylistManager::Rename);
  const auto update_playlist_header = [this, playlist_header]() {
    Playlist *playlist = app_->playlist_manager()->current();
    playlist_header->SetPlaylist(playlist ? playlist->id() : -1, playlist ? app_->playlist_manager()->GetPlaylistName(playlist->id()) : QString());
  };
  update_playlist_header();
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentChanged, this, update_playlist_header);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::PlaylistRenamed, this, update_playlist_header);
  // FancyTabWidget wraps android_playlist_pages_ in its own page, so compare tab indexes from IndexOfTab() instead of widgets.
  QObject::connect(ui_->tabs->tabBar(), &QTabBar::tabBarClicked, this, [this](const int index) {
    if (index == ui_->tabs->IndexOfTab(android_playlist_pages_)) android_playlist_pages_->setCurrentWidget(playlist_list_);
  });
  QObject::connect(playlist_list_, &PlaylistListContainer::PlaylistActivated, this, [this]() {
    android_playlist_pages_->setCurrentWidget(ui_->playlist_layout);
  });
  QObject::connect(ui_->action_new_playlist, &QAction::triggered, this, [this]() {
    ui_->tabs->setCurrentIndex(ui_->tabs->IndexOfTab(android_playlist_pages_));
    android_playlist_pages_->setCurrentWidget(ui_->playlist_layout);
  });
  QObject::connect(ui_->tabs, &QTabWidget::currentChanged, this, [this]() {
    if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(android_playlist_pages_)) android_playlist_pages_->setCurrentWidget(playlist_list_);
  });

  ui_->tabs->SetMode(FancyTabWidget::Mode::LargeSidebar);
  ui_->tabs->setContextMenuPolicy(Qt::PreventContextMenu);

  ui_->analyzer->hide();
  ui_->volume->hide();
  ui_->line_volume->hide();
  ui_->playlist_summary->setMinimumWidth(0);
  ui_->playlist_summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  ui_->playlist_summary->setWordWrap(true);
  ui_->layout_status_bar_internal->removeWidget(ui_->track_slider);
  ui_->layout_player_controls_container->insertWidget(0, ui_->track_slider);
  ui_->track_slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  // On Android, rearrange the player controls for mobile layout
  // Create mute button with proper styling to match other buttons
  android_mute_button_ = new QToolButton(ui_->player_controls);
  android_mute_button_->setDefaultAction(ui_->action_mute);
  android_mute_button_->setIconSize(ui_->pause_play_button->iconSize());
  android_mute_button_->setAutoRaise(true);
  ui_->layout_player_controls->addWidget(android_mute_button_);
  UpdateAndroidMuteButton(app_->player()->is_muted());

  // Create love button with proper styling to match other buttons
  auto *love_button = new QToolButton(ui_->player_controls);
  love_button->setDefaultAction(ui_->action_love);
  love_button->setIconSize(ui_->pause_play_button->iconSize());
  love_button->setAccessibleName(tr("Love"));
  love_button->setAutoRaise(true);
  ui_->layout_player_controls->addWidget(love_button);

  // Menu bar actions are reachable through the Tools and Settings sidebar tabs and the playlist editor on Android.
  ui_->menuBar->hide();
  ui_->action_transcoder->setVisible(false);
  ui_->action_add_files_to_transcoder->setVisible(false);

  for (QToolButton *button : {ui_->back_button, ui_->pause_play_button, ui_->stop_button, ui_->forward_button, love_button, android_mute_button_}) {
    button->setMinimumSize(48, 48);
  }

}

QWidget *MainWindow::CreateAndroidToolsPage() {

  // The Tools tab shows the list of tools, or one tool opened from it as a subpage.
  android_tools_pages_ = new QStackedWidget(ui_->tabs);
  QScrollArea *page = new QScrollArea(android_tools_pages_);
  android_tools_list_ = page;
  android_tools_pages_->addWidget(page);
  page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  page->setWidgetResizable(true);
  page->setFrameShape(QFrame::NoFrame);

  QWidget *contents = new QWidget(page);
  auto *layout = new QVBoxLayout(contents);
  layout->setSpacing(8);
  page->setWidget(contents);

  const auto add_section = [this, contents, layout](const QString &title, const QList<QAction*> &actions) {
    QLabel *heading = new QLabel(title, contents);
    QFont heading_font = heading->font();
    heading_font.setBold(true);
    heading->setFont(heading_font);
    heading->setWordWrap(true);
    layout->addWidget(heading);
    for (QAction *action : actions) {
      QString button_text = action->text();
      if (action == ui_->action_update_collection) button_text = tr("Update collection");
      else if (action == ui_->action_full_collection_scan) button_text = tr("Full collection scan");
      else if (action == ui_->action_stop_collection_scan) button_text = tr("Stop scan");

      QToolButton *button = new QToolButton(contents);
      button->setDefaultAction(action);
      button->setText(button_text);
      button->setAccessibleName(action->text());
      if (button_text != action->text()) {
        QObject::connect(action, &QAction::changed, button, [button, button_text]() {
          button->setText(button_text);
        });
      }
      button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      button->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
      AndroidWidgets::StyleNavigationButton(button, kAndroidToolButtonHeight);
      layout->addWidget(button);
    }
  };

  add_section(tr("Tools"), { ui_->action_cover_manager, ui_->action_equalizer });
  add_section(tr("Collection"), { ui_->action_update_collection, ui_->action_full_collection_scan, ui_->action_stop_collection_scan });
  layout->addStretch();

  // Tapping the Tools tab returns to the list, like the Playlists tab.
  QObject::connect(ui_->tabs->tabBar(), &QTabBar::tabBarClicked, this, [this](const int index) {
    if (index == ui_->tabs->IndexOfTab(android_tools_pages_) && android_current_tool_) android_current_tool_->close();
  });

  return android_tools_pages_;

}

void MainWindow::ShowAndroidTool(QWidget *tool, const bool add_header) {

  if (android_current_tool_ && android_current_tool_ != tool) {
    android_current_tool_->close();
    // The tool refused to close, for example to keep a running search.
    if (android_current_tool_) return;
  }

  // Embed the tool in the Tools tab instead of showing it as a window over the tab bar.
  QWidget *page = tool->parentWidget();
  if (!page || android_tools_pages_->indexOf(page) < 0) {
    page = new QWidget(android_tools_pages_);
    auto *layout = new QVBoxLayout(page);
    if (add_header) {
      QLabel *title = new QLabel(tr("Tools > %1").arg(tool->windowTitle()), page);
      title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
      QPushButton *back = AndroidWidgets::CreateBackButton(page);
      QObject::connect(back, &QPushButton::clicked, tool, &QWidget::close);
      layout->addLayout(AndroidWidgets::CreatePageHeader(title, back));
    }
    else {
      layout->setContentsMargins(0, 0, 0, 0);
    }
    tool->setParent(page, Qt::Widget);
    tool->setWindowModality(Qt::NonModal);
    tool->setMinimumSize(0, 0);
    tool->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    layout->addWidget(tool);
    android_tools_pages_->addWidget(page);
    new AndroidToolPageFilter(page, tool, [this, tool]() {
      if (android_current_tool_ != tool) return;
      android_current_tool_.clear();
      android_tools_pages_->setCurrentWidget(android_tools_list_);
    });
  }

  android_current_tool_ = tool;
  ui_->tabs->setCurrentIndex(ui_->tabs->IndexOfTab(android_tools_pages_));
  android_tools_pages_->setCurrentWidget(page);
  tool->show();
  tool->setFocus(Qt::OtherFocusReason);

}

QWidget *MainWindow::CreateAndroidSettingsTab() {

  android_settings_tab_ = new QWidget(ui_->tabs);
  auto *settings_layout = new QVBoxLayout(android_settings_tab_);
  settings_layout->setContentsMargins(0, 0, 0, 0);
  settings_layout->setSpacing(0);
  return android_settings_tab_;

}

void MainWindow::SetupAndroidSettingsTab() {

  // Selecting the Settings tab opens the settings dialog in it; closing the dialog returns to the tab that was open before.
  const int settings_tab_index = ui_->tabs->IndexOfTab(android_settings_tab_);
  android_previous_tab_index_ = ui_->tabs->currentIndex();
  if (android_previous_tab_index_ == settings_tab_index) android_previous_tab_index_ = ui_->tabs->IndexOfTab(collection_view_);
  QObject::connect(ui_->tabs, &FancyTabWidget::CurrentTabChanged, this, [this, settings_tab_index](const int index) {
    if (index == settings_tab_index) {
      OpenSettingsDialog();
    }
    else {
      android_previous_tab_index_ = index;
    }
  });

}

void MainWindow::EmbedAndroidSettingsDialog(SettingsDialog *settings_dialog) {

  settings_dialog->setWindowFlags(Qt::Widget);
  settings_dialog->setMinimumSize(0, 0);
  settings_dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  android_settings_tab_->layout()->addWidget(settings_dialog);
  QObject::connect(settings_dialog, &QDialog::finished, this, [this]() {
    android_settings_overview_on_open_ = true;
    if (android_previous_tab_index_ >= 0) ui_->tabs->setCurrentIndex(android_previous_tab_index_);
  });
  settings_dialog->AddAndroidHelpCategory({ ui_->action_about_strawberry, ui_->action_about_qt });

}

void MainWindow::ShowAndroidSettingsDialog() {

  const int settings_tab_index = ui_->tabs->IndexOfTab(android_settings_tab_);
  if (ui_->tabs->currentIndex() != settings_tab_index) {
    // Selecting the tab calls OpenSettingsDialog() again through SetupAndroidSettingsTab().
    ui_->tabs->setCurrentIndex(settings_tab_index);
    return;
  }
  if (android_settings_overview_on_open_) settings_dialog_->OpenOverview();
  else settings_dialog_->show();
  android_settings_overview_on_open_ = false;
  settings_dialog_->raise();

}

void MainWindow::UpdateAndroidMuteButton(const bool mute) {

  // The button is created in SetupAndroidLayout(), after the first MuteChanged() call.
  if (!android_mute_button_) return;

  ui_->action_mute->setIcon(style()->standardIcon(mute ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
  android_mute_button_->setAccessibleName(mute ? tr("Unmute") : tr("Mute"));

}

bool MainWindow::AndroidBack() {

  // Like the header's Back button, return from an open playlist to the playlist list.
  if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(android_playlist_pages_) && android_playlist_pages_->currentWidget() != playlist_list_) {
    android_playlist_pages_->setCurrentWidget(playlist_list_);
    return true;
  }

  return false;

}
