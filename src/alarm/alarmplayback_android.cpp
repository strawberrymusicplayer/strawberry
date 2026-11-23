// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

#include "alarmplayback_android.h"

bool AlarmPlaybackSession::Start(const QString &token) {

  const bool replaced = phase_ != Phase::Idle;
  token_ = token;
  phase_ = Phase::Preparing;
  return replaced;

}

bool AlarmPlaybackSession::SongsReady(const QString &token) {

  if (phase_ != Phase::Preparing || token != token_) return false;
  phase_ = Phase::Starting;
  return true;

}

bool AlarmPlaybackSession::Stop(const QString &token) {

  if (!IsCurrent(token)) return false;
  Reset();
  return true;

}

void AlarmPlaybackSession::Reset() {

  phase_ = Phase::Idle;
  token_.clear();

}

AlarmPlaybackSession::Action AlarmPlaybackSession::HandleEvent(const Event event) {

  switch (phase_) {
    case Phase::Starting:
      if (event == Event::Playing) {
        phase_ = Phase::Playing;
        return Action::Claim;
      }
      // The player stops briefly between songs it can't play; only running out of songs is a failure here.
      if (event == Event::Finished) return Action::Fail;
      return Action::None;
    case Phase::Playing:
      // Pausing or stopping in the Qt app, or the end of the songs, hands the alarm back to AlarmService.java, which keeps it going until Stop or Snooze.
      return event == Event::Playing ? Action::None : Action::Fail;
    case Phase::Idle:
    case Phase::Preparing:
      break;
  }
  return Action::None;

}

namespace AlarmPlayback {

uint FadeVolume(const qint64 now_msec, const qint64 fade_start_msec, const qint64 fade_msec) {

  if (fade_msec <= 0) return 100;
  const double progress = std::clamp(static_cast<double>(now_msec - fade_start_msec) / static_cast<double>(fade_msec), 0.0, 1.0);
  return static_cast<uint>(std::max(1L, std::lround(progress * progress * 100.0)));

}

QList<int> ReachableFiles(const QStringList &files, const int maximum, const int checked, const std::function<bool(const QString &file)> &exists, const std::function<bool(const QString &file)> &is_nas, const std::function<quint64()> &failure_serial) {

  QList<int> reachable;
  bool nas_unreachable = false;
  int found = 0;
  for (int i = 0; i < files.count() && reachable.count() < maximum; ++i) {
    const QString &file = files[i];
    if (file.isEmpty() || (found >= checked && !(nas_unreachable && is_nas(file)))) {
      reachable << i;
      continue;
    }
    const bool nas = is_nas(file);
    if (nas && nas_unreachable) continue;
    const quint64 serial = nas ? failure_serial() : 0;
    if (exists(file)) {
      reachable << i;
      ++found;
    }
    else if (nas && failure_serial() != serial) {
      nas_unreachable = true;
    }
  }
  return reachable;

}

}  // namespace AlarmPlayback
