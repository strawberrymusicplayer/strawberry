// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <functional>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtConcurrentRun>

#include "core/logging.h"
#ifdef HAVE_NAS
#  include "core/nasfileengine.h"
#endif
#include "core/settings.h"
#include "core/song.h"
#include "collection/collectionbackend.h"
#include "playlist/playlistbackend.h"
#include "smartplaylists/smartplaylistsearch.h"
#include "smartplaylists/smartplaylistsearchterm.h"
#include "alarmbridge_android.h"
#include "alarmclock_android.h"
#include "alarmtracks_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Starting the app should not be slowed down by copying songs.
constexpr int kOutdatedTracksDelayMsec = 30000;
// Random and recently played songs are chosen again twice a day, so the alarm does not always play the same songs.
constexpr qint64 kOutdatedTracksSeconds = 12 * 60 * 60;
// Enough candidates to reach the largest number of tracks per alarm when some can't be copied.
constexpr int kCandidateTracks = 25;

constexpr char kSettingsGroup[] = "AlarmClock";
constexpr char kCopyTracks[] = "copy_tracks";
constexpr char kTracksPerAlarm[] = "tracks_per_alarm";
constexpr char kMaximumTotalMb[] = "maximum_total_mb";

QList<QUrl> SearchUrls(SharedPtr<CollectionBackend> backend, const SmartPlaylistSearch &search) {

  QVariantList bound_values;
  const QString sql = search.ToSql(backend->songs_table(), bound_values);
  const SongList songs = backend->ExecuteQuery(sql, bound_values);
  backend->Close();
  QList<QUrl> urls;
  urls.reserve(songs.count());
  for (const Song &song : songs) urls << song.url();
  return urls;

}

}  // namespace

AlarmClock::AlarmClock(SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, QObject *parent)
    : QObject(parent),
      collection_backend_(collection_backend),
      playlist_backend_(playlist_backend) {}

void AlarmClock::Start() {

  QJsonObject labels;
  labels.insert(u"channel"_s, tr("Alarm clock"));
  labels.insert(u"alarm"_s, tr("Alarm"));
  labels.insert(u"snooze"_s, tr("Snooze"));
  labels.insert(u"stop"_s, tr("Stop"));
  labels.insert(u"back"_s, tr("Back"));
  labels.insert(u"tools"_s, tr("Tools"));
  labels.insert(u"clock"_s, tr("Clock"));
  labels.insert(u"next_alarm"_s, tr("Next alarm"));
  labels.insert(u"no_alarm"_s, tr("No alarm set"));
  labels.insert(u"default_no_song"_s, tr("Playing the default sound: no song available"));
  labels.insert(u"default_failed"_s, tr("Playing the default sound: the music could not be played"));
  AlarmBridge::Init(labels);

  QTimer::singleShot(kOutdatedTracksDelayMsec, this, &AlarmClock::UpdateOutdatedTracks);

}

bool AlarmClock::UsesTracks(const Alarm &alarm) {
  return alarm.source != Alarm::Source::Sound && alarm.source != Alarm::Source::Radio;
}

void AlarmClock::UpdateOutdatedTracks() {

  const qint64 now = QDateTime::currentSecsSinceEpoch();
  const AlarmList alarms = AlarmBridge::Alarms();
  for (const Alarm &alarm : alarms) {
    if (!alarm.enabled || !UsesTracks(alarm)) continue;
    const bool changing = alarm.source == Alarm::Source::Random || alarm.source == Alarm::Source::LastPlayed || alarm.source == Alarm::Source::Playlist;
    if (alarm.tracks.isEmpty() || (changing && now - alarm.tracks_updated > kOutdatedTracksSeconds)) {
      UpdateTracks(alarm.id);
    }
  }

}

AlarmTrackSettings AlarmClock::TrackSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  AlarmTrackSettings settings;
  settings.copy = s.value(kCopyTracks, true).toBool();
  settings.tracks_per_alarm = s.value(kTracksPerAlarm, AlarmTrackSettings::kDefaultTracksPerAlarm).toInt();
  settings.maximum_total_mb = s.value(kMaximumTotalMb, AlarmTrackSettings::kDefaultMaximumTotalMb).toInt();
  s.endGroup();
  return settings.Normalized();

}

void AlarmClock::SetTrackSettings(const AlarmTrackSettings &settings) {

  const bool was_copying = TrackSettings().copy;
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kCopyTracks, settings.copy);
  s.setValue(kTracksPerAlarm, settings.tracks_per_alarm);
  s.setValue(kMaximumTotalMb, settings.maximum_total_mb);
  s.endGroup();
  s.sync();

  // New limits only apply to the next copies; switching copying on or off updates the songs of every alarm right away.
  if (settings.copy == was_copying) return;
  if (!settings.copy) DeleteCopies();
  const AlarmList alarms = AlarmBridge::Alarms();
  for (const Alarm &alarm : alarms) {
    if (UsesTracks(alarm)) UpdateTracks(alarm.id);
  }

}

QString AlarmClock::TracksRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/alarms"_s;
}

QString AlarmClock::TracksDirectory(const QString &id) {

  // The ID is generated by the app, but keep it from reaching outside the alarms directory anyway.
  QString name = id;
  name.remove(u'/').remove(u'\\').remove(u'.');
  return TracksRoot() + u'/' + name;

}

qint64 AlarmClock::CopiesSize() {
  return AlarmTracks::DirectorySize(TracksRoot());
}

void AlarmClock::DeleteCopies() {

  // Copies that are still being made are dropped when they finish.
  const AlarmList alarms = AlarmBridge::Alarms();
  for (const Alarm &alarm : alarms) ++generations_[alarm.id];
  const QStringList ids = generations_.keys();
  for (const QString &id : ids) ++generations_[id];
  QDir(TracksRoot()).removeRecursively();

  const QString root = TracksRoot() + u'/';
  for (const Alarm &alarm : alarms) {
    if (std::none_of(alarm.tracks.cbegin(), alarm.tracks.cend(), [&root](const QString &track) { return track.startsWith(root); })) continue;
    Alarm stored = AlarmBridge::FindAlarm(alarm.id);
    if (stored.id.isEmpty()) continue;
    stored.tracks.clear();
    stored.tracks_updated = 0;
    AlarmBridge::SaveAlarm(stored);
    Q_EMIT TracksUpdated(stored.id);
  }
  qLog(Debug) << "Deleted the songs copied for alarms";

}

void AlarmClock::UpdateTracks(const QString &id) {

  const Alarm alarm = AlarmBridge::FindAlarm(id);
  if (alarm.id.isEmpty()) return;
  const int generation = ++generations_[id];
  const AlarmTrackSettings settings = TrackSettings();
  const QString directory = TracksDirectory(id);
  // Every update writes new file names, so concurrent updates for the same alarm never overwrite or delete each other's copies.
  const QString prefix = QString::number(QDateTime::currentMSecsSinceEpoch()) + u'_' + QString::number(generation) + u'_';

  QFutureWatcher<QStringList> *watcher = new QFutureWatcher<QStringList>(this);
  QObject::connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, alarm, generation, directory, prefix, settings]() {
    watcher->deleteLater();
    const QStringList tracks = watcher->result();
    Alarm current = AlarmBridge::FindAlarm(alarm.id);
    // A newer update replaced this one, or the alarm was deleted or got another source while the songs were copied.
    if (generations_.value(alarm.id) != generation || current.id.isEmpty() || current.source != alarm.source || current.source_id != alarm.source_id || current.source_url != alarm.source_url) {
      // The alarm may still ring with interim copies of this update until the newer update saves its songs.
      RemoveCopies(directory, [&prefix, &directory, &current](const QString &file) { return file.startsWith(prefix) && !current.tracks.contains(directory + u'/' + file); });
      return;
    }
    current.tracks = tracks;
    current.tracks_updated = QDateTime::currentSecsSinceEpoch();
    AlarmBridge::SaveAlarm(current);
    interim_.remove(current.id);
    // Only now that the alarm refers to the new copies, or to the original songs when copying is off, can the old copies go.
    // An older update that is still copying removes its own files when it finishes.
    RemoveCopies(directory, [&directory, &tracks](const QString &file) { return !tracks.contains(directory + u'/' + file); });
    qLog(Info) << (settings.copy ? "Copied" : "Using") << current.tracks.count() << "song(s) for alarm" << current.id;
    Q_EMIT TracksUpdated(current.id);
  });
  const std::function<void(const QStringList &tracks)> interim = [this, alarm, generation](const QStringList &tracks) {
    QMetaObject::invokeMethod(this, [this, alarm, generation, tracks]() { SaveInterimTracks(alarm, generation, tracks); }, Qt::QueuedConnection);
  };
  watcher->setFuture(QtConcurrent::run(&AlarmClock::CopyTracks, alarm, collection_backend_, playlist_backend_, directory, prefix, settings, interim));

}

void AlarmClock::SaveInterimTracks(const Alarm &alarm, const int generation, const QStringList &tracks) {

  if (generations_.value(alarm.id) != generation) return;
  Alarm current = AlarmBridge::FindAlarm(alarm.id);
  if (current.id.isEmpty() || current.source != alarm.source || current.source_id != alarm.source_id || current.source_url != alarm.source_url) return;
  // Songs that were completely copied before are better than interim songs.
  if (!current.tracks.isEmpty() && !interim_.contains(current.id)) return;
  current.tracks = tracks;
  AlarmBridge::SaveAlarm(current);
  interim_.insert(current.id);
  qLog(Info) << "Alarm" << current.id << "can ring with" << tracks.count() << "song(s) while its songs are copied";
  Q_EMIT TracksUpdated(current.id);

}

void AlarmClock::RemoveCopies(const QString &directory, const std::function<bool(const QString &file)> &remove) {

  QDir dir(directory);
  const QStringList files = dir.entryList(QDir::Files);
  for (const QString &file : files) {
    if (remove(file)) dir.remove(file);
  }
  if (dir.isEmpty()) dir.removeRecursively();

}

void AlarmClock::RemoveTracks(const QString &id) {

  ++generations_[id];
  interim_.remove(id);
  QDir(TracksDirectory(id)).removeRecursively();

}

QStringList AlarmClock::CopyTracks(const Alarm &alarm, SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, const QString &directory, const QString &prefix, const AlarmTrackSettings &settings, const std::function<void(const QStringList &tracks)> &interim) {

  QList<QUrl> candidates;
  switch (alarm.source) {
    case Alarm::Source::Random:
      candidates = SearchUrls(collection_backend, SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, {}, SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title, kCandidateTracks));
      break;
    case Alarm::Source::LastPlayed:
      candidates = SearchUrls(collection_backend, SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And, {SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::PlayCount, SmartPlaylistSearchTerm::Operator::GreaterThan, 0)}, SmartPlaylistSearch::SortType::FieldDesc, SmartPlaylistSearchTerm::Field::LastPlayed, kCandidateTracks));
      break;
    case Alarm::Source::Playlist:{
      bool ok = false;
      const int playlist_id = alarm.source_id.toInt(&ok);
      if (ok) {
        const SongList songs = playlist_backend->GetPlaylistSongs(playlist_id);
        playlist_backend->Close();
        for (const Song &song : songs) candidates << song.url();
      }
      break;
    }
    case Alarm::Source::Song:
      candidates << QUrl(alarm.source_url);
      break;
    case Alarm::Source::Sound:
    case Alarm::Source::Radio:
      break;
  }

  const auto is_nas = [](const QString &source) {
#ifdef HAVE_NAS
    return Nas::IsPath(source);
#else
    Q_UNUSED(source)
    return false;
#endif
  };

  if (!settings.copy) {
    // AlarmService.java plays the original songs; it has no SMB client, so NAS songs can't ring without a copy.
    int nas_tracks = 0;
    const QStringList tracks = AlarmTracks::PlayableOriginals(candidates, settings.tracks_per_alarm, true, is_nas, &nas_tracks);
    if (nas_tracks > 0) qLog(Debug) << "Skipped" << nas_tracks << "NAS song(s) for alarm" << alarm.id << "because copying is off";
    return tracks;
  }

  // Copying takes a while, especially from a NAS, so an alarm that rings soon after it was set rings with the local originals until the copies are made.
  const QStringList originals = AlarmTracks::PlayableOriginals(candidates, settings.tracks_per_alarm, false, is_nas);
  if (!originals.isEmpty()) interim(originals);

  // New copies get new names, so a song that is ringing right now is not overwritten.
  QDir().mkpath(directory);
  QStringList tracks;
  qint64 total_size = 0;
  for (const QUrl &url : std::as_const(candidates)) {
    if (tracks.count() >= settings.tracks_per_alarm) break;
    const QString source = AlarmTracks::CopySource(url);
    if (source.isEmpty()) continue;
    QFile file(source);
    const qint64 size = file.size();
    if (!AlarmTracks::CopyFits(size, tracks.count(), total_size, settings)) continue;
    const QString suffix = QFileInfo(url.path()).suffix().toLower();
    const QString destination = directory + u'/' + prefix + QString::number(tracks.count()) + (suffix.isEmpty() ? QString() : u'.' + suffix);
    if (!file.copy(destination)) {
      qLog(Warning) << "Could not copy" << source << "for an alarm:" << file.errorString();
      QFile::remove(destination);
      continue;
    }
    tracks << destination;
    total_size += size;
    // Without local originals, for example for NAS songs, every copy lets the alarm ring with one more song.
    if (originals.isEmpty()) interim(tracks);
  }

  // Keep the previous copies when none of the songs could be copied, for example while the NAS is offline, or else the local originals.
  if (tracks.isEmpty() && UsesTracks(alarm)) {
    QStringList previous;
    for (const QString &track : alarm.tracks) {
      if (QFile::exists(track)) previous << track;
    }
    if (!previous.isEmpty()) return previous;
    return originals;
  }

  return tracks;

}
