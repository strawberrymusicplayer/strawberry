/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QApplication>
#include <QThread>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "lyricssearchrequest.h"
#include "songlyricscomlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://www.songlyrics.com/";
constexpr char kStartTag[] = "<p[^>]*>";
constexpr char kEndTag[] = "<\\/p>";
constexpr char kLyricsStart[] = "<p id=\"songLyricsDiv\"[^>]+>";
}  // namespace

SongLyricsComLyricsProvider::SongLyricsComLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(u"songlyrics.com"_s, true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl SongLyricsComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + StringFixup(request.artist) + QLatin1Char('/') + StringFixup(request.title) + "-lyrics/"_L1);

}

QString SongLyricsComLyricsProvider::StringFixup(QString text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_illegal_characters(u"[^\\w0-9\\- ]"_s);
  static const QRegularExpression regex_multiple_whitespaces(u" {2,}"_s);
  static const QRegularExpression regex_multiple_dashes(u"(-)\\1+"_s);

  return text.replace(u'/', u'-')
             .replace(u'\'', u'-')
             .remove(regex_illegal_characters)
             .replace(regex_multiple_whitespaces, u" "_s)
             .simplified()
             .replace(u' ', u'-')
             .replace(regex_multiple_dashes, u"-"_s)
             .toLower();

}
