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

#ifndef LYRICSCOMLYRICSPROVIDER_H
#define LYRICSCOMLYRICSPROVIDER_H

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"

class QNetworkReply;
class NetworkAccessManager;

class LyricsComLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit LyricsComLyricsProvider(NetworkAccessManager *network, QObject *parent = nullptr);
  ~LyricsComLyricsProvider() override;

  bool StartSearch(const int id, const LyricsSearchRequest &request) override;
  void CancelSearch(const int id) override;

 private:
  void SendSearchRequest(const int id, const LyricsSearchRequest &request);
  void CreateLyricsRequest(const int id, const LyricsSearchRequest &request);
  void SendLyricsRequest(const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title, QUrl url = QUrl());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  static QString StringFixup(QString string);

 private slots:
  void HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request);
  void HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title);

 private:
  static const char *kApiUrl;
  static const char *kLyricsUrl;
  static const char *kUID;
  static const char *kTokenB64;
  QList<QNetworkReply*> replies_;
  bool use_api_;
};

#endif  // LYRICSCOMLYRICSPROVIDER_H
