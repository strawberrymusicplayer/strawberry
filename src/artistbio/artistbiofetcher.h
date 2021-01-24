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

#ifndef ARTISTBIOFETCHER_H
#define ARTISTBIOFETCHER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QUrl>

#include "widgets/collapsibleinfopane.h"
#include "core/song.h"

class QTimer;
class ArtistBioProvider;

class ArtistBioFetcher : public QObject {
  Q_OBJECT

 public:
  explicit ArtistBioFetcher(QObject *parent = nullptr);
  ~ArtistBioFetcher() override;

  struct Result {
    QList<QUrl> images_;
    QList<CollapsibleInfoPane::Data> info_;
  };

  static const int kDefaultTimeoutDuration = 25000;

  void AddProvider(ArtistBioProvider *provider);
  int FetchInfo(const Song &metadata);

  QList<ArtistBioProvider*> providers() const { return providers_; }

 signals:
  void InfoResultReady(int, CollapsibleInfoPane::Data);
  void ResultReady(int, ArtistBioFetcher::Result);

 private slots:
  void ImageReady(const int id, const QUrl &url);
  void InfoReady(const int id, const CollapsibleInfoPane::Data &data);
  void ProviderFinished(const int id);
  void Timeout(const int id);

 private:
  QList<ArtistBioProvider*> providers_;

  QMap<int, Result> results_;
  QMap<int, QList<ArtistBioProvider*>> waiting_for_;
  QMap<int, QTimer*> timeout_timers_;

  int timeout_duration_;
  int next_id_;
};

#endif  // ARTISTBIOFETCHER_H
