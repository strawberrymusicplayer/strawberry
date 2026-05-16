/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QApplication>
#include <QThread>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "letraslyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://www.letras.mus.br/";
constexpr char kStartTag[] = "<div[^>]*>";
constexpr char kEndTag[] = "<\\/div>";
constexpr char kLyricsStart[] = "<div class=\"lyric-original\">";
}  // namespace

LetrasLyricsProvider::LetrasLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(u"letras.mus.br"_s, true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl LetrasLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + StringFixup(request.artist) + u'/' + StringFixup(request.title) + u'/');

}

void LetrasLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  // letras.mus.br's Akamai edge blocks generic browser User-Agents (Mozilla/Firefox/Chrome strings all return 403)
  // but lets through identifiable HTTP-client UAs in the `name/version (+url)` form.
  // Send a Strawberry-identifying UA instead of the default fake browser UA used by other HTML providers.
  const QUrl url = Url(request);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Strawberry/%1 (+https://www.strawberrymusicplayer.org)").arg(QCoreApplication::applicationVersion()));
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &HttpBaseRequest::HandleSSLErrors);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleLyricsReply(reply, id, request); });

  qLog(Debug) << name_ << "Sending request for" << url;

}

void LetrasLyricsProvider::HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  LyricsSearchResults results;

  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() >= 200) {
      reply->readAll();  // QTBUG-135641
    }
    if (reply->error() == QNetworkReply::ContentNotFoundError) {
      qLog(Debug) << name_ << "No lyrics for" << request.artist << request.album << request.title;
    }
    else {
      qLog(Error) << name_ << reply->errorString() << reply->error();
    }
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http_status_code < 200 || http_status_code > 207) {
      qLog(Error) << name_ << "Received HTTP code" << http_status_code;
      reply->readAll();  // QTBUG-135641
      return;
    }
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    qLog(Error) << name_ << "Empty reply received from server.";
    return;
  }

  const QString content = QString::fromUtf8(data);

  // Letras's slug router ambiguously resolves common titles -- e.g.
  // /pink-floyd/time/ redirects to the Pink Floyd song "Biding My Time" rather than the Dark Side track.
  // The page embeds the resolved identifiers in window.__pageArgs; verify track_name matches what was requested so we don't surface lyrics from a different song.
  static const QRegularExpression regex_track_name(u"\"track_name\"\\s*:\\s*\"([^\"]*)\""_s);
  const QRegularExpressionMatch track_match = regex_track_name.match(content);
  if (track_match.hasMatch()) {
    const QString resolved_title = track_match.captured(1);
    // Use prefix match so qualifiers Letras appends to the canonical title
    // (e.g. " (feat. Stéphane Grappelli)", " (Live)") don't cause a false reject.
    if (!resolved_title.startsWith(request.title, Qt::CaseInsensitive)) {
      qLog(Debug) << name_ << "Slug resolved to a different track" << resolved_title << "for requested" << request.title;
      return;
    }
  }

  const QString lyrics = ParseLyricsFromHTML(content, QRegularExpression(start_tag_), QRegularExpression(end_tag_), QRegularExpression(lyrics_start_), multiple_);
  if (lyrics.isEmpty() || lyrics.contains("we do not have the lyrics for"_L1, Qt::CaseInsensitive)) {
    qLog(Debug) << name_ << "No lyrics for" << request.artist << request.album << request.title;
    return;
  }

  qLog(Debug) << name_ << "Got lyrics for" << request.artist << request.album << request.title;

  results << LyricsSearchResult(lyrics);

}

QString LetrasLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  // letras.mus.br slugs are strictly lowercase ASCII alpha-num with hyphens for word breaks; punctuation in the title is dropped.
  static const QRegularExpression regex_illegal_characters(u"[^A-Za-z0-9 -]"_s);
  static const QRegularExpression regex_multiple_whitespaces(u" {2,}"_s);

  return Utilities::Transliterate(text)
    .remove(regex_illegal_characters)
    .replace(regex_multiple_whitespaces, u" "_s)
    .simplified()
    .replace(u' ', u'-')
    .toLower();

}
