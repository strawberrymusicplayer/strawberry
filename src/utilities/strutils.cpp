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

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QMetaObject>
#include <QMetaEnum>

#include "strutils.h"
#include "core/song.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

QString PrettySize(const quint64 bytes) {

  QString ret;

  if (bytes > 0LL) {
    if (bytes <= 1000LL) {
      ret = QString::number(bytes) + " bytes"_L1;
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
  if (filename.section(u'/', -1, -1).contains(u'.')) return filename.section(u'.', 0, -2);
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
  for (int i = 0; i < ret.count(); ++i) ret[i].prepend(ret[i] + " = :"_L1);
  return ret;

}

QString DecodeHtmlEntities(const QString &text) {

  QString copy(text);
  copy.replace("&amp;"_L1, "&"_L1)
      .replace("&#38;"_L1, "&"_L1)
      .replace("&quot;"_L1, "\""_L1)
      .replace("&#34;"_L1, "\""_L1)
      .replace("&apos;"_L1, "'"_L1)
      .replace("&#39;"_L1, "'"_L1)
      .replace("&lt;"_L1, "<"_L1)
      .replace("&#60;"_L1, "<"_L1)
      .replace("&gt;"_L1, ">"_L1)
      .replace("&#62;"_L1, ">"_L1)
      .replace("&#x27;"_L1, "'"_L1);

  return copy;

}

QString ReplaceMessage(const QString &message, const Song &song, const QString &newline, const bool html_escaped) {

  static const QRegularExpression variable_replacer(u"[%][a-z]+[%]"_s);

  // Build the result in one pass, so variables in the values themselves, for example a title containing "%album%", are not replaced.
  QString copy;
  qsizetype pos = 0;
  QRegularExpressionMatchIterator match_iterator = variable_replacer.globalMatch(message);
  while (match_iterator.hasNext()) {
    const QRegularExpressionMatch match = match_iterator.next();
    copy += message.mid(pos, match.capturedStart() - pos);
    copy += ReplaceVariable(match.captured(0), song, newline, html_escaped);
    pos = match.capturedEnd();
  }
  copy += message.mid(pos);

  static const QRegularExpression regexp(u" - (>|$)"_s);
  qint64 index_of = copy.indexOf(regexp);
  if (index_of >= 0) copy = copy.remove(index_of, 3);

  return copy;

}

QString ReplaceVariable(const QString &variable, const Song &song, const QString &newline, const bool html_escaped) {

  QString value = variable;

  if (variable == "%title%"_L1) {
    value = song.PrettyTitle();
  }
  else if (variable == "%titlesort%"_L1) {
    value = song.titlesort();
  }
  else if (variable == "%album%"_L1) {
    value = song.album();
  }
  else if (variable == "%albumsort%"_L1) {
    value = song.albumsort();
  }
  else if (variable == "%artist%"_L1) {
    value = song.artist();
  }
  else if (variable == "%artistsort%"_L1) {
    value = song.artistsort();
  }
  else if (variable == "%albumartist%"_L1) {
    value = song.effective_albumartist();
  }
  else if (variable == "%albumartistsort%"_L1) {
    value = song.albumartistsort();
  }
  else if (variable == "%track%"_L1) {
    value.setNum(song.track());
  }
  else if (variable == "%disc%"_L1) {
    value.setNum(song.disc());
  }
  else if (variable == "%year%"_L1) {
    value = song.PrettyYear();
  }
  else if (variable == "%originalyear%"_L1) {
    value = song.PrettyOriginalYear();
  }
  else if (variable == "%genre%"_L1) {
    value = song.genre();
  }
  else if (variable == "%composer%"_L1) {
    value = song.composer();
  }
  else if (variable == "%composersort%"_L1) {
    value = song.composersort();
  }
  else if (variable == "%performer%"_L1) {
    value = song.performer();
  }
  else if (variable == "%performersort%"_L1) {
    value = song.performersort();
  }
  else if (variable == "%grouping%"_L1) {
    value = song.grouping();
  }
  else if (variable == "%length%"_L1) {
    value = song.PrettyLength();
  }
  else if (variable == "%filename%"_L1) {
    value = song.basefilename();
  }
  else if (variable == "%url%"_L1) {
    value = song.url().toString();
  }
  else if (variable == "%playcount%"_L1) {
    value.setNum(song.playcount());
  }
  else if (variable == "%skipcount%"_L1) {
    value.setNum(song.skipcount());
  }
  else if (variable == "%rating%"_L1) {
    value = song.PrettyRating();
  }
  else if (variable == "%newline%"_L1) {
    return QString(newline);  // No HTML escaping, return immediately.
  }

  if (html_escaped) {
    value = value.toHtmlEscaped();
  }
  return value;

}

QString StringListToHTML(const QStringList &string_list) {

  QString html;
  for (const QString &string : string_list) {
    html += string + "<br />"_L1;
  }

  return html;

}

QString UrlForLog(const QUrl &url) {

  return url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery).toString();

}

QString UrlForLog(const QByteArray &url) {

  // Local file URLs from the engine are not always encoded, and don't contain credentials.
  if (url.startsWith("file:")) return QString::fromUtf8(url);

  return UrlForLog(QUrl::fromEncoded(url));

}

QString RedactUrls(const QString &text) {

  // A URL ends at whitespace or quotes, or where another URL follows after a separator.
  static const QRegularExpression regex_url(u"[A-Za-z][A-Za-z0-9+.-]*://(?:(?![;,][A-Za-z][A-Za-z0-9+.-]*://)[^\\s\"'<>])*"_s);

  QString result = text;
  qsizetype offset = 0;
  QRegularExpressionMatch match;
  while ((match = regex_url.match(result, offset)).hasMatch()) {

    QString url_text = match.captured();

    // Punctuation at the end usually belongs to the text, not the URL.
    while (!url_text.isEmpty() && u".,;:!?)]"_s.contains(url_text.back())) {
      url_text.chop(1);
    }

    // Local file URLs don't contain credentials.
    if (url_text.startsWith("file:"_L1, Qt::CaseInsensitive)) {
      offset = match.capturedStart() + url_text.length();
      continue;
    }

    QString redacted_url;
    const QUrl url(url_text);
    if (url.isValid()) {
      redacted_url = UrlForLog(url);
    }
    else {
      // Remove the query and user info from the text when it can't be parsed as a URL.
      redacted_url = url_text;
      const qsizetype query_start = redacted_url.indexOf(u'?');
      if (query_start >= 0) redacted_url.truncate(query_start);
      const qsizetype authority_start = redacted_url.indexOf("://"_L1) + 3;
      const qsizetype path_start = redacted_url.indexOf(u'/', authority_start);
      const qsizetype userinfo_end = redacted_url.lastIndexOf(u'@', path_start < 0 ? -1 : path_start);
      if (userinfo_end >= authority_start) redacted_url.remove(authority_start, userinfo_end + 1 - authority_start);
    }

    result.replace(match.capturedStart(), url_text.length(), redacted_url);
    offset = match.capturedStart() + redacted_url.length();

  }

  return result;

}

}  // namespace Utilities
