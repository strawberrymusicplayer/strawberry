/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QObject>
#include <QMimeDatabase>
#include <QPair>
#include <QByteArray>
#include <QString>
#include <QChar>
#include <QUrl>
#include <QDateTime>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonObject>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "utilities/timeconstants.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzstreamurlrequest.h"

QobuzStreamURLRequest::QobuzStreamURLRequest(QobuzService *service, NetworkAccessManager *network, const QUrl &original_url, const uint id, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      service_(service),
      reply_(nullptr),
      original_url_(original_url),
      id_(id),
      song_id_(original_url.path().toInt()),
      tries_(0),
      need_login_(false) {}

QobuzStreamURLRequest::~QobuzStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

void QobuzStreamURLRequest::LoginComplete(const bool success, const QString &error) {

  if (!need_login_) return;
  need_login_ = false;

  if (!success) {
    emit StreamURLFailure(id_, original_url_, error);
    return;
  }

  Process();

}

void QobuzStreamURLRequest::Process() {

  if (app_id().isEmpty() || app_secret().isEmpty()) {
    emit StreamURLFailure(id_, original_url_, tr("Missing Qobuz app ID or secret."));
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
    emit StreamURLFailure(id_, original_url_, tr("Cancelled."));
  }

}

void QobuzStreamURLRequest::GetStreamURL() {

  ++tries_;

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  quint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();

  ParamList params_to_sign = ParamList() << Param("format_id", QString::number(format()))
                                         << Param("track_id", QString::number(song_id_));

  std::sort(params_to_sign.begin(), params_to_sign.end());

  QString data_to_sign;
  data_to_sign += "trackgetFileUrl";
  for (const Param &param : params_to_sign) {
    data_to_sign += param.first + param.second;
  }
  data_to_sign += QString::number(timestamp);
  data_to_sign += app_secret().toUtf8();

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, '0').toLower();

  ParamList params = params_to_sign;
  params << Param("request_ts", QString::number(timestamp));
  params << Param("request_sig", signature);
  params << Param("user_auth_token", user_auth_token());

  std::sort(params.begin(), params.end());

  reply_ = CreateRequest(QString("track/getFileUrl"), params);
  QObject::connect(reply_, &QNetworkReply::finished, this, &QobuzStreamURLRequest::StreamURLReceived);

}

void QobuzStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;

  QByteArray data = GetReplyData(reply_);

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (data.isEmpty()) {
    if (!authenticated() && login_sent() && tries_ <= 1) {
      need_login_ = true;
      return;
    }
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  if (!json_obj.contains("track_id")) {
    Error("Invalid Json reply, stream url is missing track_id.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  int track_id = json_obj["track_id"].toInt();
  if (track_id != song_id_) {
    Error("Incorrect track ID returned.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  if (!json_obj.contains("mime_type") || !json_obj.contains("url")) {
    Error("Invalid Json reply, stream url is missing url or mime_type.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  QUrl url(json_obj["url"].toString());
  QString mimetype = json_obj["mime_type"].toString();

  Song::FileType filetype(Song::FileType::Unknown);
  QMimeDatabase mimedb;
  QStringList suffixes = mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes();
  for (const QString &suffix : suffixes) {
    filetype = Song::FiletypeByExtension(suffix);
    if (filetype != Song::FileType::Unknown) break;
  }
  if (filetype == Song::FileType::Unknown) {
    qLog(Debug) << "Qobuz: Unknown mimetype" << mimetype;
    filetype = Song::FileType::Stream;
  }

  if (!url.isValid()) {
    Error("Returned stream url is invalid.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  qint64 duration = -1;
  if (json_obj.contains("duration")) {
    duration = json_obj["duration"].toInt() * kNsecPerSec;
  }
  int samplerate = -1;
  if (json_obj.contains("sampling_rate")) {
    samplerate = static_cast<int>(json_obj["sampling_rate"].toDouble()) * 1000;
  }
  int bit_depth = -1;
  if (json_obj.contains("bit_depth")) {
    bit_depth = static_cast<int>(json_obj["bit_depth"].toDouble());
  }

  emit StreamURLSuccess(id_, original_url_, url, filetype, samplerate, bit_depth, duration);

}

void QobuzStreamURLRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Qobuz:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}
