/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MPRIS2_H
#define MPRIS2_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QMetaType>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QDBusObjectPath>
#include <QDBusArgument>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "covermanager/albumcoverloaderresult.h"

class Player;
class PlaylistManager;
class CurrentAlbumCoverLoader;
class Song;
class Playlist;
class PlaylistItem;

using TrackMetadata = QList<QVariantMap>;
using Track_Ids = QList<QDBusObjectPath>;
Q_DECLARE_METATYPE(TrackMetadata)

struct MprisPlaylist {
  QDBusObjectPath id;
  QString name;
  QString icon;  // Uri
};
using MprisPlaylistList = QList<MprisPlaylist>;
Q_DECLARE_METATYPE(MprisPlaylist)
Q_DECLARE_METATYPE(MprisPlaylistList)

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

struct MaybePlaylist {
  bool valid{};
  MprisPlaylist playlist;
};
Q_DECLARE_METATYPE(MaybePlaylist)

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

QDBusArgument &operator<<(QDBusArgument &arg, const MprisPlaylist &playlist);
const QDBusArgument &operator>>(const QDBusArgument &arg, MprisPlaylist &playlist);

QDBusArgument &operator<<(QDBusArgument &arg, const MaybePlaylist &playlist);
const QDBusArgument &operator>>(const QDBusArgument &arg, MaybePlaylist &playlist);

namespace mpris {

class Mpris2 : public QObject {
  Q_OBJECT

 public:
  explicit Mpris2(const SharedPtr<Player> player,
                  const SharedPtr<PlaylistManager> playlist_manager,
                  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                  QObject *parent = nullptr);

  // These properties are exposed over D-Bus, where change notification is done through the org.freedesktop.DBus.Properties.PropertiesChanged signal emitted by EmitNotification, so the dynamic properties have no Qt NOTIFY signal.

  // org.mpris.MediaPlayer2 MPRIS 2.0 Root interface
  Q_PROPERTY(bool CanQuit READ CanQuit CONSTANT)
  Q_PROPERTY(bool CanRaise READ CanRaise CONSTANT)
  Q_PROPERTY(bool HasTrackList READ HasTrackList CONSTANT)
  Q_PROPERTY(QString Identity READ Identity CONSTANT)
  Q_PROPERTY(QString DesktopEntry READ DesktopEntry CONSTANT)
  Q_PROPERTY(QStringList SupportedUriSchemes READ SupportedUriSchemes CONSTANT)
  Q_PROPERTY(QStringList SupportedMimeTypes READ SupportedMimeTypes CONSTANT)

  // org.mpris.MediaPlayer2 MPRIS 2.2 Root interface
  Q_PROPERTY(bool CanSetFullscreen READ CanSetFullscreen CONSTANT)
  Q_PROPERTY(bool Fullscreen READ Fullscreen WRITE SetFullscreen)  // clazy:exclude=qproperty-without-notify

  // org.mpris.MediaPlayer2.Player MPRIS 2.0 Player interface
  Q_PROPERTY(QString PlaybackStatus READ PlaybackStatus)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(QString LoopStatus READ LoopStatus WRITE SetLoopStatus)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(double Rate READ Rate WRITE SetRate)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool Shuffle READ Shuffle WRITE SetShuffle)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(QVariantMap Metadata READ Metadata)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(double Volume READ Volume WRITE SetVolume)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(qint64 Position READ Position)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(double MinimumRate READ MinimumRate CONSTANT)
  Q_PROPERTY(double MaximumRate READ MaximumRate CONSTANT)
  Q_PROPERTY(bool CanGoNext READ CanGoNext)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanGoPrevious READ CanGoPrevious)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanPlay READ CanPlay)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanPause READ CanPause)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanSeek READ CanSeek)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanControl READ CanControl CONSTANT)

  // org.mpris.MediaPlayer2.TrackList MPRIS 2.0 Player interface
  Q_PROPERTY(Track_Ids Tracks READ Tracks)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(bool CanEditTracks READ CanEditTracks)  // clazy:exclude=qproperty-without-notify

  // org.mpris.MediaPlayer2.Playlists MPRIS 2.1 Playlists interface
  Q_PROPERTY(quint32 PlaylistCount READ PlaylistCount)  // clazy:exclude=qproperty-without-notify
  Q_PROPERTY(QStringList Orderings READ Orderings CONSTANT)
  Q_PROPERTY(MaybePlaylist ActivePlaylist READ ActivePlaylist)  // clazy:exclude=qproperty-without-notify

  // strawberry specific additional property to extend MPRIS Player interface
  Q_PROPERTY(double Rating READ Rating WRITE SetRating)  // clazy:exclude=qproperty-without-notify

  // Root Properties
  bool CanQuit() const;
  bool CanRaise() const;
  bool HasTrackList() const;
  QString Identity() const;
  QString DesktopEntry() const;
  QStringList SupportedUriSchemes() const;
  QStringList SupportedMimeTypes() const;

  // Root Properties added in MPRIS 2.2
  bool CanSetFullscreen() const { return false; }
  bool Fullscreen() const { return false; }
  void SetFullscreen(bool value) { Q_UNUSED(value) }

  // Methods
  void Raise();
  void Quit();

  // Player Properties
  QString PlaybackStatus() const;
  QString LoopStatus() const;
  void SetLoopStatus(const QString &value);
  double Rate() const;
  void SetRate(double rate);
  bool Shuffle() const;
  void SetShuffle(bool enable);
  QVariantMap Metadata() const;
  double Rating() const;
  void SetRating(double rating);
  double Volume() const;
  void SetVolume(const double volume);
  qint64 Position() const;
  double MaximumRate() const;
  double MinimumRate() const;
  bool CanGoNext() const;
  bool CanGoPrevious() const;
  bool CanPlay() const;
  bool CanPause() const;
  bool CanSeek() const;
  bool CanControl() const;

  // Methods
  void Next();
  void Previous();
  void Pause();
  void PlayPause();
  void Stop();
  void Play();
  void Seek(qint64 offset);
  void SetPosition(const QDBusObjectPath &trackId, qint64 offset);
  void OpenUri(const QString &uri);

  // TrackList Properties
  Track_Ids Tracks() const;
  bool CanEditTracks() const;

  // Methods
  TrackMetadata GetTracksMetadata(const Track_Ids &tracks) const;
  void AddTrack(const QString &uri, const QDBusObjectPath &afterTrack, bool setAsCurrent);
  void RemoveTrack(const QDBusObjectPath &trackId);
  void GoTo(const QDBusObjectPath &trackId);

  // Playlist Properties
  quint32 PlaylistCount() const;
  QStringList Orderings() const;
  QDBusObjectPath MakePlaylistPath(const int id) const;
  MaybePlaylist ActivePlaylist() const;

  // Methods
  void ActivatePlaylist(const QDBusObjectPath &mpris2_playlist_id);
  MprisPlaylistList GetPlaylists(quint32 index, quint32 max_count, const QString &order, bool reverse_order);

  // To emit the track list changed signal
  void EmitTrackListReplaced();

 Q_SIGNALS:
  // Player
  void Seeked(const qint64 position);

  // TrackList
  void TrackListReplaced(const Track_Ids &tracks, const QDBusObjectPath &current_track);
  void TrackAdded(const QVariantMap &metadata, const QDBusObjectPath &after_track);
  void TrackRemoved(const QDBusObjectPath &track_id);
  void TrackMetadataChanged(const QDBusObjectPath &track_id, const QVariantMap &metadata);

  void RaiseMainWindow();

  // Playlist
  void PlaylistChanged(const MprisPlaylist &playlist);

 private Q_SLOTS:
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result = AlbumCoverLoaderResult());
  void EngineStateChanged(EngineBase::State newState);
  void VolumeChanged();

  void PlaylistManagerInitialized();
  void AllPlaylistsLoaded();
  void CurrentSongChanged(const Song &song);
  void ShuffleModeChanged();
  void RepeatModeChanged();
  void PlaylistChangedSlot(Playlist *playlist);
  void PlaylistCollectionChanged(Playlist *playlist);
  void PlaylistItemsAdded(const int playlist_id, const QList<QUuid> &track_ids, const QUuid &after_track_id);
  void PlaylistItemsRemoved(const int playlist_id, const QList<QUuid> &track_ids);
  void PlaylistItemMetadataChanged(const int playlist_id, const QUuid &track_id);

 private:
  void EmitNotification(const QString &name);
  void EmitNotification(const QString &name, const QVariant &value);
  void EmitNotification(const QString &name, const QVariant &value, const QString &mprisEntity);

  QString PlaybackStatus(EngineBase::State state) const;

  SharedPtr<PlaylistItem> current_playlist_item() const;
  QUuid current_playlist_item_uuid() const;
  QDBusObjectPath current_track_id(const QUuid &current_row) const;

  bool CanSeek(EngineBase::State state) const;

  QString DesktopEntryAbsolutePath() const;

  static QUuid GetTrackObjectPathUuid(const QDBusObjectPath &track_object_path);

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;

  QString desktopfilepath_;
  QVariantMap last_metadata_;
};

}  // namespace mpris


#endif  // MPRIS2_H
