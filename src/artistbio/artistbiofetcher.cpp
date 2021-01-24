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

#include <QObject>
#include <QTimer>
#include <QUrl>

#include "artistbiofetcher.h"
#include "artistbioprovider.h"
#include "core/logging.h"

ArtistBioFetcher::ArtistBioFetcher(QObject *parent)
    : QObject(parent),
      timeout_duration_(kDefaultTimeoutDuration),
      next_id_(1) {}

void ArtistBioFetcher::AddProvider(ArtistBioProvider *provider) {

  providers_ << provider;
  connect(provider, SIGNAL(ImageReady(int, QUrl)), SLOT(ImageReady(int, QUrl)), Qt::QueuedConnection);
  connect(provider, SIGNAL(InfoReady(int, CollapsibleInfoPane::Data)), SLOT(InfoReady(int, CollapsibleInfoPane::Data)), Qt::QueuedConnection);
  connect(provider, SIGNAL(Finished(int)), SLOT(ProviderFinished(int)), Qt::QueuedConnection);

}

ArtistBioFetcher::~ArtistBioFetcher() {

  while (!providers_.isEmpty()) {
    ArtistBioProvider *provider = providers_.takeFirst();
    provider->deleteLater();
  }

}

int ArtistBioFetcher::FetchInfo(const Song &metadata) {

  const int id = next_id_++;
  results_[id] = Result();
  timeout_timers_[id] = new QTimer(this);
  timeout_timers_[id]->setSingleShot(true);
  timeout_timers_[id]->setInterval(timeout_duration_);
  timeout_timers_[id]->start();

  connect(timeout_timers_[id], &QTimer::timeout, [this, id]() { Timeout(id); });

  for (ArtistBioProvider *provider : providers_) {
    if (provider->is_enabled()) {
      waiting_for_[id].append(provider);
      provider->Start(id, metadata);
    }
  }
  return id;

}

void ArtistBioFetcher::ImageReady(const int id, const QUrl &url) {

  if (!results_.contains(id)) return;
  results_[id].images_ << url;

}

void ArtistBioFetcher::InfoReady(const int id, const CollapsibleInfoPane::Data &data) {

  if (!results_.contains(id)) return;
  results_[id].info_ << data;

  if (!waiting_for_.contains(id)) return;
  emit InfoResultReady(id, data);

}

void ArtistBioFetcher::ProviderFinished(const int id) {

  if (!results_.contains(id)) return;
  if (!waiting_for_.contains(id)) return;

  ArtistBioProvider *provider = qobject_cast<ArtistBioProvider*>(sender());
  if (!waiting_for_[id].contains(provider)) return;

  waiting_for_[id].removeAll(provider);
  if (waiting_for_[id].isEmpty()) {
    Result result = results_.take(id);
    emit ResultReady(id, result);
    waiting_for_.remove(id);
    delete timeout_timers_.take(id);
  }

}

void ArtistBioFetcher::Timeout(const int id) {

  if (!results_.contains(id)) return;
  if (!waiting_for_.contains(id)) return;

  // Emit the results that we have already
  emit ResultReady(id, results_.take(id));

  // Cancel any providers that we're still waiting for
  for (ArtistBioProvider *provider : waiting_for_[id]) {
    qLog(Info) << "Request timed out from info provider" << provider->name();
    provider->Cancel(id);
  }
  waiting_for_.remove(id);

  // Remove the timer
  delete timeout_timers_.take(id);

}
