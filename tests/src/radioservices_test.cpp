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
#include <QMimeData>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QPersistentModelIndex>
#include <QPixmapCache>
#include <QTreeView>
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
#include "radios/radiomodel.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"

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

RadioChannel MakeChannel(const Song::Source source, const QString &name, const QString &url = QString()) {

  RadioChannel channel;
  channel.source = source;
  channel.name = name;
  channel.url = QUrl(url.isEmpty() ? u"https://radio.example/"_s + name : url);
  return channel;

}

QModelIndex ServiceIndex(const QAbstractItemModel &model, const Song::Source source) {

  for (int row = 0; row < model.rowCount(); ++row) {
    const QModelIndex idx = model.index(row, 0);
    if (idx.data(RadioModel::Role_Source).value<Song::Source>() == source) return idx;
  }
  return QModelIndex();

}

// Regression test: the model was reset when a service was refreshed, which collapsed the whole tree, and it happened once for every service.
TEST(RadioServicesTest, RefreshKeepsTreeExpanded) {

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<NetworkAccessManager> network = make_shared<NetworkAccessManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  // The tree view can ask for the icons of the channels when it lays them out.
  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(nullptr);
  RadioServices radio_services(task_manager, network, database, albumcover_loader);

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

  QSortFilterProxyModel *model = radio_services.sort_model();
  QTreeView view;
  view.setModel(model);
  const QPersistentModelIndex somafm_index = ServiceIndex(*model, Song::Source::SomaFM);
  const QPersistentModelIndex radioparadise_index = ServiceIndex(*model, Song::Source::RadioParadise);
  ASSERT_TRUE(somafm_index.isValid());
  ASSERT_TRUE(radioparadise_index.isValid());
  view.expand(somafm_index);
  view.expand(radioparadise_index);
  // The channels of a service which didn't change are not replaced.
  const QPersistentModelIndex somafm_channel = model->index(0, 0, somafm_index);
  ASSERT_TRUE(somafm_channel.isValid());
  QSignalSpy spy_reset(model, &QAbstractItemModel::modelReset);

  // Radio Paradise changes, SomaFM stays the same.
  spy.clear();
  radioparadise.set_channel_names(QStringList() << u"Radio Paradise 2"_s << u"Radio Paradise 3"_s);
  radio_services.RefreshChannels();
  while (spy.count() < 2 && spy.wait(5000)) {}
  ASSERT_EQ(2, spy.count());

  EXPECT_EQ(0, spy_reset.count());
  ASSERT_TRUE(somafm_index.isValid());
  ASSERT_TRUE(radioparadise_index.isValid());
  EXPECT_TRUE(view.isExpanded(somafm_index));
  EXPECT_TRUE(view.isExpanded(radioparadise_index));
  EXPECT_TRUE(somafm_channel.isValid());
  EXPECT_EQ(2, model->rowCount(somafm_index));
  EXPECT_EQ(2, model->rowCount(radioparadise_index));
  EXPECT_EQ(u"Radio Paradise 2"_s, model->index(0, 0, radioparadise_index).data().toString());

  radio_services.RemoveService(&somafm);
  radio_services.RemoveService(&radioparadise);

}

TEST(RadioModelTest, SetChannelsReplacesChangedChannels) {

  RadioModel model(nullptr, nullptr);

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s, u"https://radio.example/old"_s));
  ASSERT_EQ(1, model.rowCount(QModelIndex()));
  const QModelIndex service = ServiceIndex(model, Song::Source::SomaFM);
  ASSERT_EQ(1, model.rowCount(service));

  // Only the URL changes.
  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s, u"https://radio.example/new"_s));

  const std::unique_ptr<QMimeData> mimedata(model.mimeData(QModelIndexList() << ServiceIndex(model, Song::Source::SomaFM)));
  ASSERT_TRUE(mimedata);
  EXPECT_EQ(QList<QUrl>() << QUrl(u"https://radio.example/new"_s), mimedata->urls());

}

TEST(RadioModelTest, SetChannelsRemovesLastChannels) {

  RadioModel model(nullptr, nullptr);

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s) << MakeChannel(Song::Source::SomaFM, u"Two"_s));
  ASSERT_EQ(2, model.rowCount(ServiceIndex(model, Song::Source::SomaFM)));

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s));

  EXPECT_EQ(1, model.rowCount(ServiceIndex(model, Song::Source::SomaFM)));

}

TEST(RadioModelTest, SetChannelsRemovesServiceWithoutChannels) {

  RadioModel model(nullptr, nullptr);

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s) << MakeChannel(Song::Source::RadioParadise, u"Two"_s));
  ASSERT_EQ(2, model.rowCount(QModelIndex()));

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::RadioParadise, u"Two"_s));

  EXPECT_EQ(1, model.rowCount(QModelIndex()));
  EXPECT_EQ(QList<Song::Source>() << Song::Source::RadioParadise, model.sources());
  EXPECT_FALSE(ServiceIndex(model, Song::Source::SomaFM).isValid());
  EXPECT_EQ(1, model.rowCount(ServiceIndex(model, Song::Source::RadioParadise)));

}

// A cover still being loaded when its channel is replaced is not delivered to the deleted item, and a new cover can be loaded for the new channel.
TEST(RadioModelTest, SetChannelsForgetsPendingCoversOfReplacedChannels) {

  QPixmapCache::clear();

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(nullptr);
  RadioModel model(albumcover_loader, nullptr);
  const AlbumCoverLoaderOptions options;

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s, u"https://radio.example/old"_s));
  const quint64 cover_id = albumcover_loader->LoadImageAsync(options, QImage()) + 1;
  static_cast<void>(model.data(model.index(0, 0, ServiceIndex(model, Song::Source::SomaFM)), Qt::DecorationRole));

  model.SetChannels(RadioChannelList() << MakeChannel(Song::Source::SomaFM, u"One"_s, u"https://radio.example/new"_s));

  QSignalSpy spy_data_changed(&model, &QAbstractItemModel::dataChanged);
  Q_EMIT albumcover_loader->AlbumCoverLoaded(cover_id, AlbumCoverLoaderResult());
  EXPECT_EQ(0, spy_data_changed.count());

  // Another request is made for the new channel.
  static_cast<void>(model.data(model.index(0, 0, ServiceIndex(model, Song::Source::SomaFM)), Qt::DecorationRole));
  EXPECT_EQ(cover_id + 2, albumcover_loader->LoadImageAsync(options, QImage()));

}

}  // namespace
