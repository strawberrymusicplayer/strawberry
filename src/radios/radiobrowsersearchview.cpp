/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#include <QWidget>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QShowEvent>

#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/radiobrowsersettings.h"
#include "widgets/stretchheaderview.h"
#include "radiobrowserservice.h"
#include "radiobrowsersearchview.h"
#include "radiobrowsersearchmodel.h"
#include "radiomimedata.h"
#include "ui_radiobrowsersearchview.h"

using namespace Qt::Literals::StringLiterals;

RadioBrowserSearchView::RadioBrowserSearchView(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_RadioBrowserSearchView),
      service_(nullptr),
      model_(new RadioBrowserSearchModel(this)),
      search_timer_(new QTimer(this)),
      context_menu_(nullptr),
      action_add_to_playlist_(nullptr),
      current_offset_(0),
      search_limit_(100),
      has_more_(false),
      initialized_(false) {

  ui_->setupUi(this);

  ui_->results->setModel(model_);

  StretchHeaderView *header = new StretchHeaderView(Qt::Horizontal, this);
  ui_->results->setHeader(header);
  header->SetStretchEnabled(true);
  header->SetColumnWidth(static_cast<int>(RadioBrowserSearchModel::Column::Name), 0.5);
  header->SetColumnWidth(static_cast<int>(RadioBrowserSearchModel::Column::Country), 0.2);
  header->SetColumnWidth(static_cast<int>(RadioBrowserSearchModel::Column::Tags), 0.2);
  header->SetColumnWidth(static_cast<int>(RadioBrowserSearchModel::Column::Codec), 0.1);

  ui_->search->setPlaceholderText(tr("Search radio stations..."));

  // Country filter - starts with "All countries", populated dynamically after API fetch
  ui_->combo_country->addItem(tr("All countries"), QString());

  // Sort order
  ui_->combo_sort->addItem(tr("By votes"), u"votes"_s);
  ui_->combo_sort->addItem(tr("By clicks"), u"clickcount"_s);
  ui_->combo_sort->addItem(tr("By name"), u"name"_s);
  ui_->combo_sort->addItem(tr("By bitrate"), u"bitrate"_s);

  search_timer_->setSingleShot(true);
  search_timer_->setInterval(300);

  QObject::connect(ui_->search, &SearchField::textChanged, this, &RadioBrowserSearchView::TextChanged);
  QObject::connect(search_timer_, &QTimer::timeout, this, &RadioBrowserSearchView::SearchTriggered);
  QObject::connect(ui_->button_loadmore, &QPushButton::clicked, this, &RadioBrowserSearchView::LoadMore);
  QObject::connect(ui_->combo_country, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioBrowserSearchView::CountryChanged);
  QObject::connect(ui_->combo_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioBrowserSearchView::SortChanged);
  QObject::connect(ui_->results, &QTreeView::doubleClicked, this, &RadioBrowserSearchView::ItemDoubleClicked);
  ui_->results->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui_->results, &QTreeView::customContextMenuRequested, this, &RadioBrowserSearchView::ShowContextMenu);

}

RadioBrowserSearchView::~RadioBrowserSearchView() {

  if (service_) {
    QObject::disconnect(service_, nullptr, this, nullptr);
  }
  delete ui_;

}

void RadioBrowserSearchView::showEvent(QShowEvent *e) {

  Q_UNUSED(e)

  if (!initialized_ && service_) {
    service_->FetchCountries();
    initialized_ = true;
  }

}

void RadioBrowserSearchView::Init(RadioBrowserService *service) {

  service_ = service;
  QObject::connect(service_, &RadioBrowserService::SearchFinished, this, &RadioBrowserSearchView::SearchFinished);
  QObject::connect(service_, &RadioBrowserService::SearchError, this, &RadioBrowserSearchView::SearchError);
  QObject::connect(service_, &RadioBrowserService::CountriesLoaded, this, &RadioBrowserSearchView::CountriesLoaded);

  // Load defaults from settings
  Settings s;
  s.beginGroup(QLatin1String(RadioBrowserSettings::kSettingsGroup));
  search_limit_ = s.value(QLatin1String(RadioBrowserSettings::kSearchLimit), RadioBrowserSettings::kSearchLimitDefault).toInt();

  const QString default_sort = s.value(u"default_sort"_s, u"votes"_s).toString();
  for (int i = 0; i < ui_->combo_sort->count(); ++i) {
    if (ui_->combo_sort->itemData(i).toString() == default_sort) {
      ui_->combo_sort->setCurrentIndex(i);
      break;
    }
  }

  default_country_ = s.value(u"default_country"_s).toString();
  s.endGroup();

}

void RadioBrowserSearchView::TextChanged(const QString &text) {

  Q_UNUSED(text)
  search_timer_->start();

}

void RadioBrowserSearchView::SearchTriggered() {

  current_offset_ = 0;
  model_->Clear();
  DoSearch();

}

void RadioBrowserSearchView::DoSearch() {

  if (!service_) return;

  const QString query = ui_->search->text().trimmed();
  const QString country = ui_->combo_country->currentData().toString();
  const QString order = ui_->combo_sort->currentData().toString();

  ui_->label_status->setText(tr("Searching..."));
  ui_->stacked->setCurrentWidget(ui_->page_results);

  service_->Search(query, country, QString(), QString(), order, search_limit_, current_offset_);

}

void RadioBrowserSearchView::SearchFinished(const RadioChannelList &channels, const bool has_more) {

  has_more_ = has_more;
  ui_->button_loadmore->setVisible(has_more);

  if (channels.isEmpty() && current_offset_ == 0) {
    ui_->label_status->setText(tr("No stations found."));
    return;
  }

  ui_->label_status->setText(tr("%1 stations found").arg(model_->rowCount() + channels.size()));

  model_->AddChannels(channels);

}

void RadioBrowserSearchView::SearchError(const QString &error) {

  ui_->label_status->setText(error);

}

void RadioBrowserSearchView::CountriesLoaded(const QList<QPair<QString, QString>> &countries) {

  ui_->combo_country->clear();
  ui_->combo_country->addItem(tr("All countries"), QString());

  for (const QPair<QString, QString> &entry : countries) {
    ui_->combo_country->addItem(entry.first, entry.second);
  }

  // Restore saved default country
  if (!default_country_.isEmpty()) {
    for (int i = 0; i < ui_->combo_country->count(); ++i) {
      if (ui_->combo_country->itemData(i).toString() == default_country_) {
        ui_->combo_country->setCurrentIndex(i);
        break;
      }
    }
  }

}

void RadioBrowserSearchView::LoadMore() {

  current_offset_ += search_limit_;
  DoSearch();

}

void RadioBrowserSearchView::CountryChanged(const int index) {

  Q_UNUSED(index)

  if (service_) SearchTriggered();

}

void RadioBrowserSearchView::SortChanged(const int index) {

  Q_UNUSED(index)

  if (service_) SearchTriggered();

}

void RadioBrowserSearchView::ItemDoubleClicked(const QModelIndex &index) {

  if (!index.isValid()) return;

  const RadioChannel channel = model_->ChannelForRow(index.row());
  if (channel.url.isEmpty()) return;

  RadioMimeData *mimedata = new RadioMimeData;
  mimedata->songs << channel.ToSong();
  Q_EMIT AddToPlaylist(mimedata);

}

void RadioBrowserSearchView::AddSelectedToPlaylist() {

  const QModelIndexList selected = ui_->results->selectionModel()->selectedRows(static_cast<int>(RadioBrowserSearchModel::Column::Name));
  if (selected.isEmpty()) return;

  RadioMimeData *mimedata = new RadioMimeData;
  for (const QModelIndex &idx : selected) {
    const RadioChannel channel = model_->ChannelForRow(idx.row());
    if (!channel.url.isEmpty()) {
      mimedata->songs << channel.ToSong();
    }
  }
  if (!mimedata->songs.isEmpty()) {
    Q_EMIT AddToPlaylist(mimedata);
  }
  else {
    delete mimedata;
  }

}

void RadioBrowserSearchView::ShowContextMenu(const QPoint &pos) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);

    action_add_to_playlist_ = new QAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this);
    QObject::connect(action_add_to_playlist_, &QAction::triggered, this, &RadioBrowserSearchView::AddSelectedToPlaylist);
    context_menu_->addAction(action_add_to_playlist_);
  }

  const bool has_selection = !ui_->results->selectionModel()->selectedRows().isEmpty();
  action_add_to_playlist_->setEnabled(has_selection);

  context_menu_->popup(ui_->results->viewport()->mapToGlobal(pos));

}
