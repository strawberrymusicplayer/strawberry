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

#ifndef STREAMINGSERVICE_H
#define STREAMINGSERVICE_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QIcon>

#include "includes/shared_ptr.h"
#include "core/song.h"

class CollectionBackend;
class CollectionModel;
class CollectionFilter;

class StreamingService : public QObject {
  Q_OBJECT

 public:
  explicit StreamingService(const Song::Source source, const QString &name, const QString &url_scheme, const QString &settings_group, QObject *parent = nullptr);
  ~StreamingService() override {}

  enum class SearchType {
    Artists = 1,
    Albums = 2,
    Songs = 3
  };

  virtual void Exit() = 0;

  virtual Song::Source source() const { return source_; }
  virtual QString name() const { return name_; }
  virtual QString url_scheme() const { return url_scheme_; }
  virtual QString settings_group() const { return settings_group_; }
  virtual bool has_initial_load_settings() const { return false; }
  virtual void InitialLoadSettings() {}
  virtual void ReloadSettings() {}
  virtual QIcon Icon() const { return Song::IconForSource(source_); }
  virtual bool oauth() const { return false; }
  virtual bool authenticated() const { return false; }
  virtual int Search(const QString &query, const SearchType type) { Q_UNUSED(query); Q_UNUSED(type); return 0; }
  virtual void CancelSearch() {}
  virtual bool show_progress() const { return true; }
  virtual bool enable_refresh_button() const { return true; }

  virtual SharedPtr<CollectionBackend> artists_collection_backend() { return nullptr; }
  virtual SharedPtr<CollectionBackend> albums_collection_backend() { return nullptr; }
  virtual SharedPtr<CollectionBackend> songs_collection_backend() { return nullptr; }

  virtual CollectionModel *artists_collection_model() { return nullptr; }
  virtual CollectionModel *albums_collection_model() { return nullptr; }
  virtual CollectionModel *songs_collection_model() { return nullptr; }

  virtual CollectionFilter *artists_collection_filter_model() { return nullptr; }
  virtual CollectionFilter *albums_collection_filter_model() { return nullptr; }
  virtual CollectionFilter *songs_collection_filter_model() { return nullptr; }

 public Q_SLOTS:
  virtual void Configure() {}
  virtual void GetArtists() {}
  virtual void GetAlbums() {}
  virtual void GetSongs() {}
  virtual void ResetArtistsRequest() {}
  virtual void ResetAlbumsRequest() {}
  virtual void ResetSongsRequest() {}

 Q_SIGNALS:
  void ExitFinished();
  void RequestLogin();
  void RequestLogout();
  void LoginWithCredentials(const QString &api_token, const QString &username, const QString &password);
  void LoginSuccess();
  void LoginFailure(const QString &error);
  void LoginFinished(const bool success, const QString &error = QString());

  void TestSuccess();
  void TestFailure(const QString &error);
  void TestComplete(const bool success, const QString &error = QString());

  void ShowErrorDialog(const QString &error);
  void Results(const SongMap &songs, const QString &error);
  void UpdateStatus(const QString &text);
  void ProgressSetMaximum(const int max);
  void UpdateProgress(const int max);

  void ArtistsResults(const SongMap &songs, const QString &error);
  void ArtistsUpdateStatus(const QString &text);
  void ArtistsProgressSetMaximum(const int max);
  void ArtistsUpdateProgress(const int max);

  void AlbumsResults(const SongMap &songs, const QString &error);
  void AlbumsUpdateStatus(const QString &text);
  void AlbumsProgressSetMaximum(const int max);
  void AlbumsUpdateProgress(const int max);

  void SongsResults(const SongMap &songs, const QString &error);
  void SongsUpdateStatus(const QString &text);
  void SongsProgressSetMaximum(const int max);
  void SongsUpdateProgress(const int max);

  void SearchResults(const int id, const SongMap &songs, const QString &error);
  void SearchUpdateStatus(const int id, const QString &text);
  void SearchProgressSetMaximum(const int id, const int max);
  void SearchUpdateProgress(const int id, const int max);

  void AddArtists(const SongList &songs);
  void AddAlbums(const SongList &songs);
  void AddSongs(const SongList &songs);

  void RemoveArtists(const SongList &songs);
  void RemoveAlbums(const SongList &songs);
  void RemoveSongsByList(const SongList &songs);
  void RemoveSongsByMap(const SongMap &songs);

  void StreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void StreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);
  void StreamURLRequestFinished(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error = QString());

  void OpenSettingsDialog(const Song::Source source);

 private:
  Song::Source source_;
  QString name_;
  QString url_scheme_;
  QString settings_group_;
};

using StreamingServicePtr = SharedPtr<StreamingService>;

Q_DECLARE_METATYPE(StreamingService*)
Q_DECLARE_METATYPE(StreamingServicePtr)

#endif  // STREAMINGSERVICE_H
