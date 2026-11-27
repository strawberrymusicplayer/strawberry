/*
 * Strawberry Music Player
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

#include "gtest_include.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>

#include "widgets/playlisttitleheader.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistTitleHeaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    header_.SetPlaylist(3, u"Road trip"_s);
  }

  void Rename(const QString &new_name) {
    header_.name_button()->click();
    header_.name_editor()->setText(new_name);
    Q_EMIT header_.name_editor()->returnPressed();
  }

  PlaylistTitleHeader header_;
};

}  // namespace

TEST_F(PlaylistTitleHeaderTest, ShowsThePlaylistName) {

  EXPECT_EQ(3, header_.playlist_id());
  EXPECT_EQ(u"Road trip"_s, header_.name_button()->text());

  header_.SetPlaylist(-1, u"Ignored"_s);
  EXPECT_TRUE(header_.name_button()->text().isEmpty());

}

TEST_F(PlaylistTitleHeaderTest, TappingTheNameStartsRenaming) {

  header_.name_button()->click();
  EXPECT_TRUE(header_.is_renaming());
  EXPECT_EQ(u"Road trip"_s, header_.name_editor()->text());
  EXPECT_EQ(u"Road trip"_s, header_.name_editor()->selectedText());

}

TEST_F(PlaylistTitleHeaderTest, NoPlaylistCannotBeRenamed) {

  header_.SetPlaylist(-1, QString());
  header_.name_button()->click();
  EXPECT_FALSE(header_.is_renaming());

}

TEST_F(PlaylistTitleHeaderTest, ConfirmingRequestsTheTrimmedName) {

  QSignalSpy spy(&header_, &PlaylistTitleHeader::RenameRequested);
  Rename(u"  Holiday  "_s);
  EXPECT_FALSE(header_.is_renaming());
  ASSERT_EQ(1, spy.count());
  EXPECT_EQ(3, spy.at(0).at(0).toInt());
  EXPECT_EQ(u"Holiday"_s, spy.at(0).at(1).toString());

}

TEST_F(PlaylistTitleHeaderTest, EmptyOrUnchangedNamesAreIgnored) {

  QSignalSpy spy(&header_, &PlaylistTitleHeader::RenameRequested);
  Rename(u"   "_s);
  Rename(u"Road trip"_s);
  EXPECT_FALSE(header_.is_renaming());
  EXPECT_EQ(0, spy.count());

}

TEST_F(PlaylistTitleHeaderTest, LeavingTheEditorCancels) {

  QSignalSpy spy(&header_, &PlaylistTitleHeader::RenameRequested);
  header_.name_button()->click();
  header_.name_editor()->setText(u"Holiday"_s);
  Q_EMIT header_.name_editor()->editingFinished();
  EXPECT_FALSE(header_.is_renaming());
  EXPECT_EQ(0, spy.count());

  // A late Return after cancelling does not rename either.
  Q_EMIT header_.name_editor()->returnPressed();
  EXPECT_EQ(0, spy.count());

}

TEST_F(PlaylistTitleHeaderTest, SwitchingPlaylistsCancelsRenaming) {

  QSignalSpy spy(&header_, &PlaylistTitleHeader::RenameRequested);
  header_.name_button()->click();
  header_.name_editor()->setText(u"Holiday"_s);

  // Renaming the edited playlist elsewhere keeps the editor open.
  header_.SetPlaylist(3, u"Road trip 2"_s);
  EXPECT_TRUE(header_.is_renaming());

  header_.SetPlaylist(4, u"Work"_s);
  EXPECT_FALSE(header_.is_renaming());
  EXPECT_EQ(u"Work"_s, header_.name_button()->text());
  Q_EMIT header_.name_editor()->returnPressed();
  EXPECT_EQ(0, spy.count());

}

TEST_F(PlaylistTitleHeaderTest, BackButtonEmitsBackClicked) {

  QSignalSpy spy(&header_, &PlaylistTitleHeader::BackClicked);
  header_.back_button()->click();
  EXPECT_EQ(1, spy.count());

}
