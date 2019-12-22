/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QDialog>
#include <QWindow>
#include <QScreen>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QTreeWidget>
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

#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "engine/enginebase.h"
#include "widgets/groupediconview.h"
#include "collection/collectionmodel.h"

#include "settingsdialog.h"
#include "settingspage.h"
#include "appearancesettingspage.h"
#include "backendsettingspage.h"
#include "behavioursettingspage.h"
#include "contextsettingspage.h"
#include "collectionsettingspage.h"
#include "notificationssettingspage.h"
#include "playlistsettingspage.h"
#include "shortcutssettingspage.h"
#include "transcodersettingspage.h"
#include "networkproxysettingspage.h"
#include "scrobblersettingspage.h"
#ifdef HAVE_MOODBAR
#  include "moodbarsettingspage.h"
#endif
#ifdef HAVE_TIDAL
#  include "tidalsettingspage.h"
#endif
#ifdef HAVE_QOBUZ
#  include "qobuzsettingspage.h"
#endif
#ifdef HAVE_SUBSONIC
#  include "subsonicsettingspage.h"
#endif

#include "ui_settingsdialog.h"

class QShowEvent;

const char *SettingsDialog::kSettingsGroup = "SettingsDialog";

SettingsItemDelegate::SettingsItemDelegate(QObject *parent)
  : QStyledItemDelegate(parent) {}

QSize SettingsItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {

  const bool is_separator = index.data(SettingsDialog::Role_IsSeparator).toBool();
  QSize ret = QStyledItemDelegate::sizeHint(option, index);

  if (is_separator) {
    ret.setHeight(ret.height() * 2);
  }

  return ret;

}

void SettingsItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  const bool is_separator = index.data(SettingsDialog::Role_IsSeparator).toBool();

  if (is_separator) {
    GroupedIconView::DrawHeader(painter, option.rect, option.font, option.palette, index.data().toString());
  }
  else {
    QStyledItemDelegate::paint(painter, option, index);
  }

}

SettingsDialog::SettingsDialog(Application *app, QWidget *parent)
    : QDialog(parent),
      app_(app),
      player_(app_->player()),
      engine_(app_->player()->engine()),
      model_(app_->collection_model()->directory_model()),
      appearance_(app_->appearance()),
      ui_(new Ui_SettingsDialog),
      loading_settings_(false) {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new SettingsItemDelegate(this));

  QTreeWidgetItem *general = AddCategory(tr("General"));
  AddPage(Page_Behaviour, new BehaviourSettingsPage(this), general);
  AddPage(Page_Collection, new CollectionSettingsPage(this), general);
  AddPage(Page_Backend, new BackendSettingsPage(this), general);
  AddPage(Page_Playlist, new PlaylistSettingsPage(this), general);
  AddPage(Page_Scrobbler, new ScrobblerSettingsPage(this), general);
#ifdef HAVE_GSTREAMER
  AddPage(Page_Transcoding, new TranscoderSettingsPage(this), general);
#endif
  AddPage(Page_Proxy, new NetworkProxySettingsPage(this), general);

  QTreeWidgetItem *iface = AddCategory(tr("User interface"));
  AddPage(Page_Appearance, new AppearanceSettingsPage(this), iface);
  AddPage(Page_Context, new ContextSettingsPage(this), iface);
  AddPage(Page_Notifications, new NotificationsSettingsPage(this), iface);

#ifdef HAVE_GLOBALSHORTCUTS
  AddPage(Page_GlobalShortcuts, new GlobalShortcutsSettingsPage(this), iface);
#endif

#ifdef HAVE_MOODBAR
  AddPage(Page_Moodbar, new MoodbarSettingsPage(this), iface);
#endif

#if defined(HAVE_TIDAL) || defined(HAVE_SUBSONIC) || defined(HAVE_QOBUZ)
  QTreeWidgetItem *streaming = AddCategory(tr("Streaming"));
#endif
#ifdef HAVE_TIDAL
  AddPage(Page_Tidal, new TidalSettingsPage(this), streaming);
#endif
#ifdef HAVE_QOBUZ
  AddPage(Page_Qobuz, new QobuzSettingsPage(this), streaming);
#endif
#ifdef HAVE_SUBSONIC
  AddPage(Page_Subsonic, new SubsonicSettingsPage(this), streaming);
#endif

  // List box
  connect(ui_->list, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), SLOT(CurrentItemChanged(QTreeWidgetItem*)));
  ui_->list->setCurrentItem(pages_[Page_Behaviour].item_);

  // Make sure the list is big enough to show all the items
  ui_->list->setMinimumWidth(static_cast<QAbstractItemView*>(ui_->list)->sizeHintForColumn(0));

  ui_->buttonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence::Close);

  QSettings s;
  s.beginGroup(kSettingsGroup);
  if (s.contains("geometry")) {
    restoreGeometry(s.value("geometry").toByteArray());
  }
  s.endGroup();

}

SettingsDialog::~SettingsDialog() {
  delete ui_;
}

void SettingsDialog::SaveGeometry() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
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

void SettingsDialog::AddPage(Page id, SettingsPage *page, QTreeWidgetItem *parent) {

  if (!parent) parent = ui_->list->invisibleRootItem();

  // Connect page's signals to the settings dialog's signals
  connect(page, SIGNAL(NotificationPreview(OSD::Behaviour, QString, QString)), SIGNAL(NotificationPreview(OSD::Behaviour, QString, QString)));

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
  PageData data;
  data.item_ = item;
  data.scroll_area_ = area;
  data.page_ = page;
  pages_[id] = data;

}

void SettingsDialog::Save() {

  for (const PageData &data : pages_.values()) {
    data.page_->Save();
  }
  emit ReloadSettings();

}

void SettingsDialog::accept() {
  Save();
  SaveGeometry();
  QDialog::accept();
}

void SettingsDialog::reject() {

  // Notify each page that user clicks on Cancel
  for (const PageData &data : pages_.values()) {
    data.page_->Cancel();
  }
  SaveGeometry();

  QDialog::reject();
}

void SettingsDialog::DialogButtonClicked(QAbstractButton *button) {

  // While we only connect Apply at the moment, this might change in the future
  if (ui_->buttonBox->button(QDialogButtonBox::Apply) == button) {
    Save();
  }
}

void SettingsDialog::showEvent(QShowEvent *e) {

  // Load settings
  loading_settings_ = true;
  for (const PageData &data : pages_.values()) {
    data.page_->Load();
  }
  loading_settings_ = false;

  // Resize the dialog if it's too big
  const QRect available = window()->windowHandle()->screen()->geometry();
  if (available.height() < height()) {
    resize(width(), sizeHint().height());
  }

  QDialog::showEvent(e);

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
  ui_->title->setText("<b>" + item->text(0) + "</b>");

  // Display the right page
  for (const PageData &data : pages_.values()) {
    if (data.item_ == item) {
      ui_->stacked_widget->setCurrentWidget(data.scroll_area_);
      break;
    }
  }

}

void SettingsDialog::ComboBoxLoadFromSettings(const QSettings &s, QComboBox *combobox, const QString &setting, const QString &default_value) {

  QString value = s.value(setting, default_value).toString();
  int i = combobox->findData(value);
  if (i == -1) i = combobox->findData(default_value);
  combobox->setCurrentIndex(i);

}

void SettingsDialog::ComboBoxLoadFromSettings(const QSettings &s, QComboBox *combobox, const QString &setting, const int default_value) {

  int value = s.value(setting, default_value).toInt();
  int i = combobox->findData(value);
  if (i == -1) i = combobox->findData(default_value);
  combobox->setCurrentIndex(i);

}
