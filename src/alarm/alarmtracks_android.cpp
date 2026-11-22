// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QUrl>

#include "alarmtracks_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// A single song larger than this is not copied; it is most likely a long mix or an audio book.
constexpr qint64 kMaximumTrackSize = 80LL * 1024 * 1024;

}  // namespace

AlarmTrackSettings AlarmTrackSettings::Normalized() const {

  AlarmTrackSettings settings = *this;
  if (!AlarmTracks::TracksPerAlarmChoices().contains(settings.tracks_per_alarm)) settings.tracks_per_alarm = kDefaultTracksPerAlarm;
  if (!AlarmTracks::MaximumTotalMbChoices().contains(settings.maximum_total_mb)) settings.maximum_total_mb = kDefaultMaximumTotalMb;
  return settings;

}

namespace AlarmTracks {

QList<int> TracksPerAlarmChoices() {
  return {1, 3, 5, 10};
}

QList<int> MaximumTotalMbChoices() {
  return {50, 100, 200, 500, 1024};
}

QString CopySource(const QUrl &url) {

  if (url.isLocalFile()) return url.toLocalFile();
  if (url.scheme() == "content"_L1) return url.toString(QUrl::FullyEncoded);
  return QString();

}

QString OriginalSource(const QUrl &url) {

  if (url.scheme() == "http"_L1 || url.scheme() == "https"_L1) return url.toString(QUrl::FullyEncoded);
  return CopySource(url);

}

bool CopyFits(const qint64 size, const int count, const qint64 total_size, const AlarmTrackSettings &settings) {
  return count < settings.tracks_per_alarm && size > 0 && size <= kMaximumTrackSize && total_size + size <= settings.maximum_total_bytes();
}

qint64 DirectorySize(const QString &directory) {

  qint64 size = 0;
  QDirIterator it(directory, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    size += it.fileInfo().size();
  }
  return size;

}

}  // namespace AlarmTracks
