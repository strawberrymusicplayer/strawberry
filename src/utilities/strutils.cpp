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

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QMetaObject>
#include <QMetaEnum>

#include "strutils.h"
#include "core/song.h"

namespace Utilities {

QString PrettySize(const quint64 bytes) {

  QString ret;

  if (bytes > 0LL) {
    if (bytes <= 1000LL) {
      ret = QString::number(bytes) + QLatin1String(" bytes");
    }
    else if (bytes <= 1000LL * 1000LL) {
      ret = QString::asprintf("%.1f KB", static_cast<float>(bytes) / 1000.0F);
    }
    else if (bytes <= 1000LL * 1000LL * 1000LL) {
      ret = QString::asprintf("%.1f MB", static_cast<float>(bytes) / (1000.0F * 1000.0F));
    }
    else {
      ret = QString::asprintf("%.1f GB", static_cast<float>(bytes) / (1000.0F * 1000.0F * 1000.0F));
    }
  }
  return ret;

}

QString PrettySize(const QSize size) {
  return QString::number(size.width()) + QLatin1Char('x') + QString::number(size.height());
}

QString PathWithoutFilenameExtension(const QString &filename) {
  if (filename.section(QLatin1Char('/'), -1, -1).contains(QLatin1Char('.'))) return filename.section(QLatin1Char('.'), 0, -2);
  return filename;
}

QString FiddleFileExtension(const QString &filename, const QString &new_extension) {
  return PathWithoutFilenameExtension(filename) + QLatin1Char('.') + new_extension;
}

const char *EnumToString(const QMetaObject &meta, const char *name, const int value) {

  int index = meta.indexOfEnumerator(name);
  if (index == -1) return "[UnknownEnum]";
  QMetaEnum metaenum = meta.enumerator(index);
  const char *result = metaenum.valueToKey(value);
  if (!result) return "[UnknownEnumValue]";
  return result;

}

QStringList Prepend(const QString &text, const QStringList &list) {

  QStringList ret(list);
  for (int i = 0; i < ret.count(); ++i) ret[i].prepend(text);
  return ret;

}

QStringList Updateify(const QStringList &list) {

  QStringList ret(list);
  for (int i = 0; i < ret.count(); ++i) ret[i].prepend(ret[i] + QLatin1String(" = :"));
  return ret;

}

QString DecodeHtmlEntities(const QString &text) {

  QString copy(text);
  copy.replace(QLatin1String("&amp;"), QLatin1String("&"))
      .replace(QLatin1String("&#38;"), QLatin1String("&"))
      .replace(QLatin1String("&quot;"), QLatin1String("\""))
      .replace(QLatin1String("&#34;"), QLatin1String("\""))
      .replace(QLatin1String("&apos;"), QLatin1String("'"))
      .replace(QLatin1String("&#39;"), QLatin1String("'"))
      .replace(QLatin1String("&lt;"), QLatin1String("<"))
      .replace(QLatin1String("&#60;"), QLatin1String("<"))
      .replace(QLatin1String("&gt;"), QLatin1String(">"))
      .replace(QLatin1String("&#62;"), QLatin1String(">"))
      .replace(QLatin1String("&#x27;"), QLatin1String("'"));

  return copy;

}

QString ReplaceMessage(const QString &message, const Song &song, const QString &newline, const bool html_escaped) {

  QRegularExpression variable_replacer(QStringLiteral("[%][a-z]+[%]"));
  QString copy(message);

  // Replace the first line
  qint64 pos = 0;
  QRegularExpressionMatch match;
  for (match = variable_replacer.match(message, pos); match.hasMatch(); match = variable_replacer.match(message, pos)) {
    pos = match.capturedStart();
    QStringList captured = match.capturedTexts();
    copy.replace(captured[0], ReplaceVariable(captured[0], song, newline, html_escaped));
    pos += match.capturedLength();
  }

  qint64 index_of = copy.indexOf(QRegularExpression(QStringLiteral(" - (>|$)")));
  if (index_of >= 0) copy = copy.remove(index_of, 3);

  return copy;

}

QString ReplaceVariable(const QString &variable, const Song &song, const QString &newline, const bool html_escaped) {

  QString value = variable;

  if (variable == QLatin1String("%title%")) {
    value = song.PrettyTitle();
  }
  else if (variable == QLatin1String("%album%")) {
    value = song.album();
  }
  else if (variable == QLatin1String("%artist%")) {
    value = song.artist();
  }
  else if (variable == QLatin1String("%albumartist%")) {
    value = song.effective_albumartist();
  }
  else if (variable == QLatin1String("%track%")) {
    value.setNum(song.track());
  }
  else if (variable == QLatin1String("%disc%")) {
    value.setNum(song.disc());
  }
  else if (variable == QLatin1String("%year%")) {
    value = song.PrettyYear();
  }
  else if (variable == QLatin1String("%originalyear%")) {
    value = song.PrettyOriginalYear();
  }
  else if (variable == QLatin1String("%genre%")) {
    value = song.genre();
  }
  else if (variable == QLatin1String("%composer%")) {
    value = song.composer();
  }
  else if (variable == QLatin1String("%performer%")) {
    value = song.performer();
  }
  else if (variable == QLatin1String("%grouping%")) {
    value = song.grouping();
  }
  else if (variable == QLatin1String("%length%")) {
    value = song.PrettyLength();
  }
  else if (variable == QLatin1String("%filename%")) {
    value = song.basefilename();
  }
  else if (variable == QLatin1String("%url%")) {
    value = song.url().toString();
  }
  else if (variable == QLatin1String("%playcount%")) {
    value.setNum(song.playcount());
  }
  else if (variable == QLatin1String("%skipcount%")) {
    value.setNum(song.skipcount());
  }
  else if (variable == QLatin1String("%rating%")) {
    value = song.PrettyRating();
  }
  else if (variable == QLatin1String("%newline%")) {
    return QString(newline);  // No HTML escaping, return immediately.
  }

  if (html_escaped) {
    value = value.toHtmlEscaped();
  }
  return value;

}

}  // namespace Utilities
