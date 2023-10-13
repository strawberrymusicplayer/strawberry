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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "azlyricscomlyricsprovider.h"

const char AzLyricsComLyricsProvider::kUrl[] = "https://www.azlyrics.com/lyrics/";
const char AzLyricsComLyricsProvider::kStartTag[] = "<div>";
const char AzLyricsComLyricsProvider::kEndTag[] = "</div>";
const char AzLyricsComLyricsProvider::kLyricsStart[] = "<!-- Usage of azlyrics.com content by any third-party lyrics provider is prohibited by our licensing agreement. Sorry about that. -->";

AzLyricsComLyricsProvider::AzLyricsComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider("azlyrics.com", true, kStartTag, kEndTag, kLyricsStart, false, network, parent) {}

QUrl AzLyricsComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(kUrl + StringFixup(request.artist) + "/" + StringFixup(request.title) + ".html");

}

QString AzLyricsComLyricsProvider::StringFixup(const QString &text) {

  return Utilities::Transliterate(text).remove(QRegularExpression("[^\\w0-9\\-]")).toLower();

}
