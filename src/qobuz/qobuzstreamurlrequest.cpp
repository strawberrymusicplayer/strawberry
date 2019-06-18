/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QFile>
#include <QDir>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QJsonValue>
#include <QJsonObject>
#include <QXmlStreamReader>

#include "core/logging.h"
#include "core/network.h"
#include "core/song.h"
#include "settings/qobuzsettingspage.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzstreamurlrequest.h"

QobuzStreamURLRequest::QobuzStreamURLRequest(QobuzService *service, NetworkAccessManager *network, const QUrl &original_url, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
    service_(service),
    reply_(nullptr),
    original_url_(original_url),
    song_id_(original_url.path().toInt()),
    tries_(0),
    need_login_(false) {}

QobuzStreamURLRequest::~QobuzStreamURLRequest() {

  if (reply_) {
    disconnect(reply_, 0, nullptr, 0);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

void QobuzStreamURLRequest::LoginComplete(bool success, QString error) {

  if (!need_login_) return;
  need_login_ = false;

  if (!success) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  Process();

}

void QobuzStreamURLRequest::Process() {

  if (app_id().isEmpty() || app_secret().isEmpty()) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, tr("Missing app ID or secret."));
    return;
  }

  if (!authenticated()) {
    need_login_ = true;
    emit TryLogin();
    return;
  }
  GetStreamURL();

}

void QobuzStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, tr("Cancelled."));
  }

}

void QobuzStreamURLRequest::GetStreamURL() {

  ++tries_;

  if (reply_) {
    disconnect(reply_, 0, nullptr, 0);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  quint64 timestamp = QDateTime::currentDateTime().toTime_t();

  ParamList params_to_sign = ParamList() << Param("format", QString::number(format()))
                                         << Param("track_id", QString::number(song_id_));

  std::sort(params_to_sign.begin(), params_to_sign.end());

  QString data_to_sign;
  data_to_sign += "trackgetFileUrl";
  for (const Param &param : params_to_sign) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    data_to_sign += param.first;
    data_to_sign += param.second;
  }
  data_to_sign += QString::number(timestamp);
  data_to_sign += app_secret();

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, '0').toLower();

  ParamList params = params_to_sign;
  params << Param("request_ts", QString::number(timestamp));
  params << Param("request_sig", signature);
  params << Param("user_auth_token", access_token());

  std::sort(params.begin(), params.end());
  
  qLog(Debug) << params;

  reply_ = CreateRequest(QString("track/getFileUrl"), params);
  connect(reply_, SIGNAL(finished()), this, SLOT(StreamURLReceived()));

}

void QobuzStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;
  disconnect(reply_, 0, nullptr, 0);
  reply_->deleteLater();

  QString error;

  QByteArray data = GetReplyData(reply_, error);
  if (data.isEmpty()) {
    reply_ = nullptr;
    if (!authenticated() && login_sent() && tries_ <= 1) {
      need_login_ = true;
      return;
    }
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }
  reply_ = nullptr;

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  if (!json_obj.contains("track_id")) {
    error = Error("Invalid Json reply, stream url is missing track_id.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  int track_id = json_obj["track_id"].toInt();
  if (track_id != song_id_) {
    error = Error("Incorrect track ID returned.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  if (!json_obj.contains("mime_type") || !json_obj.contains("url")) {
    error = Error("Invalid Json reply, stream url is missing url or mime_type.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  QUrl url(json_obj["url"].toString());
  QString mimetype = json_obj["mime_type"].toString();

  Song::FileType filetype(Song::FileType_Unknown);
  QMimeDatabase mimedb;
  for (QString suffix : mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes()) {
    filetype = Song::FiletypeByExtension(suffix);
    if (filetype != Song::FileType_Unknown) break;
  }
  if (filetype == Song::FileType_Unknown) {
    qLog(Debug) << "Qobuz: Unknown mimetype" << mimetype;
    filetype = Song::FileType_Stream;
  }

  if (!url.isValid()) {
    error = Error("Returned stream url is invalid.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, filetype, error);
    return;
  }

  emit StreamURLFinished(original_url_, url, filetype);

}
