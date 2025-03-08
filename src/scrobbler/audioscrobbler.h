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

#ifndef AUDIOSCROBBLER_H
#define AUDIOSCROBBLER_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblersettingsservice.h"

class ScrobblerService;
class Song;

class AudioScrobbler : public QObject {
  Q_OBJECT

 public:
  explicit AudioScrobbler(QObject *parent = nullptr);
  ~AudioScrobbler();

  void AddService(SharedPtr<ScrobblerService> service);
  void RemoveService(SharedPtr<ScrobblerService> service);
  QList<SharedPtr<ScrobblerService>> List() const { return services_.values(); }
  bool HasAnyServices() const { return !services_.isEmpty(); }
  int NextId();

  QList<SharedPtr<ScrobblerService>> GetAll();
  SharedPtr<ScrobblerService> ServiceByName(const QString &name);
  template<typename T>
  SharedPtr<T> Service() {
    return std::static_pointer_cast<T>(ServiceByName(QLatin1String(T::kName)));
  }

  void ReloadSettings();
  SharedPtr<ScrobblerSettingsService> settings() { return settings_; }

  bool enabled() const { return settings_->enabled(); }
  bool offline() const { return settings_->offline(); }
  bool scrobble_button() const { return settings_->scrobble_button(); }
  bool love_button() const { return settings_->love_button(); }
  int submit_delay() const { return settings_->submit_delay(); }
  bool prefer_albumartist() const { return settings_->prefer_albumartist(); }
  bool ShowErrorDialog() const { return settings_->show_error_dialog(); }
  bool strip_remastered() const { return settings_->strip_remastered(); }
  QList<Song::Source> sources() const { return settings_->sources(); }

  void UpdateNowPlaying(const Song &song);
  void ClearPlaying();
  void Scrobble(const Song &song, const qint64 scrobble_point);

 public Q_SLOTS:
  void ToggleScrobbling();
  void ToggleOffline();
  void ErrorReceived(const QString &error);
  void Submit();
  void Love();
  void WriteCache();

 Q_SIGNALS:
  void ErrorMessage(const QString &error);

 private:
  SharedPtr<ScrobblerSettingsService> settings_;
  QMap<QString, SharedPtr<ScrobblerService>> services_;

  Q_DISABLE_COPY(AudioScrobbler)
};

#endif  // AUDIOSCROBBLER_H
