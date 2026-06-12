/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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
#include <QPair>
#include <QSet>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>

#include "core/networkaccessmanager.h"
#include "core/taskmanager.h"
#include "core/iconloader.h"
#include "settings/radiosettingspage.h"
#include "radiobrowserservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

const QStringList RadioBrowserService::kServers = {
    u"de1.api.radio-browser.info"_s,
    u"de2.api.radio-browser.info"_s,
    u"nl1.api.radio-browser.info"_s,
    u"at1.api.radio-browser.info"_s
};

RadioBrowserService::RadioBrowserService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::RadioBrowser, u"Radio Browser"_s, IconLoader::Load(u"radiobrowser"_s), task_manager, network, parent),
      server_discovered_(false),
      has_pending_search_(false),
      has_pending_countries_(false),
      server_index_(0),
      servers_tried_(0) {}

RadioBrowserService::~RadioBrowserService() {
  Abort();
}

QUrl RadioBrowserService::Homepage() { return QUrl(u"https://www.radio-browser.info/"_s); }
QUrl RadioBrowserService::Donate() { return QUrl(u"https://www.radio-browser.info/"_s); }

void RadioBrowserService::Abort() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  for (const int task_id : std::as_const(pending_search_tasks_)) {
    task_manager_->SetTaskFinished(task_id);
  }
  pending_search_tasks_.clear();

  has_pending_search_ = false;
  has_pending_countries_ = false;

}

void RadioBrowserService::GetChannels() {
  // RadioBrowser has 50,000+ stations. We don't load them all.
  // Only emit empty list so the model doesn't break on refresh.
  Q_EMIT NewChannels();
}

void RadioBrowserService::DiscoverServer() {

  // The API guidelines ask clients to spread load across the mirrors, so start at a random server and advance round-robin on failure.
  server_index_ = QRandomGenerator::global()->bounded(static_cast<int>(kServers.size()));
  servers_tried_ = 0;

  TestServer(kServers.at(server_index_));

}

void RadioBrowserService::TestServer(const QString &hostname) {

  QUrl url;
  url.setScheme(u"https"_s);
  url.setHost(hostname);
  url.setPath(u"/json/stats"_s);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
  QNetworkReply *reply = network_->get(request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { ServerTestReply(reply); });

}

void RadioBrowserService::ServerTestReply(QNetworkReply *reply) {

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    ++servers_tried_;
    if (servers_tried_ < kServers.size()) {
      server_index_ = (server_index_ + 1) % kServers.size();
      TestServer(kServers.at(server_index_));
    }
    else {
      Q_EMIT SearchError(tr("No Radio Browser server available."));
    }
    return;
  }

  // Server works
  QUrl url;
  url.setScheme(u"https"_s);
  url.setHost(reply->url().host());
  server_url_ = url;
  server_discovered_ = true;

  // Execute pending search if any
  if (has_pending_search_) {
    has_pending_search_ = false;
    Search(pending_search_.query, pending_search_.country, pending_search_.tag, pending_search_.language, pending_search_.order, pending_search_.limit, pending_search_.offset, pending_search_.hide_broken);
  }

  // Execute pending countries fetch if any
  if (has_pending_countries_) {
    has_pending_countries_ = false;
    FetchCountries();
  }

}

void RadioBrowserService::Search(const QString &query,
                                  const QString &country,
                                  const QString &tag,
                                  const QString &language,
                                  const QString &order,
                                  const int limit,
                                  const int offset,
                                  const bool hide_broken) {

  if (!server_discovered_) {
    // Save search and discover server first
    pending_search_ = {query, country, tag, language, order, limit, offset, hide_broken};
    has_pending_search_ = true;
    DiscoverServer();
    return;
  }

  QUrl url(server_url_);
  url.setPath(u"/json/stations/search"_s);

  QUrlQuery url_query;
  if (!query.isEmpty()) url_query.addQueryItem(u"name"_s, query);
  if (!country.isEmpty()) url_query.addQueryItem(u"countrycode"_s, country);
  if (!tag.isEmpty()) url_query.addQueryItem(u"tag"_s, tag);
  if (!language.isEmpty()) url_query.addQueryItem(u"language"_s, language);
  url_query.addQueryItem(u"limit"_s, QString::number(limit));
  url_query.addQueryItem(u"offset"_s, QString::number(offset));
  if (hide_broken) url_query.addQueryItem(u"hidebroken"_s, u"true"_s);

  if (!order.isEmpty()) {
    url_query.addQueryItem(u"order"_s, order);
    if (order != u"name"_s) {
      url_query.addQueryItem(u"reverse"_s, u"true"_s);
    }
  }
  else {
    url_query.addQueryItem(u"order"_s, u"votes"_s);
    url_query.addQueryItem(u"reverse"_s, u"true"_s);
  }

  url.setQuery(url_query);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
  QNetworkReply *reply = network_->get(request);
  replies_ << reply;
  const int task_id = task_manager_->StartTask(tr("Searching Radio Browser"));
  pending_search_tasks_ << task_id;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id, limit]() { SearchReply(reply, task_id, limit); });

}

void RadioBrowserService::SearchReply(QNetworkReply *reply, const int task_id, const int limit) {

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();
  task_manager_->SetTaskFinished(task_id);
  pending_search_tasks_.removeAll(task_id);

  if (reply->error() != QNetworkReply::NoError) {
    // The server may have gone down since discovery; rediscover on the next search.
    server_discovered_ = false;
    Q_EMIT SearchError(tr("Radio Browser search failed: %1").arg(reply->errorString()));
    return;
  }

  const QJsonArray array = ExtractJsonArray(reply);

  if (array.isEmpty()) {
    Q_EMIT SearchFinished(RadioChannelList(), false);
    return;
  }

  RadioChannelList channels;
  for (const QJsonValue &value : array) {
    if (!value.isObject()) continue;
    const QJsonObject obj = value.toObject();

    const QString name = obj["name"_L1].toString().trimmed();
    if (name.isEmpty()) continue;

    // Prefer url_resolved over url
    QString stream_url = obj["url_resolved"_L1].toString();
    if (stream_url.isEmpty()) stream_url = obj["url"_L1].toString();
    if (stream_url.isEmpty()) continue;

    RadioChannel channel;
    channel.source = source_;
    channel.name = name;
    channel.url.setUrl(stream_url);
    channel.country = obj["country"_L1].toString().trimmed();
    channel.tags = obj["tags"_L1].toString().trimmed();
    channel.codec = obj["codec"_L1].toString().trimmed();

    const QString favicon = obj["favicon"_L1].toString();
    if (!favicon.isEmpty()) {
      channel.thumbnail_url.setUrl(favicon);
    }

    channels << channel;
  }

  const bool has_more = (array.size() == limit);
  Q_EMIT SearchFinished(channels, has_more);

}

void RadioBrowserService::FetchCountries() {

  if (!server_discovered_) {
    has_pending_countries_ = true;
    if (!has_pending_search_) {
      DiscoverServer();
    }
    return;
  }

  QUrl url(server_url_);
  url.setPath(u"/json/countrycodes"_s);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
  QNetworkReply *reply = network_->get(request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { CountriesReply(reply); });

}

void RadioBrowserService::CountriesReply(QNetworkReply *reply) {

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    // The server may have gone down since discovery; rediscover on the next request.
    server_discovered_ = false;
    Error(QStringLiteral("Fetching countries failed: %1").arg(reply->errorString()));
    return;
  }

  const QJsonArray array = ExtractJsonArray(reply);

  // Collect country codes that have stations
  QSet<QString> codes_with_stations;
  for (const QJsonValue &value : array) {
    if (!value.isObject()) continue;
    const QJsonObject obj = value.toObject();
    const QString code = obj["name"_L1].toString().toUpper();
    const int count = obj["stationcount"_L1].toInt();
    if (!code.isEmpty() && count > 0) {
      codes_with_stations.insert(code);
    }
  }

  // Filter the full country list to only include countries with stations
  const QList<QPair<QString, QString>> all_countries = RadioSettingsPage::CountryList();
  QList<QPair<QString, QString>> countries;
  for (const QPair<QString, QString> &entry : all_countries) {
    if (codes_with_stations.contains(entry.second)) {
      countries << entry;
    }
  }

  Q_EMIT CountriesLoaded(countries);

}
