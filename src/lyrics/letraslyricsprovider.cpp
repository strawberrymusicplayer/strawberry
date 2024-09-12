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

#include <QApplication>
#include <QThread>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "letraslyricsprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kUrl[] = "https://www.letras.mus.br/winamp.php";
constexpr char kStartTag[] = "<div[^>]*>";
constexpr char kEndTag[] = "<\\/div>";
constexpr char kLyricsStart[] = "<div id=\"letra-cnt\">";
}  // namespace

LetrasLyricsProvider::LetrasLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(QStringLiteral("letras.mus.br"), true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl LetrasLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + "?musica="_L1 + StringFixup(request.artist) + "&artista="_L1 + StringFixup(request.title));

}

QString LetrasLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_illegal_characters(QStringLiteral("[^\\w0-9_,&\\-\\(\\) ]"));
  static const QRegularExpression regex_multiple_whitespaces(QStringLiteral(" {2,}"));

  return QString::fromLatin1(QUrl::toPercentEncoding(Utilities::Transliterate(text)
    .replace(regex_illegal_characters, QStringLiteral("_"))
    .replace(regex_multiple_whitespaces, QStringLiteral(" "))
    .simplified()
    .replace(u' ', u'-')
    .toLower()
    ));

}
