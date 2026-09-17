/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  bool has_compiled_api_credentials() const override;
  bool supports_custom_api_credentials() const override { return true; }
  QString api_credentials_id_label() const override { return tr("App ID"); }
  QString api_credentials_secret_label() const override { return tr("Signature secret"); }
  QString api_credentials_settings_group() const override;
  QString api_credentials_use_custom_key() const override;
  QString api_credentials_id_key() const override;
  QString api_credentials_secret_key() const override;

  void ReloadSettings() override;

 private:
  struct LyricsSearchContext {
    explicit LyricsSearchContext() : id(-1), token_retried(false) {}
    int id;
    LyricsSearchRequest request;
    LyricsSearchResults results;
    bool token_retried;
  };

  using LyricsSearchContextPtr = SharedPtr<LyricsSearchContext>;

  QUrl SignedUrl(const QString &endpoint, const ParamList &params) const;
  QNetworkReply *CreateAndroidGetRequest(const QUrl &url);
  void RequestUserToken();
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void SendLyricsRequest(LyricsSearchContextPtr search);
  bool RetryLyricsRequestWithFreshToken(LyricsSearchContextPtr search);
  void EndSearch(LyricsSearchContextPtr search);

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleTokenReply(QNetworkReply *reply);
  void HandleLyricsReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search);

 private:
  QList<LyricsSearchContextPtr> requests_search_;
  QList<LyricsSearchContextPtr> pending_token_requests_;
  QString client_id_;
  QString client_secret_;
  QString user_token_;
  bool requesting_user_token_;

  Q_DISABLE_COPY_MOVE(MusixmatchLyricsProvider)
};

#endif  // MUSIXMATCHLYRICSPROVIDER_H
