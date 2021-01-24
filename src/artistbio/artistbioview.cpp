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

#include <QWidget>
#include <QFile>
#include <QScrollArea>
#include <QSettings>
#include <QSpacerItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QShowEvent>

#include "core/song.h"
#include "core/networkaccessmanager.h"
#include "widgets/prettyimageview.h"
#include "widgets/widgetfadehelper.h"
#include "artistbiofetcher.h"
#include "lastfmartistbio.h"
#include "wikipediaartistbio.h"

#include "artistbioview.h"

const char *ArtistBioView::kSettingsGroup = "ArtistBio";

ArtistBioView::ArtistBioView(QWidget *parent)
    : QWidget(parent),
      network_(new NetworkAccessManager(this)),
      fetcher_(new ArtistBioFetcher(this)),
      current_request_id_(-1),
      container_(new QVBoxLayout),
      section_container_(nullptr),
      fader_(new WidgetFadeHelper(this, 1000)),
      dirty_(false) {

  // Add the top-level scroll area
  QScrollArea *scrollarea = new QScrollArea(this);
  setLayout(new QVBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(scrollarea);

  // Add a container widget to the scroll area
  QWidget *container_widget = new QWidget;
  container_widget->setLayout(container_);
  container_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  container_widget->setBackgroundRole(QPalette::Base);
  container_->setSizeConstraint(QLayout::SetMinAndMaxSize);
  container_->setContentsMargins(0, 0, 0, 0);
  container_->setSpacing(6);
  scrollarea->setWidget(container_widget);
  scrollarea->setWidgetResizable(true);

  // Add a spacer to the bottom of the container
  container_->addStretch();

  // Set stylesheet
  QFile stylesheet(":/style/artistbio.css");
  if (stylesheet.open(QIODevice::ReadOnly)) {
    setStyleSheet(QString::fromLatin1(stylesheet.readAll()));
    stylesheet.close();
  }

  fetcher_->AddProvider(new LastFMArtistBio);
  fetcher_->AddProvider(new WikipediaArtistBio);

  connect(fetcher_, SIGNAL(ResultReady(int, ArtistBioFetcher::Result)), SLOT(ResultReady(int, ArtistBioFetcher::Result)));
  connect(fetcher_, SIGNAL(InfoResultReady(int, CollapsibleInfoPane::Data)), SLOT(InfoResultReady(int, CollapsibleInfoPane::Data)));

}

ArtistBioView::~ArtistBioView() {}

void ArtistBioView::showEvent(QShowEvent *e) {

  if (dirty_) {
    MaybeUpdate(queued_metadata_);
    dirty_ = false;
  }

  QWidget::showEvent(e);

}

void ArtistBioView::ReloadSettings() {

  for (CollapsibleInfoPane *pane : sections_) {
    QWidget *contents = pane->data().contents_;
    if (!contents) continue;

    QMetaObject::invokeMethod(contents, "ReloadSettings");
  }

}

bool ArtistBioView::NeedsUpdate(const Song &old_metadata, const Song &new_metadata) const {

  if (new_metadata.artist().isEmpty()) return false;

  return old_metadata.artist() != new_metadata.artist();

}

void ArtistBioView::InfoResultReady(const int id, const CollapsibleInfoPane::Data &_data) {

  if (id != current_request_id_) return;

  AddSection(new CollapsibleInfoPane(_data, this));
  CollapseSections();

}

void ArtistBioView::ResultReady(const int id, const ArtistBioFetcher::Result &result) {

  if (id != current_request_id_) return;

  if (!result.images_.isEmpty()) {
    // Image view goes at the top
    PrettyImageView *image_view = new PrettyImageView(network_, this);
    AddWidget(image_view);

    for (const QUrl& url : result.images_) {
      image_view->AddImage(url);
    }
  }

  CollapseSections();

}

void ArtistBioView::Clear() {

  fader_->StartFade();

  qDeleteAll(widgets_);
  widgets_.clear();
  if (section_container_) {
    container_->removeWidget(section_container_);
    delete section_container_;
  }
  sections_.clear();

  // Container for collapsible sections goes below
  section_container_ = new QWidget;
  section_container_->setLayout(new QVBoxLayout);
  section_container_->layout()->setContentsMargins(0, 0, 0, 0);
  section_container_->layout()->setSpacing(1);
  section_container_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  container_->insertWidget(0, section_container_);

}

void ArtistBioView::AddSection(CollapsibleInfoPane *section) {

  int i = 0;
  for (; i < sections_.count(); ++i) {
    if (section->data() < sections_[i]->data()) break;
  }

  ConnectWidget(section->data().contents_);

  sections_.insert(i, section);
  qobject_cast<QVBoxLayout*>(section_container_->layout())->insertWidget(i, section);
  section->show();

}

void ArtistBioView::AddWidget(QWidget *widget) {

  ConnectWidget(widget);

  container_->insertWidget(container_->count() - 2, widget);
  widgets_ << widget;

}

void ArtistBioView::SongChanged(const Song &metadata) {

  if (isVisible()) {
    MaybeUpdate(metadata);
    dirty_ = false;
  }
  else {
    queued_metadata_ = metadata;
    dirty_ = true;
  }

}

void ArtistBioView::SongFinished() { dirty_ = false; }

void ArtistBioView::MaybeUpdate(const Song &metadata) {

  if (old_metadata_.is_valid()) {
    if (!NeedsUpdate(old_metadata_, metadata)) {
      return;
    }
  }

  Update(metadata);
  old_metadata_ = metadata;

}

void ArtistBioView::Update(const Song &metadata) {

  current_request_id_ = fetcher_->FetchInfo(metadata);

  // Do this after the new pane has been shown otherwise it'll just grab a black rectangle.
  Clear();
  QTimer::singleShot(0, fader_, SLOT(StartBlur()));

}

void ArtistBioView::CollapseSections() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  // Sections are already sorted by type and relevance, so the algorithm we use to determine which ones to show by default is:
  //   * In the absence of any user preference, show the first (highest relevance section of each type and hide the rest)
  //   * If one or more sections in a type have been explicitly hidden/shown by the user before then hide all sections in that type and show only the ones that are explicitly shown.

  QMultiMap<CollapsibleInfoPane::Data::Type, CollapsibleInfoPane*> types_;
  QSet<CollapsibleInfoPane::Data::Type> has_user_preference_;
  for (CollapsibleInfoPane *pane : sections_) {
    const CollapsibleInfoPane::Data::Type type = pane->data().type_;
    types_.insert(type, pane);

    QVariant preference = s.value(pane->data().id_);
    if (preference.isValid()) {
      has_user_preference_.insert(type);
      if (preference.toBool()) {
        pane->Expand();
      }
    }
  }

  for (CollapsibleInfoPane::Data::Type type : types_.keys()) {
    if (!has_user_preference_.contains(type)) {
      // Expand the first one
      types_.values(type).last()->Expand();
    }
  }

  for (CollapsibleInfoPane *pane : sections_) {
    connect(pane, SIGNAL(Toggled(bool)), SLOT(SectionToggled(bool)));
  }

}

void ArtistBioView::SectionToggled(const bool value) {

  CollapsibleInfoPane *pane = qobject_cast<CollapsibleInfoPane*>(sender());
  if (!pane || !sections_.contains(pane)) return;

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(pane->data().id_, value);
  s.endGroup();

}

void ArtistBioView::ConnectWidget(QWidget *widget) {

  const QMetaObject *m = widget->metaObject();

  if (m->indexOfSignal("ShowSettingsDialog()") != -1) {
    connect(widget, SIGNAL(ShowSettingsDialog()), SIGNAL(ShowSettingsDialog()));
  }

}
