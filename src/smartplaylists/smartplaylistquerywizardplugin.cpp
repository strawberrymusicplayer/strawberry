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

#include <algorithm>
#include <utility>
#include <memory>

#include <QWizardPage>
#include <QList>
#include <QString>
#include <QVBoxLayout>
#include <QScrollBar>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "playlistquerygenerator.h"
#include "smartplaylistquerywizardplugin.h"
#include "smartplaylistquerywizardpluginsearchpage.h"
#include "smartplaylistquerywizardpluginsortpage.h"
#include "smartplaylistsearchtermwidget.h"
#include "ui_smartplaylistquerysearchpage.h"
#include "ui_smartplaylistquerysortpage.h"

using std::make_unique;
using std::make_shared;

SmartPlaylistQueryWizardPlugin::SmartPlaylistQueryWizardPlugin(const SharedPtr<Player> player,
                                                               const SharedPtr<PlaylistManager> playlist_manager,
                                                               const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                                                               const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                                                               const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                                               QObject *parent)
    : SmartPlaylistWizardPlugin(collection_backend, parent),
      player_(player),
      playlist_manager_(playlist_manager),
      collection_backend_(collection_backend),
#ifdef HAVE_MOODBAR
      moodbar_loader_(moodbar_loader),
#endif
      current_albumcover_loader_(current_albumcover_loader),
      search_page_(nullptr),
      previous_scrollarea_max_(0) {}

SmartPlaylistQueryWizardPlugin::~SmartPlaylistQueryWizardPlugin() = default;

QString SmartPlaylistQueryWizardPlugin::name() const { return tr("Collection search"); }

QString SmartPlaylistQueryWizardPlugin::description() const {
  return tr("Find songs in your collection that match the criteria you specify.");
}

int SmartPlaylistQueryWizardPlugin::CreatePages(QWizard *wizard, int finish_page_id) {

  // Create the UI
  search_page_ = new SmartPlaylistQueryWizardPluginSearchPage(wizard);

  QWizardPage *sort_page = new SmartPlaylistQueryWizardPluginSortPage(this, wizard, finish_page_id);
  sort_ui_ = make_unique<Ui_SmartPlaylistQuerySortPage>();
  sort_ui_->setupUi(sort_page);

  sort_ui_->limit_value->setValue(PlaylistGenerator::kDefaultLimit);

  QObject::connect(search_page_->ui_->type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistQueryWizardPlugin::SearchTypeChanged);

  // Create the new search term widget
  search_page_->new_term_ = new SmartPlaylistSearchTermWidget(collection_backend_, search_page_);
  search_page_->new_term_->SetActive(false);
  QObject::connect(search_page_->new_term_, &SmartPlaylistSearchTermWidget::Clicked, this, &SmartPlaylistQueryWizardPlugin::AddSearchTerm);

  // Add an empty initial term
  search_page_->layout_ = static_cast<QVBoxLayout*>(search_page_->ui_->terms_scroll_area_content->layout());
  search_page_->layout_->addWidget(search_page_->new_term_);
  AddSearchTerm();

  // Ensure that the terms are scrolled to the bottom when a new one is added
  QObject::connect(search_page_->ui_->terms_scroll_area->verticalScrollBar(), &QScrollBar::rangeChanged, this, &SmartPlaylistQueryWizardPlugin::MoveTermListToBottom);

  // Add the preview widget at the bottom of the search terms page
  QVBoxLayout *terms_page_layout = static_cast<QVBoxLayout*>(search_page_->layout());
  terms_page_layout->addStretch();
  search_page_->preview_ = new SmartPlaylistSearchPreview(search_page_);
  search_page_->preview_->Init(player_,
                               playlist_manager_,
                               collection_backend_,
#ifdef HAVE_MOODBAR
                               moodbar_loader_,
#endif
                               current_albumcover_loader_);
  terms_page_layout->addWidget(search_page_->preview_);

  // Add sort field texts
  for (int i = 0; i < static_cast<int>(SmartPlaylistSearchTerm::Field::FieldCount); ++i) {
    const SmartPlaylistSearchTerm::Field field = static_cast<SmartPlaylistSearchTerm::Field>(i);
    const QString field_name = SmartPlaylistSearchTerm::FieldName(field);
    sort_ui_->field_value->addItem(field_name);
  }
  QObject::connect(sort_ui_->field_value, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistQueryWizardPlugin::UpdateSortOrder);
  UpdateSortOrder();

  // Set the sort and limit radio buttons back to their defaults - they would
  // have been changed by setupUi
  sort_ui_->random->setChecked(true);
  sort_ui_->limit_none->setChecked(true);

  // Set up the preview widget that's already at the bottom of the sort page
  sort_ui_->preview->Init(player_,
                          playlist_manager_,
                          collection_backend_,
#ifdef HAVE_MOODBAR
                          moodbar_loader_,
#endif
                          current_albumcover_loader_);

  QObject::connect(sort_ui_->field, &QRadioButton::toggled, this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->field_value, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->limit_limit, &QRadioButton::toggled, this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->limit_none, &QRadioButton::toggled, this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->limit_value, QOverload<int>::of(&QSpinBox::valueChanged), this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->order, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);
  QObject::connect(sort_ui_->random, &QRadioButton::toggled, this, &SmartPlaylistQueryWizardPlugin::UpdateSortPreview);

  // Configure the page text
  search_page_->setTitle(tr("Search terms"));
  search_page_->setSubTitle(tr("A song will be included in the playlist if it matches these conditions."));
  sort_page->setTitle(tr("Search options"));
  sort_page->setSubTitle(tr("Choose how the playlist is sorted and how many songs it will contain."));

  // Add the pages
  const int first_page = wizard->addPage(search_page_);
  wizard->addPage(sort_page);
  return first_page;

}

void SmartPlaylistQueryWizardPlugin::SetGenerator(PlaylistGeneratorPtr g) {

  SharedPtr<PlaylistQueryGenerator> gen = std::dynamic_pointer_cast<PlaylistQueryGenerator>(g);
  if (!gen) return;
  SmartPlaylistSearch search = gen->search();

  // Search type
  search_page_->ui_->type->setCurrentIndex(static_cast<int>(search.search_type_));

  // Search terms
  qDeleteAll(search_page_->terms_);
  search_page_->terms_.clear();

  for (const SmartPlaylistSearchTerm &term : std::as_const(search.terms_)) {
    AddSearchTerm();
    search_page_->terms_.constLast()->SetTerm(term);
  }

  // Sort order
  if (search.sort_type_ == SmartPlaylistSearch::SortType::Random) {
    sort_ui_->random->setChecked(true);
  }
  else {
    sort_ui_->field->setChecked(true);
    sort_ui_->order->setCurrentIndex(search.sort_type_ == SmartPlaylistSearch::SortType::FieldAsc ? 0 : 1);
    sort_ui_->field_value->setCurrentIndex(static_cast<int>(search.sort_field_));
  }

  // Limit
  if (search.limit_ == -1) {
    sort_ui_->limit_none->setChecked(true);
  }
  else {
    sort_ui_->limit_limit->setChecked(true);
    sort_ui_->limit_value->setValue(search.limit_);
  }

}

PlaylistGeneratorPtr SmartPlaylistQueryWizardPlugin::CreateGenerator() const {

  SharedPtr<PlaylistQueryGenerator> gen = make_shared<PlaylistQueryGenerator>();
  gen->Load(MakeSearch());

  return std::static_pointer_cast<PlaylistGenerator>(gen);

}

void SmartPlaylistQueryWizardPlugin::UpdateSortOrder() {

  const SmartPlaylistSearchTerm::Field field = static_cast<SmartPlaylistSearchTerm::Field>(sort_ui_->field_value->currentIndex());
  const SmartPlaylistSearchTerm::Type type = SmartPlaylistSearchTerm::TypeOf(field);
  const QString asc = SmartPlaylistSearchTerm::FieldSortOrderText(type, true);
  const QString desc = SmartPlaylistSearchTerm::FieldSortOrderText(type, false);

  const int old_current_index = sort_ui_->order->currentIndex();
  sort_ui_->order->clear();
  sort_ui_->order->addItem(asc);
  sort_ui_->order->addItem(desc);
  sort_ui_->order->setCurrentIndex(old_current_index);

}

void SmartPlaylistQueryWizardPlugin::AddSearchTerm() {

  SmartPlaylistSearchTermWidget *widget = new SmartPlaylistSearchTermWidget(collection_backend_, search_page_);
  QObject::connect(widget, &SmartPlaylistSearchTermWidget::RemoveClicked, this, &SmartPlaylistQueryWizardPlugin::RemoveSearchTerm);
  QObject::connect(widget, &SmartPlaylistSearchTermWidget::Changed, this, &SmartPlaylistQueryWizardPlugin::UpdateTermPreview);

  search_page_->layout_->insertWidget(static_cast<int>(search_page_->terms_.count()), widget);
  search_page_->terms_ << widget;

  UpdateTermPreview();

}

void SmartPlaylistQueryWizardPlugin::RemoveSearchTerm() {

  SmartPlaylistSearchTermWidget *widget = qobject_cast<SmartPlaylistSearchTermWidget*>(sender());
  if (!widget) return;

  const qint64 index = search_page_->terms_.indexOf(widget);
  if (index == -1) return;

  search_page_->terms_.takeAt(index)->deleteLater();
  UpdateTermPreview();

}

void SmartPlaylistQueryWizardPlugin::UpdateTermPreview() {

  SmartPlaylistSearch search = MakeSearch();
  Q_EMIT search_page_->completeChanged();
  // When removing last term, update anyway the search
  if (!search.is_valid() && !search_page_->terms_.isEmpty()) return;

  // Don't apply limits in the term page
  search.limit_ = -1;

  search_page_->preview_->Update(search);

}

void SmartPlaylistQueryWizardPlugin::UpdateSortPreview() {

  SmartPlaylistSearch search = MakeSearch();
  if (!search.is_valid()) return;

  sort_ui_->preview->Update(search);

}

SmartPlaylistSearch SmartPlaylistQueryWizardPlugin::MakeSearch() const {

  SmartPlaylistSearch ret;

  // Search type
  ret.search_type_ = static_cast<SmartPlaylistSearch::SearchType>(search_page_->ui_->type->currentIndex());

  // Search terms
  for (SmartPlaylistSearchTermWidget *widget : std::as_const(search_page_->terms_)) {
    SmartPlaylistSearchTerm term = widget->Term();
    if (term.is_valid()) ret.terms_ << term;
  }

  // Sort order
  if (sort_ui_->random->isChecked()) {
    ret.sort_type_ = SmartPlaylistSearch::SortType::Random;
  }
  else {
    const bool ascending = sort_ui_->order->currentIndex() == 0;
    ret.sort_type_ = ascending ? SmartPlaylistSearch::SortType::FieldAsc : SmartPlaylistSearch::SortType::FieldDesc;
    ret.sort_field_ = static_cast<SmartPlaylistSearchTerm::Field>(sort_ui_->field_value->currentIndex());
  }

  // Limit
  if (sort_ui_->limit_none->isChecked()) {
    ret.limit_ = -1;
  }
  else {
    ret.limit_ = sort_ui_->limit_value->value();
  }

  return ret;

}

void SmartPlaylistQueryWizardPlugin::SearchTypeChanged() {

  const bool all = search_page_->ui_->type->currentIndex() == 2;
  search_page_->ui_->terms_scroll_area_content->setEnabled(!all);

  UpdateTermPreview();

}

void SmartPlaylistQueryWizardPlugin::MoveTermListToBottom(int min, int max) {

  Q_UNUSED(min);
  // Only scroll to the bottom if a new term is added
  if (previous_scrollarea_max_ < max) {
    search_page_->ui_->terms_scroll_area->verticalScrollBar()->setValue(max);
  }

  previous_scrollarea_max_ = max;

}
