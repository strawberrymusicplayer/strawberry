// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMPLAYBACK_ANDROID_H
#define ALARMPLAYBACK_ANDROID_H

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// The Qt side of a sounding alarm that AlarmService.java hands over to the Qt player, without Android or player classes, so it is tested natively.
// AlarmService.java sends a request with a token; the session acknowledges it once the player plays, and reports a failure when it can't or stops.
// A stop or a request with another token ends the session, so an acknowledgement never belongs to an alarm that was stopped or replaced.
class AlarmPlaybackSession {
 public:
  enum class Phase {
    Idle,
    // Finding the songs and checking that they can be reached.
    Preparing,
    // The player was asked to play and has not started yet.
    Starting,
    // The player plays, and AlarmService.java was told so.
    Playing
  };
  enum class Event { Playing, Paused, Stopped, Finished };
  enum class Action { None, Claim, Fail };

  Phase phase() const { return phase_; }
  QString token() const { return token_; }
  bool IsCurrent(const QString &token) const { return phase_ != Phase::Idle && token == token_; }

  // Returns true if a previous session was still active, which the caller must stop first.
  bool Start(const QString &token);
  // The songs were found; returns false if the session was stopped or replaced meanwhile.
  bool SongsReady(const QString &token);
  // Returns true if the token was the active session, which is now over.
  bool Stop(const QString &token);
  // Ends the session after a failure.
  void Reset();
  // What a change of the player state means for the alarm.
  Action HandleEvent(const Event event);

 private:
  Phase phase_ = Phase::Idle;
  QString token_;
};

namespace AlarmPlayback {

// The most songs the Qt player gets for one alarm.
constexpr int kMaximumSongs = 50;
// How many songs are checked before the Qt player starts, since checking a NAS file may take most of a second.
// A later song that can't be played makes the player stop, and AlarmService.java then plays the alarm itself.
constexpr int kCheckedSongs = 3;

// Returns the player volume in percent during the fade-in, with the same curve as AlarmService.java.
uint FadeVolume(const qint64 now_msec, const qint64 fade_start_msec, const qint64 fade_msec);

// Returns the indexes of the files that exist, at most maximum; an empty file name is a stream that is kept unchecked.
// Once checked files exist, the remaining files are kept without checking them.
// Once a NAS file fails while failure_serial() changes, the NAS is unreachable and its other files are skipped without waiting for each time-out.
QList<int> ReachableFiles(const QStringList &files, const int maximum, const int checked, const std::function<bool(const QString &file)> &exists, const std::function<bool(const QString &file)> &is_nas, const std::function<quint64()> &failure_serial);

}  // namespace AlarmPlayback

#endif  // ALARMPLAYBACK_ANDROID_H
