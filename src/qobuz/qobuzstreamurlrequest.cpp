/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "constants/timeconstants.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzstreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

QobuzStreamURLRequest::QobuzStreamURLRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      reply_(nullptr),
      media_url_(media_url),
      id_(id),
      song_id_(media_url.path().toInt()),
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
    Q_EMIT StreamURLFailure(id_, media_url_, error);
    return;
  }

  Process();

}

void QobuzStreamURLRequest::Process() {

  if (service_->app_id().isEmpty() || service_->app_secret().isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Missing Qobuz app ID or secret."));
    return;
  }

  if (!authenticated()) {
    need_login_ = true;
    Q_EMIT TryLogin();
    return;
  }
  GetStreamURL();

}

void QobuzStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Cancelled."));
  }

}

void QobuzStreamURLRequest::GetStreamURL() {

  ++tries_;

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  const quint64 timestamp = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());

  ParamList params_to_sign = ParamList() << Param(u"format_id"_s, QString::number(service_->format()))
                                         << Param(u"track_id"_s, QString::number(song_id_));

  std::sort(params_to_sign.begin(), params_to_sign.end());

  QString data_to_sign;
  data_to_sign += "trackgetFileUrl"_L1;
  for (const Param &param : std::as_const(params_to_sign)) {
    data_to_sign += param.first + param.second;
  }
  data_to_sign += QString::number(timestamp);
  data_to_sign += service_->app_secret();

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

  ParamList params = params_to_sign;
    params << Param(u"request_ts"_s, QString::number(timestamp));
    params << Param(u"request_sig"_s, signature);
    params << Param(u"user_auth_token"_s, service_->user_auth_token());

  std::sort(params.begin(), params.end());

  reply_ = CreateRequest(u"track/getFileUrl"_s, params);
  QObject::connect(reply_, &QNetworkReply::finished, this, &QobuzStreamURLRequest::StreamURLReceived);

}

void QobuzStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;

  Q_ASSERT(replies_.contains(reply_));
  replies_.removeAll(reply_);

  const JsonObjectResult json_object_result = ParseJsonObject(reply_);

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (!json_object_result.success()) {
    if (!authenticated() && service_->login_sent() && tries_ <= 1) {
      need_login_ = true;
      return;
    }
    Q_EMIT StreamURLFailure(id_, media_url_, json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;

  if (json_object.isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Empty json object."_s);
    return;
  }

  if (!json_object.contains("track_id"_L1)) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid Json reply, stream url is missing track_id."_s);
    return;
  }

  const int track_id = json_object["track_id"_L1].toInt();
  if (track_id != song_id_) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Incorrect track ID returned."_s);
    return;
  }

  if (!json_object.contains("mime_type"_L1) || !json_object.contains("url"_L1)) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid Json reply, stream url is missing url or mime_type."_s);
    return;
  }

  const QUrl url(json_object["url"_L1].toString());
  const QString mimetype = json_object["mime_type"_L1].toString();

  Song::FileType filetype(Song::FileType::Unknown);
  QMimeDatabase mimedb;
  const QStringList suffixes = mimedb.mimeTypeForName(mimetype).suffixes();
  for (const QString &suffix : suffixes) {
    filetype = Song::FiletypeByExtension(suffix);
    if (filetype != Song::FileType::Unknown) break;
  }
  if (filetype == Song::FileType::Unknown) {
    qLog(Debug) << "Qobuz: Unknown mimetype" << mimetype;
    filetype = Song::FileType::Stream;
  }

  if (!url.isValid()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Returned stream url is invalid."_s);
    return;
  }

  qint64 duration = -1;
  if (json_object.contains("duration"_L1)) {
    duration = json_object["duration"_L1].toInt() * kNsecPerSec;
  }
  int samplerate = -1;
  if (json_object.contains("sampling_rate"_L1)) {
    samplerate = static_cast<int>(json_object["sampling_rate"_L1].toDouble()) * 1000;
  }
  int bit_depth = -1;
  if (json_object.contains("bit_depth"_L1)) {
    bit_depth = static_cast<int>(json_object["bit_depth"_L1].toDouble());
  }

  Q_EMIT StreamURLSuccess(id_, media_url_, url, filetype, samplerate, bit_depth, duration);

}
