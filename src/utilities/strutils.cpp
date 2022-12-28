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

  if (bytes > 0) {
    if (bytes <= 1000) {
      ret = QString::number(bytes) + " bytes";
    }
    else if (bytes <= 1000 * 1000) {
      ret = QString::asprintf("%.1f KB", static_cast<float>(bytes) / 1000.0F);
    }
    else if (bytes <= 1000 * 1000 * 1000) {
      ret = QString::asprintf("%.1f MB", static_cast<float>(bytes) / (1000.0F * 1000.0F));
    }
    else {
      ret = QString::asprintf("%.1f GB", static_cast<float>(bytes) / (1000.0F * 1000.0F * 1000.0F));
    }
  }
  return ret;

}

QString PrettySize(const QSize size) {
  return QString::number(size.width()) + "x" + QString::number(size.height());
}

QString PathWithoutFilenameExtension(const QString &filename) {
  if (filename.section('/', -1, -1).contains('.')) return filename.section('.', 0, -2);
  return filename;
}

QString FiddleFileExtension(const QString &filename, const QString &new_extension) {
  return PathWithoutFilenameExtension(filename) + "." + new_extension;
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
  for (int i = 0; i < ret.count(); ++i) ret[i].prepend(ret[i] + " = :");
  return ret;

}

QString DecodeHtmlEntities(const QString &text) {

  QString copy(text);
  copy.replace("&amp;", "&")
      .replace("&#38;", "&")
      .replace("&quot;", "\"")
      .replace("&#34;", "\"")
      .replace("&apos;", "'")
      .replace("&#39;", "'")
      .replace("&lt;", "<")
      .replace("&#60;", "<")
      .replace("&gt;", ">")
      .replace("&#62;", ">")
      .replace("&#x27;", "'");

  return copy;

}

QString ReplaceMessage(const QString &message, const Song &song, const QString &newline, const bool html_escaped) {

  QRegularExpression variable_replacer("[%][a-z]+[%]");
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

  qint64 index_of = copy.indexOf(QRegularExpression(" - (>|$)"));
  if (index_of >= 0) copy = copy.remove(index_of, 3);

  return copy;

}

QString ReplaceVariable(const QString &variable, const Song &song, const QString &newline, const bool html_escaped) {

  QString value = variable;

  if (variable == "%title%") {
    value = song.PrettyTitle();
  }
  else if (variable == "%album%") {
    value = song.album();
  }
  else if (variable == "%artist%") {
    value = song.artist();
  }
  else if (variable == "%albumartist%") {
    value = song.effective_albumartist();
  }
  else if (variable == "%track%") {
    value.setNum(song.track());
  }
  else if (variable == "%disc%") {
    value.setNum(song.disc());
  }
  else if (variable == "%year%") {
    value = song.PrettyYear();
  }
  else if (variable == "%originalyear%") {
    value = song.PrettyOriginalYear();
  }
  else if (variable == "%genre%") {
    value = song.genre();
  }
  else if (variable == "%composer%") {
    value = song.composer();
  }
  else if (variable == "%performer%") {
    value = song.performer();
  }
  else if (variable == "%grouping%") {
    value = song.grouping();
  }
  else if (variable == "%length%") {
    value = song.PrettyLength();
  }
  else if (variable == "%filename%") {
    value = song.basefilename();
  }
  else if (variable == "%url%") {
    value = song.url().toString();
  }
  else if (variable == "%playcount%") {
    value.setNum(song.playcount());
  }
  else if (variable == "%skipcount%") {
    value.setNum(song.skipcount());
  }
  else if (variable == "%rating%") {
    value = song.PrettyRating();
  }
  else if (variable == "%newline%") {
    return QString(newline);  // No HTML escaping, return immediately.
  }

  if (html_escaped) {
    value = value.toHtmlEscaped();
  }
  return value;

}

}  // namespace Utilities
