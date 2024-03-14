/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/logging.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "letraslyricsprovider.h"

const char LetrasLyricsProvider::kUrl[] = "https://www.letras.mus.br/winamp.php";
const char LetrasLyricsProvider::kStartTag[] = "<div[^>]*>";
const char LetrasLyricsProvider::kEndTag[] = "<\\/div>";
const char LetrasLyricsProvider::kLyricsStart[] = "<div id=\"letra-cnt\">";

LetrasLyricsProvider::LetrasLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider("letras.mus.br", true, kStartTag, kEndTag, kLyricsStart, false, network, parent) {}

QUrl LetrasLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QString(kUrl) + QStringLiteral("?musica=") + StringFixup(request.artist) + "&artista=" + StringFixup(request.title));

}

QString LetrasLyricsProvider::StringFixup(const QString &text) {

  return QUrl::toPercentEncoding(Utilities::Transliterate(text)
    .replace(QRegularExpression("[^\\w0-9_,&\\-\\(\\) ]"), "_")
    .replace(QRegularExpression(" {2,}"), " ")
    .simplified()
    .replace(' ', '-')
    .toLower()
    );

}
