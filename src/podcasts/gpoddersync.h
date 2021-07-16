/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GPODDERSYNC_H
#define GPODDERSYNC_H

#include <QObject>
#include <QScopedPointer>
#include <QSet>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QUrl>
#include <QNetworkReply>

#include <ApiRequest.h>

#include "podcastepisode.h"

class QTimer;

class Application;
class NetworkAccessManager;
class Podcast;
class PodcastBackend;
class PodcastUrlLoader;
class PodcastUrlLoaderReply;

class GPodderSync : public QObject {
  Q_OBJECT

 public:
  explicit GPodderSync(Application *app, QObject *parent = nullptr);
  ~GPodderSync();

  static const char *kSettingsGroup;
  static const int kFlushUpdateQueueDelay;
  static const int kGetUpdatesInterval;
  static const int kRequestTimeout;

  static QString DefaultDeviceName();
  static QString DeviceId();

  bool is_logged_in() const;

  // Tries to login using the given username and password.  Also sets the device name and type on gpodder.net.
  // If login succeeds the username and password will be saved in QSettings.
  void Login(const QString &username, const QString &password, const QString &device_name);

  // Clears any saved username and password from QSettings.
  void Logout();

 signals:
  void LoginSuccess();
  void LoginFailure(const QString &error);

 public slots:
  void GetUpdatesNow();

 private slots:
  void ReloadSettings();
  void LoginFinished(QNetworkReply *reply, const QString &username, const QString &password);

  void DeviceUpdatesFinished(mygpo::DeviceUpdatesPtr reply);
  void DeviceUpdatesParseError();
  void DeviceUpdatesRequestError(QNetworkReply::NetworkError error);

  void NewPodcastLoaded(PodcastUrlLoaderReply *reply, const QUrl &url, const QList<mygpo::EpisodePtr> &actions);

  void ApplyActions(const QList<mygpo::EpisodePtr> &actions, PodcastEpisodeList *episodes);

  void SubscriptionAdded(const Podcast &podcast);
  void SubscriptionRemoved(const Podcast &podcast);
  void FlushUpdateQueue();

  void AddRemoveFinished(const QList<QUrl> &affected_urls);
  void AddRemoveParseError();
  void AddRemoveRequestError(QNetworkReply::NetworkError error);

 private:
  void LoadQueue();
  void SaveQueue();

  void DoInitialSync();

 private:
  Application *app_;
  NetworkAccessManager *network_;
  QScopedPointer<mygpo::ApiRequest> api_;

  PodcastBackend *backend_;
  PodcastUrlLoader *loader_;

  QString username_;
  QString password_;
  QDateTime last_successful_get_;
  QTimer *get_updates_timer_;

  QTimer *flush_queue_timer_;
  QSet<QUrl> queued_add_subscriptions_;
  QSet<QUrl> queued_remove_subscriptions_;
  bool flushing_queue_;
};

#endif  // GPODDERSYNC_H
