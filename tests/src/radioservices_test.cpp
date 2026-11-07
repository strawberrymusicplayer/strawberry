/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>
#include <algorithm>

#include "gtest_include.h"

#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QIcon>
#include <QSignalSpy>
#include <QSortFilterProxyModel>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/memorydatabase.h"
#include "core/networkaccessmanager.h"
#include "radios/radioservices.h"
#include "radios/radioservice.h"
#include "radios/radiobackend.h"
#include "radios/radiochannel.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// Sends the channels it's given instead of getting them from the network, an empty list is what the services send when getting the channels failed.
class FakeRadioService : public RadioService {
 public:
  explicit FakeRadioService(const Song::Source source, const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr)
      : RadioService(source, Song::TextForSource(source), QIcon(), task_manager, network, parent) {}

  QUrl Homepage() override { return QUrl(); }
  QUrl Donate() override { return QUrl(); }

  void set_channel_names(const QStringList &channel_names) { channel_names_ = channel_names; }

  void GetChannels() override {
    RadioChannelList channels;
    for (const QString &channel_name : std::as_const(channel_names_)) {
      RadioChannel channel;
      channel.source = source_;
      channel.name = channel_name;
      channel.url = QUrl(u"https://radio.example/"_s + channel_name);
      channels << channel;
    }
    Q_EMIT NewChannels(channels);
  }

 private:
  QStringList channel_names_;
};

QStringList ChannelNames(const QList<QVariant> &arguments) {

  QStringList channel_names;
  const RadioChannelList channels = arguments.value(0).value<RadioChannelList>();
  for (const RadioChannel &channel : channels) {
    channel_names << channel.name;
  }
  std::sort(channel_names.begin(), channel_names.end());
  return channel_names;

}

int ChannelCount(const QSortFilterProxyModel *model) {

  int count = 0;
  for (int row = 0; row < model->rowCount(); ++row) {
    count += model->rowCount(model->index(row, 0));
  }
  return count;

}

// Regression test: refreshing deleted all the stored channels before getting the new ones, so the channels of a service which couldn't be reached were lost.
TEST(RadioServicesTest, RefreshKeepsChannelsOfFailedService) {

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<NetworkAccessManager> network = make_shared<NetworkAccessManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  RadioServices radio_services(task_manager, network, database, nullptr);

  // Replace the real services, so the test doesn't use the network.
  for (const Song::Source source : {Song::Source::SomaFM, Song::Source::RadioParadise, Song::Source::RadioBrowser}) {
    RadioService *service = radio_services.ServiceBySource(source);
    if (service) radio_services.RemoveService(service);
  }
  FakeRadioService somafm(Song::Source::SomaFM, task_manager, network);
  FakeRadioService radioparadise(Song::Source::RadioParadise, task_manager, network);
  radio_services.AddService(&somafm);
  radio_services.AddService(&radioparadise);

  QSignalSpy spy(&*radio_services.radio_backend(), &RadioBackend::NewChannels);

  somafm.set_channel_names(QStringList() << u"SomaFM 1"_s << u"SomaFM 2"_s);
  radioparadise.set_channel_names(QStringList() << u"Radio Paradise 1"_s);
  radio_services.RefreshChannels();
  while (spy.count() < 2 && spy.wait(5000)) {}
  ASSERT_EQ(2, spy.count());
  EXPECT_EQ(QStringList() << u"Radio Paradise 1"_s << u"SomaFM 1"_s << u"SomaFM 2"_s, ChannelNames(spy.last()));
  EXPECT_EQ(3, ChannelCount(radio_services.sort_model()));

  // SomaFM fails, Radio Paradise sends new channels.
  spy.clear();
  somafm.set_channel_names(QStringList());
  radioparadise.set_channel_names(QStringList() << u"Radio Paradise 2"_s);
  radio_services.RefreshChannels();
  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(QStringList() << u"Radio Paradise 2"_s << u"SomaFM 1"_s << u"SomaFM 2"_s, ChannelNames(spy.last()));
  EXPECT_EQ(3, ChannelCount(radio_services.sort_model()));

  // All the services fail, the stored channels are still there when loading them again.
  spy.clear();
  radioparadise.set_channel_names(QStringList());
  radio_services.RefreshChannels();
  radio_services.GetChannels();
  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(QStringList() << u"Radio Paradise 2"_s << u"SomaFM 1"_s << u"SomaFM 2"_s, ChannelNames(spy.last()));
  EXPECT_EQ(3, ChannelCount(radio_services.sort_model()));

  radio_services.RemoveService(&somafm);
  radio_services.RemoveService(&radioparadise);

}

}  // namespace
