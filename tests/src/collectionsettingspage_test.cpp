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

#include "gtest_include.h"

#include <QComboBox>
#include <QSpinBox>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "collection/collectionmanager.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectiondirectorymodel.h"
#include "constants/collectionsettings.h"
#include "settings/collectionsettingspage.h"

using std::make_shared;
using std::make_unique;
using namespace Qt::Literals::StringLiterals;
using namespace CollectionSettings;

namespace {

class CollectionSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings().remove(QLatin1String(kSettingsGroup));
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    model_ = make_unique<CollectionModel>(backend_, nullptr);
    directory_model_ = make_unique<CollectionDirectoryModel>(backend_);
    page_ = make_unique<CollectionSettingsPage>(nullptr, nullptr, backend_, &*model_, &*directory_model_);
  }

  void TearDown() override {
    page_.reset();
    Settings().remove(QLatin1String(kSettingsGroup));
  }

  static void SaveCacheSizes(const int cache_size, const CacheSizeUnit cache_size_unit, const int disk_cache_size, const CacheSizeUnit disk_cache_size_unit) {
    Settings s;
    s.beginGroup(QLatin1String(kSettingsGroup));
    s.setValue(QLatin1String(kSettingsCacheSize), cache_size);
    s.setValue(QLatin1String(kSettingsCacheSizeUnit), static_cast<int>(cache_size_unit));
    s.setValue(QLatin1String(kSettingsDiskCacheSize), disk_cache_size);
    s.setValue(QLatin1String(kSettingsDiskCacheSizeUnit), static_cast<int>(disk_cache_size_unit));
    s.endGroup();
  }

  QSpinBox *SpinBox(const QString &name) const { return page_->findChild<QSpinBox*>(name); }
  QComboBox *ComboBox(const QString &name) const { return page_->findChild<QComboBox*>(name); }

  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> backend_;
  std::unique_ptr<CollectionModel> model_;
  std::unique_ptr<CollectionDirectoryModel> directory_model_;
  std::unique_ptr<CollectionSettingsPage> page_;
};

// Regression test: the size was set before the unit, so when the unit shown was still GB from a cancelled change, the size in MB was limited to the maximum for GB.
TEST_F(CollectionSettingsPageTest, LoadingAfterCancelledUnitChangeKeepsDiskCacheSize) {

  SaveCacheSizes(160, CacheSizeUnit::MB, 360, CacheSizeUnit::MB);
  page_->Load();

  QSpinBox *spinbox_disk_cache_size = SpinBox(u"spinbox_disk_cache_size"_s);
  QComboBox *combobox_disk_cache_size = ComboBox(u"combobox_disk_cache_size"_s);
  ASSERT_NE(nullptr, spinbox_disk_cache_size);
  ASSERT_NE(nullptr, combobox_disk_cache_size);
  ASSERT_EQ(360, spinbox_disk_cache_size->value());

  // Changed to GB, then cancelled, which loads the settings again.
  combobox_disk_cache_size->setCurrentIndex(combobox_disk_cache_size->findData(static_cast<int>(CacheSizeUnit::GB)));
  page_->Load();

  EXPECT_EQ(static_cast<int>(CacheSizeUnit::MB), combobox_disk_cache_size->currentData().toInt());
  EXPECT_EQ(360, spinbox_disk_cache_size->value());

}

// Regression test: the maximum was only set when the unit changed, so with KB, which is the first unit, the size was limited to the maximum set in the user interface.
TEST_F(CollectionSettingsPageTest, LoadingLargeSizeInKBKeepsSize) {

  SaveCacheSizes(2000000, CacheSizeUnit::KB, 3000000, CacheSizeUnit::KB);
  page_->Load();

  QSpinBox *spinbox_cache_size = SpinBox(u"spinbox_cache_size"_s);
  QSpinBox *spinbox_disk_cache_size = SpinBox(u"spinbox_disk_cache_size"_s);
  ASSERT_NE(nullptr, spinbox_cache_size);
  ASSERT_NE(nullptr, spinbox_disk_cache_size);
  EXPECT_EQ(2000000, spinbox_cache_size->value());
  EXPECT_EQ(3000000, spinbox_disk_cache_size->value());

}

}  // namespace
