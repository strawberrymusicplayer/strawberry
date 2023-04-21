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

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>
#include <QAtomicInt>

#include "core/song.h"

class Application;
class ScrobblerService;
class Song;

class AudioScrobbler : public QObject {
  Q_OBJECT

 public:
  explicit AudioScrobbler(Application *app, QObject *parent = nullptr);
  ~AudioScrobbler();

  void AddService(ScrobblerService *service);
  void RemoveService(ScrobblerService *service);
  QList<ScrobblerService*> List() const { return services_.values(); }
  bool HasAnyServices() const { return !services_.isEmpty(); }
  int NextId();

  QList<ScrobblerService*> GetAll();
  ScrobblerService *ServiceByName(const QString &name);
  template<typename T>
  T *Service() {
    return qobject_cast<T*>(ServiceByName(T::kName));
  }

  void ReloadSettings();

  bool IsEnabled() const { return enabled_; }
  bool IsOffline() const { return offline_; }
  bool ScrobbleButton() const { return scrobble_button_; }
  bool LoveButton() const { return love_button_; }
  int SubmitDelay() const { return submit_delay_; }
  bool PreferAlbumArtist() const { return prefer_albumartist_; }
  bool ShowErrorDialog() const { return show_error_dialog_; }
  QList<Song::Source> sources() const { return sources_; }

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
  void ScrobblingEnabledChanged(const bool value);
  void ScrobblingOfflineChanged(const bool value);
  void ScrobbleButtonVisibilityChanged(const bool value);
  void LoveButtonVisibilityChanged(const bool value);

 private:
  Application *app_;

  QMap<QString, ScrobblerService*> services_;
  QMutex mutex_;
  QAtomicInt next_id_;

  bool enabled_;
  bool offline_;
  bool scrobble_button_;
  bool love_button_;
  int submit_delay_;
  bool prefer_albumartist_;
  bool show_error_dialog_;
  QList<Song::Source> sources_;

  Q_DISABLE_COPY(AudioScrobbler)
};

#endif  // AUDIOSCROBBLER_H
