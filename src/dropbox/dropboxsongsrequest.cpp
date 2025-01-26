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

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "constants/dropboxsettings.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/networkaccessmanager.h"
#include "collection/collectionbackend.h"
#include "dropboxservice.h"
#include "dropboxbaserequest.h"
#include "dropboxsongsrequest.h"

using namespace Qt::Literals::StringLiterals;
using namespace DropboxSettings;

DropboxSongsRequest::DropboxSongsRequest(const SharedPtr<NetworkAccessManager> network, const SharedPtr<CollectionBackend> collection_backend, DropboxService *service, QObject *parent)
    : DropboxBaseRequest(network, service, parent),
      network_(network),
      collection_backend_(collection_backend),
      service_(service) {}

void DropboxSongsRequest::GetFolderList() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  QString cursor = s.value("cursor").toString();
  s.endGroup();

  QUrl url(QLatin1String(kApiUrl) + "/2/files/list_folder"_L1);
  QJsonObject json_object;

  if (cursor.isEmpty()) {
    json_object.insert("path"_L1, ""_L1);
    json_object.insert("recursive"_L1, true);
    json_object.insert("include_deleted"_L1, true);
  }
  else {
    url.setUrl(QLatin1String(kApiUrl) + "/2/files/list_folder/continue"_L1);
    json_object.insert("cursor"_L1, cursor);
  }

  QNetworkReply *reply = CreatePostRequest(url, json_object);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] { GetFolderListFinished(reply); });

}

void DropboxSongsRequest::GetFolderListFinished(QNetworkReply *reply) {

  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (json_object.contains("reset"_L1) && json_object["reset"_L1].toBool()) {
    qLog(Debug) << "Resetting Dropbox database";
    collection_backend_->DeleteAll();
  }

  {
    Settings s;
    s.beginGroup(kSettingsGroup);
    s.setValue("cursor", json_object["cursor"_L1].toString());
    s.endGroup();
  }

  const QJsonArray entires = json_object["entries"_L1].toArray();
  qLog(Debug) << "File list found:" << entires.size();

  QList<QUrl> urls_deleted;
  for (const QJsonValue &value_entry : entires) {
    if (!value_entry.isObject()) {
      continue;
    }
    const QJsonObject object_entry = value_entry.toObject();
    const QString tag = object_entry[".tag"_L1].toString();
    const QString path = object_entry["path_lower"_L1].toString();
    const qint64 size = object_entry["size"_L1].toInt();
    const QString server_modified = object_entry["server_modified"_L1].toString();

    QUrl url;
    url.setScheme(service_->url_scheme());
    url.setPath(path);

    if (tag == "deleted"_L1) {
      qLog(Debug) << "Deleting song with URL" << url;
      urls_deleted << url;
      continue;
    }

    if (tag == "folder"_L1) {
      continue;
    }

    if (DropboxService::IsSupportedFiletype(path)) {
      GetStreamURL(url, path, size, QDateTime::fromString(server_modified, Qt::ISODate).toSecsSinceEpoch());
    }

  }

  if (!urls_deleted.isEmpty()) {
    collection_backend_->DeleteSongsByUrlsAsync(urls_deleted);
  }

  if (json_object.contains("has_more"_L1) && json_object["has_more"_L1].isBool() && json_object["has_more"_L1].toBool()) {
    Settings s;
    s.beginGroup(kSettingsGroup);
    s.setValue("cursor", json_object["cursor"_L1].toVariant());
    s.endGroup();
    GetFolderList();
  }
  else {
    // Long-poll wait for changes.
    LongPollDelta();
  }

}

void DropboxSongsRequest::LongPollDelta() {

  if (!service_->authenticated()) {
    return;
  }

  Settings s;
  s.beginGroup(kSettingsGroup);
  const QString cursor = s.value("cursor").toString();
  s.endGroup();

  QJsonObject json_object;
  json_object.insert("cursor"_L1, cursor);
  json_object.insert("timeout"_L1, 30);

  QNetworkReply *reply = CreatePostRequest(QUrl(QLatin1String(kNotifyApiUrl) + "/2/files/list_folder/longpoll"_L1), json_object);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] { LongPollDeltaFinished(reply); });

}

void DropboxSongsRequest::LongPollDeltaFinished(QNetworkReply *reply) {

  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object["changes"_L1].toBool()) {
    qLog(Debug) << "Dropbox: Received changes...";
    GetFolderList();
  }
  else {
    bool ok = false;
    int backoff = json_object["backoff"_L1].toString().toInt(&ok);
    if (!ok) {
      backoff = 10;
    }
    QTimer::singleShot(backoff * 1000, this, &DropboxSongsRequest::LongPollDelta);
  }

}

void DropboxSongsRequest::GetStreamURL(const QUrl &url, const QString &path, const qint64 size, const qint64 mtime) {

  QNetworkReply *reply = GetTemporaryLink(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, path, size, mtime]() {
    GetStreamUrlFinished(reply, path, size, mtime);
  });

}

void DropboxSongsRequest::GetStreamUrlFinished(QNetworkReply *reply, const QString &filename, const qint64 size, const qint64 mtime) {

  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("link"_L1)) {
    Error(u"Missing link"_s);
    return;
  }

  const QUrl url = QUrl::fromEncoded(json_object["link"_L1].toVariant().toByteArray());

  service_->MaybeAddFileToDatabase(url, filename, size, mtime);

}

void DropboxSongsRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << service_name() << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  Q_EMIT ShowErrorDialog(error_message);

}
