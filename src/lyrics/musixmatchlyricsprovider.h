/*
 * Strawberry Music Player
 * Copyright 2020-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MUSIXMATCHLYRICSPROVIDER_H
#define MUSIXMATCHLYRICSPROVIDER_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit MusixmatchLyricsProvider(NetworkAccessManager *network, QObject *parent = nullptr);
  ~MusixmatchLyricsProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

 private:
  struct LyricsSearchContext {
    explicit LyricsSearchContext() : id(-1) {}
    int id;
    QString artist;
    QString album;
    QString title;
    QList<QUrl> requests_lyrics_;
    LyricsSearchResults results;
  };

  using LyricsSearchContextPtr = std::shared_ptr<LyricsSearchContext>;

  QString StringFixup(QString string);
  bool SendSearchRequest(LyricsSearchContextPtr search);
  bool CreateLyricsRequest(LyricsSearchContextPtr search);
  bool CreateLyricsRequest(LyricsSearchContextPtr search, const QUrl &url);
  bool SendLyricsRequest(LyricsSearchContextPtr search, const QUrl &url);
  void EndSearch(LyricsSearchContextPtr search, const QUrl &url = QUrl());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private slots:
  void HandleSearchReply(QNetworkReply *reply, LyricsSearchContextPtr search);
  void HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search, const QUrl &url);

 private:
  static const char *kApiUrl;
  static const char *kApiKey;
  QList<LyricsSearchContextPtr> requests_search_;
  QList<QNetworkReply*> replies_;
  bool rate_limit_exceeded_;

};

#endif  // MUSIXMATCHLYRICSPROVIDER_H
