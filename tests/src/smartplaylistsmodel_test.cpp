// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <memory>

#include "core/memorydatabase.h"
#include "core/settings.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "smartplaylists/playlistquerygenerator.h"
#include "smartplaylists/smartplaylistsmodel.h"

using namespace Qt::Literals::StringLiterals;

TEST(SmartPlaylistsModelTest, CreateEditReloadAndDeletePreserveRules) {

  auto database = std::make_shared<MemoryDatabase>(nullptr);
  auto backend = std::make_shared<CollectionBackend>();
  backend->Init(database, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable),
                QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  Settings settings;
  settings.remove(u"SerializedSmartPlaylists"_s);
  SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::And,
                            {SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Artist, SmartPlaylistSearchTerm::Operator::Contains, u"Test Artist"_s)},
                            SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, 3);
  int custom_row = -1;
  {
    SmartPlaylistsModel model(backend);
    model.Init();
    custom_row = model.rowCount(QModelIndex());
    model.AddGenerator(std::make_shared<PlaylistQueryGenerator>(u"Owned smart playlist"_s, search, false));
    ASSERT_EQ(custom_row + 1, model.rowCount(QModelIndex()));
    search.terms_ << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::PlayCount, SmartPlaylistSearchTerm::Operator::GreaterThan, 2);
    search.limit_ = 7;
    model.UpdateGenerator(model.index(custom_row, 0), std::make_shared<PlaylistQueryGenerator>(u"Edited smart playlist"_s, search, true));
  }
  {
    SmartPlaylistsModel restarted(backend);
    restarted.Init();
    ASSERT_EQ(custom_row + 1, restarted.rowCount(QModelIndex()));
    const auto generator = std::dynamic_pointer_cast<PlaylistQueryGenerator>(restarted.CreateGenerator(restarted.index(custom_row, 0)));
    ASSERT_TRUE(generator);
    EXPECT_EQ(u"Edited smart playlist"_s, generator->name());
    EXPECT_TRUE(generator->is_dynamic());
    EXPECT_EQ(search, generator->search());
    restarted.DeleteGenerator(restarted.index(custom_row, 0));
    EXPECT_EQ(custom_row, restarted.rowCount(QModelIndex()));
  }
  {
    SmartPlaylistsModel restarted(backend);
    restarted.Init();
    EXPECT_EQ(custom_row, restarted.rowCount(QModelIndex()));
  }
  database->Close();

}
