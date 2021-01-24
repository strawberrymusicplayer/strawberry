/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/iconloader.h"
#include "core/latch.h"

#include "widgets/infotextview.h"
#include "wikipediaartistbio.h"

const char *WikipediaArtistBio::kApiUrl = "https://en.wikipedia.org/w/api.php";
const int WikipediaArtistBio::kMinimumImageSize = 400;

WikipediaArtistBio::WikipediaArtistBio() : ArtistBioProvider(), network_(new NetworkAccessManager(this)) {}

WikipediaArtistBio::~WikipediaArtistBio() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

QNetworkReply *WikipediaArtistBio::CreateRequest(QList<Param> &params) {

  params << Param("format", "json");
  params << Param("action", "query");

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kApiUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QNetworkReply *reply = network_->get(req);
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleSSLErrors(QList<QSslError>)));
  replies_ << reply;

  return reply;

}

QByteArray WikipediaArtistBio::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() == QNetworkReply::NoError) {
      qLog(Error) << "Wikipedia artist biography error: Received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    else {
      qLog(Error) << "Wikipedia artist biography error:" << reply->error() << reply->errorString();
    }
  }

  return data;

}

QJsonObject WikipediaArtistBio::ExtractJsonObj(const QByteArray &data) {

  if (data.isEmpty()) return QJsonObject();

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    qLog(Error) << "Wikipedia artist biography error: Failed to parse json data:" << json_error.errorString();
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
    qLog(Error) << "Wikipedia artist biography error: Received empty Json document.";
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    qLog(Error) << "Wikipedia artist biography error: Json document is not an object.";
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    qLog(Error) << "Wikipedia artist biography error: Received empty Json object.";
    return QJsonObject();
  }

  return json_obj;

}

void WikipediaArtistBio::HandleSSLErrors(QList<QSslError>) {}

void WikipediaArtistBio::Start(const int id, const Song &metadata) {

  if (metadata.artist().isEmpty()) {
    emit Finished(id);
    return;
  }

  CountdownLatch *latch = new CountdownLatch;
  connect(latch, &CountdownLatch::Done, [this, id, latch](){
    latch->deleteLater();
    emit Finished(id);
  });

  GetImageTitles(id, metadata.artist(), latch);
  //GetArticle(id, metadata.artist(), latch);

}

void WikipediaArtistBio::GetArticle(const int id, const QString &artist, CountdownLatch *latch) {

  latch->Wait();

  ParamList params = ParamList() << Param("titles", artist)
                                 << Param("prop", "extracts");

  QNetworkReply *reply = CreateRequest(params);
  connect(reply, &QNetworkReply::finished, [this, reply, id, latch]() { GetArticleReply(reply, id, latch); });

}

void WikipediaArtistBio::GetArticleReply(QNetworkReply *reply, const int id, CountdownLatch *latch) {

  reply->deleteLater();
  replies_.removeAll(reply);

  QJsonObject json_obj = ExtractJsonObj(GetReplyData(reply));

  QString title;
  QString text;
  if (!json_obj.isEmpty() && json_obj.contains("query") && json_obj["query"].isObject()) {
    json_obj = json_obj["query"].toObject();
    if (json_obj.contains("pages") && json_obj["pages"].isObject()) {
      QJsonObject value_pages = json_obj["pages"].toObject();
      for (const QJsonValue value_page : value_pages) {
        if (!value_page.isObject()) continue;
        QJsonObject obj_page = value_page.toObject();
        if (!obj_page.contains("title") || !obj_page.contains("extract")) continue;
        title = obj_page["title"].toString();
        text = obj_page["extract"].toString();
      }
    }
  }

  CollapsibleInfoPane::Data info_data;
  info_data.id_ = title;
  info_data.title_ = tr("Biography");
  info_data.type_ = CollapsibleInfoPane::Data::Type_Biography;
  info_data.icon_ = IconLoader::Load("wikipedia");
  InfoTextView *editor = new InfoTextView;
  editor->SetHtml(text);
  info_data.contents_ = editor;
  emit InfoReady(id, info_data);

  latch->CountDown();

}

void WikipediaArtistBio::GetImageTitles(const int id, const QString &artist, CountdownLatch *latch) {

  latch->Wait();

  ParamList params = ParamList() << Param("titles", artist)
                                 << Param("prop", "images")
                                 << Param("imlimit", "25");

  QNetworkReply *reply = CreateRequest(params);
  connect(reply, &QNetworkReply::finished, [this, reply, id, latch]() { GetImageTitlesFinished(reply, id, latch); });

}

void WikipediaArtistBio::GetImageTitlesFinished(QNetworkReply *reply, const int id, CountdownLatch *latch) {

  reply->deleteLater();
  replies_.removeAll(reply);

  QJsonObject json_obj = ExtractJsonObj(GetReplyData(reply));

  QString title;
  QStringList titles;
  if (!json_obj.isEmpty() && json_obj.contains("query") && json_obj["query"].isObject()) {
    json_obj = json_obj["query"].toObject();
    if (json_obj.contains("pages") && json_obj["pages"].isObject()) {
      QJsonObject value_pages = json_obj["pages"].toObject();
      for (const QJsonValue value_page : value_pages) {
        if (!value_page.isObject()) continue;
        QJsonObject obj_page = value_page.toObject();
        if (!obj_page.contains("title") || !obj_page.contains("images") || !obj_page["images"].isArray()) continue;
        title = obj_page["title"].toString();
        QJsonArray array_images = obj_page["images"].toArray();
        for (const QJsonValue value_image : array_images) {
          if (!value_image.isObject()) continue;
          QJsonObject obj_image = value_image.toObject();
          if (!obj_image.contains("title")) continue;
          QString filename = obj_image["title"].toString();
          if (filename.endsWith(".jpg", Qt::CaseInsensitive) || filename.endsWith(".png", Qt::CaseInsensitive)) {
            titles << filename;
          }
        }
      }
    }
  }

  for (const QString &image_title : titles) {
    GetImage(id, image_title, latch);
  }

  latch->CountDown();

}

void WikipediaArtistBio::GetImage(const int id, const QString &title, CountdownLatch *latch) {

  latch->Wait();

  ParamList params2 = ParamList() << Param("titles", title)
                                  << Param("prop", "imageinfo")
                                  << Param("iiprop", "url|size");

  QNetworkReply *reply = CreateRequest(params2);
  connect(reply, &QNetworkReply::finished, [this, reply, id, latch]() { GetImageFinished(reply, id, latch); });

}

void WikipediaArtistBio::GetImageFinished(QNetworkReply *reply, const int id, CountdownLatch *latch) {

  reply->deleteLater();
  replies_.removeAll(reply);

  QJsonObject json_obj = ExtractJsonObj(GetReplyData(reply));

  if (!json_obj.isEmpty()) {
    QList<QUrl> urls = ExtractImageUrls(json_obj);
    for (const QUrl &url : urls) {
      emit ImageReady(id, url);
    }
  }

  latch->CountDown();

}

QList<QUrl> WikipediaArtistBio::ExtractImageUrls(QJsonObject json_obj) {

  QList<QUrl> urls;
  if (json_obj.contains("query") && json_obj["query"].isObject()) {
    json_obj = json_obj["query"].toObject();
    if (json_obj.contains("pages") && json_obj["pages"].isObject()) {
      QJsonObject value_pages = json_obj["pages"].toObject();
      for (const QJsonValue value_page : value_pages) {
        if (!value_page.isObject()) continue;
        QJsonObject obj_page = value_page.toObject();
        if (!obj_page.contains("title") || !obj_page.contains("imageinfo") || !obj_page["imageinfo"].isArray()) continue;
        QJsonArray array_images = obj_page["imageinfo"].toArray();
        for (const QJsonValue value_image : array_images) {
          if (!value_image.isObject()) continue;
          QJsonObject obj_image = value_image.toObject();
          if (!obj_image.contains("url") || !obj_image.contains("width") || !obj_image.contains("height")) continue;
          QUrl url(obj_image["url"].toString());
          const int width = obj_image["width"].toInt();
          const int height = obj_image["height"].toInt();
          if (!url.isValid() || width < kMinimumImageSize || height < kMinimumImageSize) continue;
          urls << url;
        }
      }
    }
  }
  return urls;

}
