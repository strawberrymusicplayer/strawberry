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

#include <QObject>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "lyricssearchrequest.h"
#include "songlyricscomlyricsprovider.h"

namespace {
constexpr char kUrl[] = "https://www.songlyrics.com/";
constexpr char kStartTag[] = "<p[^>]*>";
constexpr char kEndTag[] = "<\\/p>";
constexpr char kLyricsStart[] = "<p id=\"songLyricsDiv\"[^>]+>";
}  // namespace

SongLyricsComLyricsProvider::SongLyricsComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(QStringLiteral("songlyrics.com"), true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl SongLyricsComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + StringFixup(request.artist) + QLatin1Char('/') + StringFixup(request.title) + QStringLiteral("-lyrics/"));

}

QString SongLyricsComLyricsProvider::StringFixup(QString text) {

  return text.replace(QLatin1Char('/'), QLatin1Char('-'))
             .replace(QLatin1Char('\''), QLatin1Char('-'))
             .remove(QRegularExpression(QStringLiteral("[^\\w0-9\\- ]")))
             .replace(QRegularExpression(QStringLiteral(" {2,}")), QStringLiteral(" "))
             .simplified()
             .replace(QLatin1Char(' '), QLatin1Char('-'))
             .replace(QRegularExpression(QStringLiteral("(-)\\1+")), QStringLiteral("-"))
             .toLower();

}
