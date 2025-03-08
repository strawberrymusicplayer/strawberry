/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCROBBLERSERVICE_H
#define SCROBBLERSERVICE_H

#include "config.h"

#include <QPair>
#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/jsonbaserequest.h"

#include "scrobblersettingsservice.h"

class ScrobblerService : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit ScrobblerService(const QString &name, const SharedPtr<NetworkAccessManager> network, const SharedPtr<ScrobblerSettingsService> settings, QObject *parent);

  QString name() const { return name_; }
  QString service_name() const override { return name_; }

  virtual void ReloadSettings() = 0;

  virtual bool enabled() const { return false; }
  virtual bool authenticated() const override { return false; }

  virtual void UpdateNowPlaying(const Song &song) = 0;
  virtual void ClearPlaying() = 0;
  virtual void Scrobble(const Song &song) = 0;
  virtual void Love() {}

  virtual void StartSubmit(const bool initial = false) = 0;
  virtual bool submitted() const { return false; }

 protected:
  using EncodedParam = QPair<QByteArray, QByteArray>;

  QString StripAlbum(const QString &album) const;
  QString StripTitle(const QString &title) const;

 public Q_SLOTS:
  virtual void Submit() = 0;
  virtual void WriteCache() = 0;

 Q_SIGNALS:
  void ErrorMessage(const QString &error);
  void OpenSettingsDialog();

 protected:
  const QString name_;
  const SharedPtr<ScrobblerSettingsService> settings_;
};

using ScrobblerServicePtr = SharedPtr<ScrobblerService>;

#endif  // SCROBBLERSERVICE_H
