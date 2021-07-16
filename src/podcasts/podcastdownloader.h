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

#ifndef PODCASTDOWNLOADER_H
#define PODCASTDOWNLOADER_H

#include <memory>

#include <QObject>
#include <QFile>
#include <QSet>
#include <QList>
#include <QQueue>
#include <QString>
#include <QRegularExpression>
#include <QNetworkRequest>

#include "core/networkaccessmanager.h"
#include "podcast.h"
#include "podcastepisode.h"

class Application;
class PodcastBackend;

class NetworkAccessManager;
class QNetworkReply;

namespace PodcastDownload {
enum State {
  NotDownloading,
  Queued,
  Downloading,
  Finished
};
}

class Task : public QObject {
  Q_OBJECT

 public:
  Task(const PodcastEpisode &episode, QFile *file, PodcastBackend *backend);
  PodcastEpisode episode() const;

 signals:
  void ProgressChanged(const PodcastEpisode &episode, const PodcastDownload::State state, const int percent);
  void finished(Task *task);

 public slots:
  void finishedPublic();

 private slots:
  void reading();
  void downloadProgressInternal(qint64 received, qint64 total);
  void finishedInternal();

 private:
  std::unique_ptr<QFile> file_;
  PodcastEpisode episode_;
  PodcastBackend *backend_;
  std::unique_ptr<NetworkAccessManager> network_;
  QNetworkRequest req_;
  QNetworkReply *reply_;
};

class PodcastDownloader : public QObject {
  Q_OBJECT

 public:
  explicit PodcastDownloader(Application *app, QObject *parent = nullptr);

  PodcastEpisodeList EpisodesDownloading(const PodcastEpisodeList &episodes);
  QString DefaultDownloadDir() const;

 public slots:
  // Adds the episode to the download queue
  void DownloadEpisode(const PodcastEpisode &episode);
  void cancelDownload(const PodcastEpisodeList &episodes);

 signals:
  void ProgressChanged(const PodcastEpisode &episode, const PodcastDownload::State state, const int percent);

 private slots:
  void ReloadSettings();

  void SubscriptionAdded(const Podcast &podcast);
  void EpisodesAdded(const PodcastEpisodeList &episodes);

  void ReplyFinished(Task *task);

 private:
  QString FilenameForEpisode(const QString &directory, const PodcastEpisode &episode) const;
  QString SanitiseFilenameComponent(const QString &text) const;

 private:
  static const char *kSettingsGroup;

  Application *app_;
  PodcastBackend *backend_;
  NetworkAccessManager *network_;

  QRegularExpression disallowed_filename_characters_;

  bool auto_download_;
  QString download_dir_;

  QList<Task*> list_tasks_;
};

#endif  // PODCASTDOWNLOADER_H
