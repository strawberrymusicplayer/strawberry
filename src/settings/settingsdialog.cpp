/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QDialog>
#include <QWidget>
#include <QMainWindow>
#include <QScreen>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QTreeWidget>
#include <QList>
#include <QString>
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

#include "core/application.h"
#include "core/player.h"
#include "core/settings.h"
#include "utilities/screenutils.h"
#include "widgets/groupediconview.h"
#include "collection/collectionmodel.h"

#include "settingsdialog.h"
#include "settingspage.h"
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
#ifdef HAVE_MOODBAR
#  include "moodbarsettingspage.h"
#endif
#ifdef HAVE_SUBSONIC
#  include "subsonicsettingspage.h"
#endif
#ifdef HAVE_TIDAL
#  include "tidalsettingspage.h"
#endif
#ifdef HAVE_SPOTIFY
#  include "spotifysettingspage.h"
#endif
#ifdef HAVE_QOBUZ
#  include "qobuzsettingspage.h"
#endif

#include "ui_settingsdialog.h"

const char *SettingsDialog::kSettingsGroup = "SettingsDialog";

SettingsItemDelegate::SettingsItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize SettingsItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  const bool is_separator = idx.data(SettingsDialog::Role_IsSeparator).toBool();
  QSize ret = QStyledItemDelegate::sizeHint(option, idx);

  if (is_separator) {
    ret.setHeight(ret.height() * 2);
  }

  return ret;

}

void SettingsItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  const bool is_separator = idx.data(SettingsDialog::Role_IsSeparator).toBool();

  if (is_separator) {
    GroupedIconView::DrawHeader(painter, option.rect, option.font, option.palette, idx.data().toString());
  }
  else {
    QStyledItemDelegate::paint(painter, option, idx);
  }

}

SettingsDialog::SettingsDialog(Application *app, OSDBase *osd, QMainWindow *mainwindow, QWidget *parent)
    : QDialog(parent),
      mainwindow_(mainwindow),
      app_(app),
      osd_(osd),
      player_(app_->player()),
      engine_(app_->player()->engine()),
      model_(app_->collection_model()->directory_model()),
      manager_(nullptr),
      ui_(new Ui_SettingsDialog),
      loading_settings_(false) {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new SettingsItemDelegate(this));

  QTreeWidgetItem *general = AddCategory(tr("General"));
  AddPage(Page::Behaviour, new BehaviourSettingsPage(this, this), general);
  AddPage(Page::Collection, new CollectionSettingsPage(this, this), general);
  AddPage(Page::Backend, new BackendSettingsPage(this, this), general);
  AddPage(Page::Playlist, new PlaylistSettingsPage(this, this), general);
  AddPage(Page::Scrobbler, new ScrobblerSettingsPage(this, this), general);
  AddPage(Page::Covers, new CoversSettingsPage(this, this), general);
  AddPage(Page::Lyrics, new LyricsSettingsPage(this, this), general);
#ifdef HAVE_GSTREAMER
  AddPage(Page::Transcoding, new TranscoderSettingsPage(this, this), general);
#endif
  AddPage(Page::Proxy, new NetworkProxySettingsPage(this, this), general);

  QTreeWidgetItem *iface = AddCategory(tr("User interface"));
  AddPage(Page::Appearance, new AppearanceSettingsPage(this, this), iface);
  AddPage(Page::Context, new ContextSettingsPage(this, this), iface);
  AddPage(Page::Notifications, new NotificationsSettingsPage(this, this), iface);

#ifdef HAVE_GLOBALSHORTCUTS
  AddPage(Page::GlobalShortcuts, new GlobalShortcutsSettingsPage(this, this), iface);
#endif

#ifdef HAVE_MOODBAR
  AddPage(Page::Moodbar, new MoodbarSettingsPage(this, this), iface);
#endif

#if defined(HAVE_SUBSONIC) || defined(HAVE_TIDAL) || defined(HAVE_SPOTIFY) || defined(HAVE_QOBUZ)
  QTreeWidgetItem *streaming = AddCategory(tr("Streaming"));
#endif

#ifdef HAVE_SUBSONIC
  AddPage(Page::Subsonic, new SubsonicSettingsPage(this, this), streaming);
#endif
#ifdef HAVE_TIDAL
  AddPage(Page::Tidal, new TidalSettingsPage(this, this), streaming);
#endif
#ifdef HAVE_SPOTIFY
  AddPage(Page::Spotify, new SpotifySettingsPage(this, this), streaming);
#endif
#ifdef HAVE_QOBUZ
  AddPage(Page::Qobuz, new QobuzSettingsPage(this, this), streaming);
#endif

  // List box
  QObject::connect(ui_->list, &QTreeWidget::currentItemChanged, this, &SettingsDialog::CurrentItemChanged);
  ui_->list->setCurrentItem(pages_[Page::Behaviour].item_);

  // Make sure the list is big enough to show all the items
  ui_->list->setMinimumWidth(qobject_cast<QAbstractItemView*>(ui_->list)->sizeHintForColumn(0));  // clazy:exclude=unneeded-cast

  ui_->buttonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence::Close);

  QObject::connect(ui_->buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::DialogButtonClicked);

}

SettingsDialog::~SettingsDialog() {
  delete ui_;
}

void SettingsDialog::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
    LoadGeometry();
    // Load settings
    loading_settings_ = true;
    const QList<PageData> pages = pages_.values();
    for (const PageData &page : pages) {
      page.page_->Load();
    }
    loading_settings_ = false;
  }

  QDialog::showEvent(e);

}

void SettingsDialog::closeEvent(QCloseEvent*) {

  SaveGeometry();

}

void SettingsDialog::accept() {

  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    page.page_->Accept();
  }
  Q_EMIT ReloadSettings();

  SaveGeometry();

  QDialog::accept();

}

void SettingsDialog::reject() {

  // Notify each page that user clicks on Cancel
  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    page.page_->Reject();
  }
  SaveGeometry();

  QDialog::reject();

}

void SettingsDialog::LoadGeometry() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  if (s.contains("geometry")) {
    restoreGeometry(s.value("geometry").toByteArray());
  }
  s.endGroup();

  // Center the dialog on the same screen as mainwindow.
  Utilities::CenterWidgetOnScreen(Utilities::GetScreen(mainwindow_), this);

}

void SettingsDialog::SaveGeometry() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  s.setValue("geometry", saveGeometry());
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
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setWidget(page);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  area->setMinimumWidth(page->layout()->minimumSize().width());

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

void SettingsDialog::OpenAtPage(Page page) {

  if (!pages_.contains(page)) {
    return;
  }

  ui_->list->setCurrentItem(pages_[page].item_);
  show();

}

void SettingsDialog::CurrentItemChanged(QTreeWidgetItem *item) {

  if (!(item->flags() & Qt::ItemIsSelectable)) {
    return;
  }

  // Set the title
  ui_->title->setText(QStringLiteral("<b>") + item->text(0) + QStringLiteral("</b>"));

  // Display the right page
  const QList<PageData> pages = pages_.values();
  for (const PageData &page : pages) {
    if (page.item_ == item) {
      ui_->stacked_widget->setCurrentWidget(page.scroll_area_);
      break;
    }
  }

}
