/*
 * Strawberry Music Player
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QDate>
#include <QSize>
#include <QLocale>

#include "timeconstants.h"
#include "timeutils.h"

namespace Utilities {

QString PrettyTime(int seconds) {

  // last.fm sometimes gets the track length wrong, so you end up with negative times.
  seconds = qAbs(seconds);

  int hours = seconds / (60 * 60);
  int minutes = (seconds / 60) % 60;
  seconds %= 60;

  QString ret;
  if (hours > 0) ret = QString::asprintf("%d:%02d:%02d", hours, minutes, seconds);
  else ret = QString::asprintf("%d:%02d", minutes, seconds);

  return ret;

}

QString PrettyTimeDelta(const int seconds) {
  return (seconds >= 0 ? QLatin1Char('+') : QLatin1Char('-')) + PrettyTime(seconds);
}

QString PrettyTimeNanosec(const qint64 nanoseconds) {
  return PrettyTime(static_cast<int>(nanoseconds / kNsecPerSec));
}

QString WordyTime(const quint64 seconds) {

  quint64 days = seconds / (60LL * 60LL * 24LL);

  // TODO: Make the plural rules translatable
  QStringList parts;

  if (days > 0) parts << (days == 1 ? QObject::tr("1 day") : QObject::tr("%1 days").arg(days));
  parts << PrettyTime(static_cast<int>(seconds - days * 60 * 60 * 24));

  return parts.join(u' ');

}

QString WordyTimeNanosec(const quint64 nanoseconds) {
  return WordyTime(nanoseconds / kNsecPerSec);
}

QString Ago(const qint64 seconds_since_epoch, const QLocale &locale) {

  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime then = QDateTime::fromSecsSinceEpoch(seconds_since_epoch);
  const qint64 days_ago = then.date().daysTo(now.date());
  const QString time = then.time().toString(locale.timeFormat(QLocale::ShortFormat));

  if (days_ago == 0) return QObject::tr("Today") + QLatin1Char(' ') + time;
  if (days_ago == 1) return QObject::tr("Yesterday") + QLatin1Char(' ') + time;
  if (days_ago <= 7) return QObject::tr("%1 days ago").arg(days_ago);

  return then.date().toString(locale.dateFormat(QLocale::ShortFormat));

}

QString PrettyFutureDate(const QDate date) {

  const QDate now = QDate::currentDate();
  const qint64 delta_days = now.daysTo(date);

  if (delta_days < 0) return QString();
  if (delta_days == 0) return QObject::tr("Today");
  if (delta_days == 1) return QObject::tr("Tomorrow");
  if (delta_days <= 7) return QObject::tr("In %1 days").arg(delta_days);
  if (delta_days <= 14) return QObject::tr("Next week");

  return QObject::tr("In %1 weeks").arg(delta_days / 7);

}

QDateTime ParseRFC822DateTime(const QString &text) {

  static const QRegularExpression regexp(QStringLiteral("(\\d{1,2}) (\\w{3,12}) (\\d+) (\\d{1,2}):(\\d{1,2}):(\\d{1,2})"));
  QRegularExpressionMatch re_match = regexp.match(text);
  if (!re_match.hasMatch()) {
    return QDateTime();
  }

  enum class MatchNames { DAYS = 1, MONTHS, YEARS, HOURS, MINUTES, SECONDS };

  QMap<QString, int> monthmap;
  monthmap[QStringLiteral("Jan")] = 1;
  monthmap[QStringLiteral("Feb")] = 2;
  monthmap[QStringLiteral("Mar")] = 3;
  monthmap[QStringLiteral("Apr")] = 4;
  monthmap[QStringLiteral("May")] = 5;
  monthmap[QStringLiteral("Jun")] = 6;
  monthmap[QStringLiteral("Jul")] = 7;
  monthmap[QStringLiteral("Aug")] = 8;
  monthmap[QStringLiteral("Sep")] = 9;
  monthmap[QStringLiteral("Oct")] = 10;
  monthmap[QStringLiteral("Nov")] = 11;
  monthmap[QStringLiteral("Dec")] = 12;
  monthmap[QStringLiteral("January")] = 1;
  monthmap[QStringLiteral("February")] = 2;
  monthmap[QStringLiteral("March")] = 3;
  monthmap[QStringLiteral("April")] = 4;
  monthmap[QStringLiteral("May")] = 5;
  monthmap[QStringLiteral("June")] = 6;
  monthmap[QStringLiteral("July")] = 7;
  monthmap[QStringLiteral("August")] = 8;
  monthmap[QStringLiteral("September")] = 9;
  monthmap[QStringLiteral("October")] = 10;
  monthmap[QStringLiteral("November")] = 11;
  monthmap[QStringLiteral("December")] = 12;

  const QDate date(re_match.captured(static_cast<int>(MatchNames::YEARS)).toInt(), monthmap[re_match.captured(static_cast<int>(MatchNames::MONTHS))], re_match.captured(static_cast<int>(MatchNames::DAYS)).toInt());

  const QTime time(re_match.captured(static_cast<int>(MatchNames::HOURS)).toInt(), re_match.captured(static_cast<int>(MatchNames::MINUTES)).toInt(), re_match.captured(static_cast<int>(MatchNames::SECONDS)).toInt());

  return QDateTime(date, time);

}

}  // namespace Utilities
