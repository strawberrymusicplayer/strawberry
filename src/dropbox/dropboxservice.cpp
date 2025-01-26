/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QUrl>
#include <QTimer>

#include "constants/dropboxsettings.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/database.h"
#include "core/urlhandlers.h"
#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "streaming/cloudstoragestreamingservice.h"
#include "dropboxservice.h"
#include "dropboxurlhandler.h"
#include "dropboxsongsrequest.h"
#include "dropboxstreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;
using namespace DropboxSettings;

const Song::Source DropboxService::kSource = Song::Source::Dropbox;

namespace {
constexpr char kClientIDB64[] = "Zmx0b2EyYzRwaGo2eHlw";
constexpr char kClientSecretB64[] = "emo3em5jNnNpM3Ftd2s3";
constexpr char kOAuthRedirectUrl[] = "http://localhost/";
constexpr char kOAuthAuthorizeUrl[] = "https://www.dropbox.com/1/oauth2/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://api.dropboxapi.com/1/oauth2/token";
}  // namespace

DropboxService::DropboxService(const SharedPtr<TaskManager> task_manager,
                               const SharedPtr<Database> database,
                               const SharedPtr<NetworkAccessManager> network,
                               const SharedPtr<UrlHandlers> url_handlers,
                               const SharedPtr<TagReaderClient> tagreader_client,
                               const SharedPtr<AlbumCoverLoader> albumcover_loader,
                               QObject *parent)
    : CloudStorageStreamingService(task_manager, database, tagreader_client, albumcover_loader, Song::Source::Dropbox, u"Dropbox"_s, u"dropbox"_s, QLatin1String(kSettingsGroup), parent),
      network_(network),
      oauth_(new OAuthenticator(network, this)),
      songs_request_(new DropboxSongsRequest(network, collection_backend_, this, this)),
      enabled_(false),
      next_stream_url_request_id_(0) {

  url_handlers->Register(new DropboxUrlHandler(task_manager, this, this));

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthAuthorizeUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)));
  oauth_->set_use_local_redirect_server(true);
  oauth_->set_random_port(true);

  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &DropboxService::OAuthFinished);

  DropboxService::ReloadSettings();
  oauth_->LoadSession();

}

bool DropboxService::authenticated() const {

  return oauth_->authenticated();

}

void DropboxService::Exit() {

  wait_for_exit_ << &*collection_backend_;
  QObject::connect(&*collection_backend_, &CollectionBackend::ExitFinished, this, &DropboxService::ExitReceived);
  collection_backend_->ExitAsync();

}

void DropboxService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void DropboxService::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  enabled_ = s.value(kEnabled, false).toBool();
  s.endGroup();

}

void DropboxService::Authenticate() {

  oauth_->Authenticate();

}

void DropboxService::ClearSession() {

  oauth_->ClearSession();
}

void DropboxService::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    Q_EMIT LoginFinished(true);
    Q_EMIT LoginSuccess();
  }
  else {
    Q_EMIT LoginFailure(error);
    Q_EMIT LoginFinished(false);
  }

}

QByteArray DropboxService::authorization_header() const {
  return oauth_->authorization_header();
}

void DropboxService::Start() {
  songs_request_->GetFolderList();
}

void DropboxService::Reset() {

  collection_backend_->DeleteAll();

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.remove("cursor");
  s.endGroup();

  if (authenticated()) {
    Start();
  }

}

uint DropboxService::GetStreamURL(const QUrl &url, QString &error) {

  if (!authenticated()) {
    error = tr("Not authenticated with Dropbox.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  DropboxStreamURLRequestPtr stream_url_request = DropboxStreamURLRequestPtr(new DropboxStreamURLRequest(network_, this, id, url));
  stream_url_requests_.insert(id, stream_url_request);
  QObject::connect(&*stream_url_request, &DropboxStreamURLRequest::StreamURLRequestFinished, this, &DropboxService::StreamURLRequestFinishedSlot);
  stream_url_request->Process();

  return id;

}

void DropboxService::StreamURLRequestFinishedSlot(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error) {

  if (!stream_url_requests_.contains(id)) return;
  DropboxStreamURLRequestPtr stream_url_request = stream_url_requests_.take(id);

  Q_EMIT StreamURLRequestFinished(id, media_url, success, stream_url, error);

}
