/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/shared_ptr.h"
#include "core/song.h"
#include "scrobblersettings.h"

class Application;
class ScrobblerService;
class Song;

class AudioScrobbler : public QObject {
  Q_OBJECT

 public:
  explicit AudioScrobbler(Application *app, QObject *parent = nullptr);
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
    return std::static_pointer_cast<T>(ServiceByName(T::kName));
  }

  void ReloadSettings();
  SharedPtr<ScrobblerSettings> settings() { return settings_; }

  bool enabled() const { return settings_->enabled(); }
  bool offline() const { return settings_->offline(); }
  bool scrobble_button() const { return settings_->scrobble_button(); }
  bool love_button() const { return settings_->love_button(); }
  int submit_delay() const { return settings_->submit_delay(); }
  bool prefer_albumartist() const { return settings_->prefer_albumartist(); }
  bool ShowErrorDialog() const { return settings_->show_error_dialog(); }
  QList<Song::Source> sources() const { return settings_->sources(); }

  void ShowConfig();

  void UpdateNowPlaying(const Song &song);
  void ClearPlaying();
  void Scrobble(const Song &song, const qint64 scrobble_point);

 public slots:
  void ToggleScrobbling();
  void ToggleOffline();
  void ErrorReceived(const QString &error);
  void Submit();
  void Love();
  void WriteCache();

 signals:
  void ErrorMessage(const QString &error);

 private:
  Application *app_;
  SharedPtr<ScrobblerSettings> settings_;
  QMap<QString, SharedPtr<ScrobblerService>> services_;

  Q_DISABLE_COPY(AudioScrobbler)
};

#endif  // AUDIOSCROBBLER_H
