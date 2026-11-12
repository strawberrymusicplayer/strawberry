// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"
#include "settings/collectionsettingsdirectorymodel.h"

TEST(CollectionSettingsDirectoryModelTest, LocalFoldersKeepTheirPaths) {
  CollectionSettingsDirectoryModel model;
  const QString path = QStringLiteral("/local/music");
  model.AddDirectory(path);
  EXPECT_EQ(model.data(model.index(0, 0)).toString(), path);
  EXPECT_EQ(model.data(model.index(0, 0), Qt::UserRole).toString(), path);
  EXPECT_EQ(model.paths(), QStringList{path});
  model.RemoveDirectory(model.index(0, 0));
  EXPECT_TRUE(model.paths().isEmpty());
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(CollectionSettingsDirectoryModelTest, RemovingLabelledFolderUsesStoredPath) {
  CollectionSettingsDirectoryModel model;
  const QString path = QStringLiteral("/strawberry-nas/12345678-1234-1234-1234-123456789abc");
  const QString other = QStringLiteral("/local/music");
  model.AddDirectory(path, QStringLiteral("smb://nas/music"));
  model.AddDirectory(other);
  EXPECT_EQ(model.data(model.index(0, 0)).toString(), QStringLiteral("smb://nas/music"));
  EXPECT_EQ(model.data(model.index(0, 0), Qt::UserRole).toString(), path);
  model.RemoveDirectory(model.index(0, 0));
  EXPECT_EQ(model.paths(), QStringList{other});
  EXPECT_EQ(model.rowCount(), 1);
}
