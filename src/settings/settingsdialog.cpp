/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <algorithm>

#include <QtGlobal>
#include <QDialog>
#include <QWidget>
#include <QFont>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QScreen>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QTreeWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QPalette>
#include <QPainter>
#include <QFrame>
#include <QKeySequence>
#include <QRect>
#include <QSize>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLayout>
#include <QStackedWidget>
#include <QSettings>
#include <QShowEvent>
#include <QCloseEvent>
#ifdef Q_OS_ANDROID
#  include <QAction>
#  include <QBackingStore>
#  include <QEvent>
#  include <QKeyEvent>
#  include <QRadioButton>
#  include <QScrollBar>
#  include <QTimer>
#endif

#include "core/settings.h"
#include "core/player.h"
#include "core/appearance.h"
#include "core/iconloader.h"
#include "utilities/screenutils.h"
#include "widgets/groupediconview.h"
#include "collection/collectionmanager.h"
#include "collection/collectionmodel.h"
#include "streaming/streamingservices.h"

#include "settingsdialog.h"
#include "settingspage.h"
#include "androidsettingspage.h"
#include "settingsitemdelegate.h"
#include "behavioursettingspage.h"
#include "collectionsettingspage.h"
#include "backendsettingspage.h"
#include "playlistsettingspage.h"
#include "scrobblersettingspage.h"
#include "coverssettingspage.h"
#include "lyricssettingspage.h"
#include "transcodersettingspage.h"
#include "networkproxysettingspage.h"
#include "appearancesettingspage.h"
#include "contextsettingspage.h"
#include "notificationssettingspage.h"
#include "globalshortcutssettingspage.h"
#ifdef HAVE_TAGFETCHER
#  include "acoustidsettingspage.h"
#endif
#ifdef HAVE_MOODBAR
#  include "moodbarsettingspage.h"
#endif
#ifdef HAVE_WAVEFORM
#  include "waveformsettingspage.h"
#endif
#ifdef HAVE_SUBSONIC
#  include "subsonic/subsonicservice.h"
#  include "subsonicsettingspage.h"
#endif
#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "tidalsettingspage.h"
#endif
#ifdef HAVE_SPOTIFY
#  include "spotify/spotifyservice.h"
#  include "spotifysettingspage.h"
#endif
#ifdef HAVE_QOBUZ
#  include "qobuz/qobuzservice.h"
#  include "qobuzsettingspage.h"
#endif
#ifdef HAVE_PLEX
#  include "plex/plexservice.h"
#  include "plexsettingspage.h"
#endif
#ifdef HAVE_JELLYFIN
#  include "jellyfin/jellyfinservice.h"
#  include "jellyfinsettingspage.h"
#endif

#include "radiosettingspage.h"

#include "ui_settingsdialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "SettingsDialog";
constexpr char kGeometry[] = "geometry";
}

SettingsDialog::SettingsDialog(const SharedPtr<Player> player,
                               const SharedPtr<AudioDeviceListers> audio_device_listers,
                               const SharedPtr<CollectionManager> collection,
                               const SharedPtr<CoverProviders> cover_providers,
                               const SharedPtr<LyricsProviders> lyrics_providers,
                               const SharedPtr<AudioScrobbler> scrobbler,
                               const SharedPtr<StreamingServices> streaming_services,
                               const SharedPtr<Appearance> appearance,
#ifdef HAVE_GLOBALSHORTCUTS
                               GlobalShortcutsManager *global_shortcuts_manager,
#endif
                               OSDBase *osd,
                               QMainWindow *mainwindow,
                               QWidget *parent)
    : QDialog(parent),
      mainwindow_(mainwindow),
      ui_(new Ui_SettingsDialog),
      loading_settings_(false)
#ifdef Q_OS_ANDROID
      , android_navigation_(nullptr),
      android_categories_page_(nullptr),
      android_current_category_page_(nullptr),
      android_settings_loaded_(false)
#endif
      {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new SettingsItemDelegate(this));

  QTreeWidgetItem *general = AddCategory(tr("General"));
  AddPage(Page::Behaviour, new BehaviourSettingsPage(this, this), general);
  AddPage(Page::Collection, new CollectionSettingsPage(this, collection, collection->backend(), collection->model(), collection->model()->directory_model(), this), general);
  AddPage(Page::Backend, new BackendSettingsPage(this, player, audio_device_listers, this), general);
  AddPage(Page::Playlist, new PlaylistSettingsPage(this, this), general);
  AddPage(Page::Scrobbler, new ScrobblerSettingsPage(this, scrobbler, this), general);
  AddPage(Page::Covers, new CoversSettingsPage(this, cover_providers, this), general);
  AddPage(Page::Lyrics, new LyricsSettingsPage(this, lyrics_providers, this), general);
#ifdef HAVE_TAGFETCHER
  AddPage(Page::Acoustid, new AcoustidSettingsPage(this, this), general);
#endif
  AddPage(Page::Transcoding, new TranscoderSettingsPage(this, this), general);
  AddPage(Page::Proxy, new NetworkProxySettingsPage(this, this), general);

  QTreeWidgetItem *iface = AddCategory(tr("User interface"));
  AddPage(Page::Appearance, new AppearanceSettingsPage(this, appearance, this), iface);
  AddPage(Page::Context, new ContextSettingsPage(this, this), iface);
  AddPage(Page::Notifications, new NotificationsSettingsPage(this, osd, this), iface);

#ifdef HAVE_GLOBALSHORTCUTS
  AddPage(Page::GlobalShortcuts, new GlobalShortcutsSettingsPage(this, global_shortcuts_manager, this), iface);
#endif

#ifdef HAVE_MOODBAR
  AddPage(Page::Moodbar, new MoodbarSettingsPage(this, this), iface);
#endif
#ifdef HAVE_WAVEFORM
  AddPage(Page::Waveform, new WaveformSettingsPage(this, this), iface);
#endif

  QTreeWidgetItem *streaming = AddCategory(tr("Streaming"));

  (void)streaming_services;
#ifdef HAVE_SUBSONIC
  AddPage(Page::Subsonic, new SubsonicSettingsPage(this, streaming_services->Service<SubsonicService>(), this), streaming);
#endif
#ifdef HAVE_TIDAL
  AddPage(Page::Tidal, new TidalSettingsPage(this, streaming_services->Service<TidalService>(), this), streaming);
#endif
#ifdef HAVE_SPOTIFY
  AddPage(Page::Spotify, new SpotifySettingsPage(this, streaming_services->Service<SpotifyService>(), this), streaming);
#endif
#ifdef HAVE_QOBUZ
  AddPage(Page::Qobuz, new QobuzSettingsPage(this, streaming_services->Service<QobuzService>(), this), streaming);
#endif
#ifdef HAVE_PLEX
  AddPage(Page::Plex, new PlexSettingsPage(this, streaming_services->Service<PlexService>(), this), streaming);
#endif
#ifdef HAVE_JELLYFIN
  AddPage(Page::Jellyfin, new JellyfinSettingsPage(this, streaming_services->Service<JellyfinService>(), this), streaming);
#endif

  AddPage(Page::Radio, new RadioSettingsPage(this, this), streaming);

#ifdef Q_OS_ANDROID
  setAutoFillBackground(true);
  android_navigation_ = new QStackedWidget(this);
  android_navigation_->setMinimumWidth(0);
  android_navigation_->setBackgroundRole(QPalette::Window);
  android_navigation_->setAutoFillBackground(true);
  android_categories_page_ = new QWidget(android_navigation_);
  android_categories_page_->setBackgroundRole(QPalette::Window);
  android_categories_page_->setAutoFillBackground(true);
  auto *categories_layout = new QVBoxLayout(android_categories_page_);
  categories_layout->setContentsMargins(0, 0, 0, 0);
  categories_layout->setSpacing(8);
  android_navigation_->addWidget(android_categories_page_);
  const QStringList category_icon_names = {u"strawberry"_s, u"view-choose"_s, u"applications-internet"_s};
  const auto style_navigation_button = [](QPushButton *button) {
    QFont font = button->font();
    font.setPointSizeF(font.pointSizeF() * 1.15);
    button->setFont(font);
    button->setStyleSheet(u"text-align: left; padding-left: 24px;"_s);
  };

  QTreeWidgetItem *root = ui_->list->invisibleRootItem();
  for (int i = 0; i < root->childCount(); ++i) {
    QTreeWidgetItem *category = root->child(i);
    QScrollArea *category_page = new QScrollArea(android_navigation_);
    category_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    category_page->setWidgetResizable(true);
    category_page->setFrameShape(QFrame::NoFrame);
    category_page->setBackgroundRole(QPalette::Window);
    category_page->setAutoFillBackground(true);
    category_page->viewport()->setBackgroundRole(QPalette::Window);
    category_page->viewport()->setAutoFillBackground(true);
    QWidget *category_contents = new QWidget(category_page);
    category_contents->setBackgroundRole(QPalette::Window);
    category_contents->setAutoFillBackground(true);
    auto *category_layout = new QVBoxLayout(category_contents);
    category_layout->setContentsMargins(0, 0, 0, 0);
    category_layout->setSpacing(8);
    category_page->setWidget(category_contents);

    QPushButton *category_button = new QPushButton(category->text(0), android_categories_page_);
    if (i < category_icon_names.size()) {
      category_button->setIcon(IconLoader::Load(category_icon_names.at(i)));
      category_button->setIconSize(QSize(32, 32));
    }
    category_button->setMinimumHeight(64);
    category_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    style_navigation_button(category_button);
    categories_layout->addWidget(category_button);
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
      page_button->setIconSize(QSize(32, 32));
      page_button->setMinimumHeight(64);
      page_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      style_navigation_button(page_button);
      category_layout->addWidget(page_button);
      android_category_pages_.insert(page_it.key(), category_page);
      QObject::connect(page_button, &QPushButton::clicked, this, [this, item, category_page]() {
        android_current_category_page_ = category_page;
        ui_->list->setCurrentItem(item);
        CurrentItemChanged(item);
        SetAndroidNavigationPage(ui_->stacked_widget);
        ui_->buttonBox->show();
      });
    }

    category_layout->addStretch();
    android_navigation_->addWidget(category_page);
  }
  categories_layout->addStretch();

  auto *header = new QHBoxLayout;
  QPushButton *back_button = new QPushButton(tr("Back"), this);
  back_button->setIcon(IconLoader::Load(u"go-previous"_s));
  back_button->setIconSize(QSize(32, 32));
  back_button->setMinimumHeight(48);
  QFont title_font = ui_->title->font();
  title_font.setPointSize(18);
  title_font.setBold(true);
  ui_->title->setFont(title_font);
  ui_->title->setWordWrap(true);
  header->addWidget(ui_->title, 1);
  header->addWidget(back_button);
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
  ui_->stacked_widget->setBackgroundRole(QPalette::Window);
  ui_->stacked_widget->setAutoFillBackground(true);
  ui_->buttonBox->setMinimumWidth(0);
  ui_->buttonBox->hide();
  setMinimumSize(0, 0);
  ui_->layout_dialog->setSizeConstraint(QLayout::SetNoConstraint);

  QObject::connect(back_button, &QPushButton::clicked, this, &SettingsDialog::AndroidBack);
#endif

  // List box
  QObject::connect(ui_->list, &QTreeWidget::currentItemChanged, this, &SettingsDialog::CurrentItemChanged);
  ui_->list->setCurrentItem(pages_.value(Page::Behaviour).item_);
#ifdef Q_OS_ANDROID
  SetAndroidTitle();
  SetAndroidNavigationPage(android_categories_page_);
  ui_->buttonBox->hide();
#endif

  // Make sure the list is big enough to show all the items
#ifndef Q_OS_ANDROID
  ui_->list->setMinimumWidth(qobject_cast<QAbstractItemView*>(ui_->list)->sizeHintForColumn(0));  // clazy:exclude=unneeded-cast
#endif

  ui_->buttonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence::Close);

  QObject::connect(ui_->buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::DialogButtonClicked);

}

SettingsDialog::~SettingsDialog() {
  delete ui_;
}

#ifdef Q_OS_ANDROID
bool SettingsDialog::event(QEvent *event) {

  const bool handled = QDialog::event(event);
  if (event->type() == QEvent::UpdateRequest && isVisible()) {
    if (QBackingStore *store = backingStore()) {
      // Qt's Android RHI backing store scales partial flush regions twice on high-DPI screens.
      store->flush(rect());
    }
  }
  return handled;

}
#endif

void SettingsDialog::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
#ifdef Q_OS_ANDROID
    if (!android_settings_loaded_) {
#endif
    LoadGeometry();
    // Load settings
    loading_settings_ = true;
    const QList<PageData> pages = pages_.values();
    for (const PageData &page : pages) {
      page.page_->Load();
    }
    loading_settings_ = false;
  #ifdef Q_OS_ANDROID
    android_settings_loaded_ = true;
    }
  #endif
  }

  QDialog::showEvent(e);

}

void SettingsDialog::closeEvent(QCloseEvent *e) {

  Q_UNUSED(e)

  SaveGeometry();

}

void SettingsDialog::accept() {

#ifdef Q_OS_ANDROID
  while (!android_subpages_.isEmpty()) CloseAndroidSubpage();
#endif
  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    page.page_->Accept();
  }
  Q_EMIT ReloadSettings();

#ifdef Q_OS_ANDROID
  android_settings_loaded_ = false;
#endif
  SaveGeometry();

  QDialog::accept();

}

void SettingsDialog::reject() {

#ifdef Q_OS_ANDROID
  while (!android_subpages_.isEmpty()) CloseAndroidSubpage();
#endif
  // Notify each page that user clicks on Cancel
  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    page.page_->Reject();
  }
#ifdef Q_OS_ANDROID
  android_settings_loaded_ = false;
#endif
  SaveGeometry();

  QDialog::reject();

}

void SettingsDialog::LoadGeometry() {

#ifdef Q_OS_ANDROID
  if (parentWidget() != mainwindow_) return;
  setGeometry(mainwindow_->geometry());
  return;
#else
  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  if (s.contains(kGeometry)) {
    restoreGeometry(s.value(kGeometry).toByteArray());
  }
  s.endGroup();

  // Center the dialog on the same screen as mainwindow.
  Utilities::CenterWidgetOnScreen(Utilities::GetScreen(mainwindow_), this);
#endif

}

void SettingsDialog::SaveGeometry() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  s.setValue(kGeometry, saveGeometry());
  s.endGroup();

}

QTreeWidgetItem *SettingsDialog::AddCategory(const QString &name) {

  QTreeWidgetItem *item = new QTreeWidgetItem;
  item->setText(0, name);
  item->setData(0, Role_IsSeparator, true);
  item->setFlags(Qt::ItemIsEnabled);

  ui_->list->invisibleRootItem()->addChild(item);
  item->setExpanded(true);

  return item;

}

void SettingsDialog::AddPage(const Page id, SettingsPage *page, QTreeWidgetItem *parent) {

#ifdef Q_OS_ANDROID
  if (id != Page::Behaviour) AndroidSettingsPage::Adapt(page);
#endif
  if (!parent) parent = ui_->list->invisibleRootItem();

  // Connect page's signals to the settings dialog's signals
  QObject::connect(page, &SettingsPage::NotificationPreview, this, &SettingsDialog::NotificationPreview);

  // Create the list item
  QTreeWidgetItem *item = new QTreeWidgetItem;
  item->setText(0, page->windowTitle());
  item->setIcon(0, page->windowIcon());
  item->setData(0, Role_IsSeparator, false);

  if (!page->IsEnabled()) {
    item->setFlags(Qt::NoItemFlags);
  }

  parent->addChild(item);

  // Create a scroll area containing the page
  QScrollArea *area = new QScrollArea;
#ifdef Q_OS_ANDROID
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  area->setMinimumWidth(0);
  page->setMinimumWidth(0);
#else
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
  area->setWidget(page);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
#ifdef Q_OS_ANDROID
  area->setBackgroundRole(QPalette::Window);
  area->setAutoFillBackground(true);
  area->viewport()->setBackgroundRole(QPalette::Window);
  area->viewport()->setAutoFillBackground(true);
#endif
#ifndef Q_OS_ANDROID
  area->setMinimumWidth(page->layout()->minimumSize().width());
#endif

  // Add the page to the stack
  ui_->stacked_widget->addWidget(area);

  // Remember where the page is
  PageData page_data;
  page_data.item_ = item;
  page_data.scroll_area_ = area;
  page_data.page_ = page;
  pages_[id] = page_data;

}

void SettingsDialog::Save() {

  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    page.page_->Apply();
  }
  Q_EMIT ReloadSettings();

}


void SettingsDialog::DialogButtonClicked(QAbstractButton *button) {

  // While we only connect Apply at the moment, this might change in the future
  if (ui_->buttonBox->button(QDialogButtonBox::Apply) == button) {
    const QList<PageData> pages = pages_.values();
    for (const PageData &page : pages) {
      page.page_->Apply();
    }
    Q_EMIT ReloadSettings();
  }

}

void SettingsDialog::OpenAtPage(const Page page) {

  if (!pages_.contains(page)) {
    return;
  }

#ifdef Q_OS_ANDROID
  while (!android_subpages_.isEmpty()) CloseAndroidSubpage();
  android_current_category_page_ = android_category_pages_.value(page, android_categories_page_);
#endif
  ui_->list->setCurrentItem(pages_.value(page).item_);
#ifdef Q_OS_ANDROID
  QTreeWidgetItem *item = pages_.value(page).item_;
  SetAndroidTitle(item->parent()->text(0), item->text(0));
  SetAndroidNavigationPage(ui_->stacked_widget);
  ui_->buttonBox->show();
#endif
  show();

}

#ifdef Q_OS_ANDROID
void SettingsDialog::OpenOverview() {

  while (!android_subpages_.isEmpty()) CloseAndroidSubpage();
  SetAndroidTitle();
  SetAndroidNavigationPage(android_categories_page_);
  show();

}

void SettingsDialog::AddAndroidHelpCategory(const QList<QAction*> &actions) {

  QScrollArea *help_page = new QScrollArea(android_navigation_);
  help_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  help_page->setWidgetResizable(true);
  help_page->setFrameShape(QFrame::NoFrame);
  help_page->setBackgroundRole(QPalette::Window);
  help_page->setAutoFillBackground(true);
  help_page->viewport()->setBackgroundRole(QPalette::Window);
  help_page->viewport()->setAutoFillBackground(true);
  QWidget *help_contents = new QWidget(help_page);
  help_contents->setBackgroundRole(QPalette::Window);
  help_contents->setAutoFillBackground(true);
  auto *help_layout = new QVBoxLayout(help_contents);
  help_layout->setContentsMargins(0, 0, 0, 0);
  help_layout->setSpacing(8);
  help_page->setWidget(help_contents);

  for (QAction *action : actions) {
    QPushButton *button = new QPushButton(action->icon(), action->iconText(), help_contents);
    button->setMinimumHeight(56);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont font = button->font();
    font.setPointSizeF(font.pointSizeF() * 1.15);
    button->setFont(font);
    button->setStyleSheet(u"text-align: left; padding-left: 24px;"_s);
    help_layout->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
  }
  help_layout->addStretch();
  android_navigation_->addWidget(help_page);

  QPushButton *help_button = new QPushButton(IconLoader::Load(u"help-hint"_s), tr("Help"), android_categories_page_);
  help_button->setIconSize(QSize(32, 32));
  help_button->setMinimumHeight(64);
  help_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QFont help_font = help_button->font();
  help_font.setPointSizeF(help_font.pointSizeF() * 1.15);
  help_button->setFont(help_font);
  help_button->setStyleSheet(u"text-align: left; padding-left: 24px;"_s);
  // Insert above the trailing stretch of the overview.
  auto *categories_layout = qobject_cast<QVBoxLayout*>(android_categories_page_->layout());
  categories_layout->insertWidget(categories_layout->count() - 1, help_button);
  QObject::connect(help_button, &QPushButton::clicked, this, [this, help_page]() {
    android_current_category_page_ = help_page;
    SetAndroidTitle(tr("Help"));
    SetAndroidNavigationPage(help_page);
    ui_->buttonBox->hide();
  });

  SetAndroidNavigationPage(android_navigation_->currentWidget());

}
#endif

void SettingsDialog::CurrentItemChanged(QTreeWidgetItem *item) {

  if (!item) return;

  if (!(item->flags() & Qt::ItemIsSelectable)) {
    return;
  }

  // Set the title
#ifdef Q_OS_ANDROID
  QTreeWidgetItem *category = item->parent();
  SetAndroidTitle(category ? category->text(0) : QString(), category ? item->text(0) : QString());
#else
  ui_->title->setText(QStringLiteral("<b>") + item->text(0) + u"</b>"_s);
#endif

  // Display the right page
  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    if (page.item_ == item) {
      ui_->stacked_widget->setCurrentWidget(page.scroll_area_);
      break;
    }
  }

}

#ifdef Q_OS_ANDROID
bool SettingsDialog::IsAndroidPageActive(const SettingsPage *page) const {

  if (!isVisible() || android_subpages_.isEmpty()) return false;
  for (const PageData &data : pages_) {
    if (data.page_ == page) return data.item_ == ui_->list->currentItem();
  }
  return false;

}

void SettingsDialog::OpenAndroidSubpage(QWidget *content, const QString &title) {

  auto *parent_area = android_subpages_.isEmpty() ? qobject_cast<QScrollArea*>(ui_->stacked_widget->currentWidget()) : android_subpages_.last().area;
  auto *area = new QScrollArea;
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
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
  const AndroidSubpage subpage = android_subpages_.takeLast();
  // Move focus before takeWidget() clears QScrollArea's internal widget pointer.
  subpage.area->widget()->hide();
  QWidget *content = subpage.area->takeWidget();
  content->setParent(subpage.content_parent);
  content->hide();
  android_navigation_->removeWidget(subpage.area);
  subpage.area->deleteLater();
  if (android_subpages_.isEmpty()) {
    CurrentItemChanged(ui_->list->currentItem());
    ui_->buttonBox->show();
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
  page->repaint();
  android_navigation_->repaint();
  repaint();

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
#endif
