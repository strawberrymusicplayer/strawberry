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
#include <utility>

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
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "utilities/timeconstants.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzstreamurlrequest.h"

using namespace Qt::StringLiterals;

QobuzStreamURLRequest::QobuzStreamURLRequest(QobuzService *service, SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      service_(service),
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

  if (app_id().isEmpty() || app_secret().isEmpty()) {
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

  quint64 timestamp = QDateTime::currentSecsSinceEpoch();

  ParamList params_to_sign = ParamList() << Param(QStringLiteral("format_id"), QString::number(format()))
                                         << Param(QStringLiteral("track_id"), QString::number(song_id_));

  std::sort(params_to_sign.begin(), params_to_sign.end());

  QString data_to_sign;
  data_to_sign += "trackgetFileUrl"_L1;
  for (const Param &param : std::as_const(params_to_sign)) {
    data_to_sign += param.first + param.second;
  }
  data_to_sign += QString::number(timestamp);
  data_to_sign += app_secret();

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

  ParamList params = params_to_sign;
    params << Param(QStringLiteral("request_ts"), QString::number(timestamp));
    params << Param(QStringLiteral("request_sig"), signature);
    params << Param(QStringLiteral("user_auth_token"), user_auth_token());

  std::sort(params.begin(), params.end());

  reply_ = CreateRequest(QStringLiteral("track/getFileUrl"), params);
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
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  if (!json_obj.contains("track_id"_L1)) {
    Error(QStringLiteral("Invalid Json reply, stream url is missing track_id."), json_obj);
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  int track_id = json_obj["track_id"_L1].toInt();
  if (track_id != song_id_) {
    Error(QStringLiteral("Incorrect track ID returned."), json_obj);
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  if (!json_obj.contains("mime_type"_L1) || !json_obj.contains("url"_L1)) {
    Error(QStringLiteral("Invalid Json reply, stream url is missing url or mime_type."), json_obj);
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  QUrl url(json_obj["url"_L1].toString());
  QString mimetype = json_obj["mime_type"_L1].toString();

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
    Error(QStringLiteral("Returned stream url is invalid."), json_obj);
    Q_EMIT StreamURLFailure(id_, media_url_, errors_.constFirst());
    return;
  }

  qint64 duration = -1;
  if (json_obj.contains("duration"_L1)) {
    duration = json_obj["duration"_L1].toInt() * kNsecPerSec;
  }
  int samplerate = -1;
  if (json_obj.contains("sampling_rate"_L1)) {
    samplerate = static_cast<int>(json_obj["sampling_rate"_L1].toDouble()) * 1000;
  }
  int bit_depth = -1;
  if (json_obj.contains("bit_depth"_L1)) {
    bit_depth = static_cast<int>(json_obj["bit_depth"_L1].toDouble());
  }

  Q_EMIT StreamURLSuccess(id_, media_url_, url, filetype, samplerate, bit_depth, duration);

}

void QobuzStreamURLRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Qobuz:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}
