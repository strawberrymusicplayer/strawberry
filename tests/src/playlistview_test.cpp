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

#include "gtest_include.h"

#include <QColor>
#include <QImage>
#include <QMetaObject>
#include <QString>
#include <QTest>
#include <QTimeLine>
#include <QUrl>

#include "core/settings.h"
#include "core/song.h"
#include "constants/appearancesettings.h"
#include "covermanager/albumcoverimageresult.h"
#include "covermanager/albumcoverloaderresult.h"
#include "playlist/playlist.h"
#include "playlist/playlistfilter.h"
#include "playlist/playlistview.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistViewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // The album cover fills the whole playlist, without opacity or blur, so the color of the cover is the color in the middle of the playlist.
    Settings s;
    s.beginGroup(AppearanceSettings::kSettingsGroup);
    s.setValue(AppearanceSettings::kBackgroundImageType, static_cast<int>(AppearanceSettings::BackgroundImageType::Album));
    s.setValue(AppearanceSettings::kBackgroundImageStretch, true);
    s.setValue(AppearanceSettings::kBackgroundImageKeepAspectRatio, false);
    s.setValue(AppearanceSettings::kBackgroundImageBlurRadius, 0);
    s.setValue(AppearanceSettings::kBackgroundImageOpacityLevel, 100);
    s.endGroup();
  }

  void TearDown() override {
    Settings().remove(QLatin1String(AppearanceSettings::kSettingsGroup));
  }

  // The view needs a playlist when it's shown.
  // The header is hidden, since the rating delegate is only created by Init(), and the header uses it when the mouse enters.
  static void ShowView(Playlist *playlist, PlaylistView *view) {
    view->setModel(playlist->filter());
    view->SetPlaylist(playlist);
    view->setHeaderHidden(true);
    view->ReloadSettings();
    view->resize(300, 300);
    view->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(view));
  }

  // Waits until the previous background image has faded out, without painting, since the fading is not painted while the window is minimized.
  static bool WaitForFade(PlaylistView *view) {
    const QTimeLine *fade_animation = view->findChild<QTimeLine*>();
    return fade_animation && QTest::qWaitFor([fade_animation]() { return fade_animation->state() == QTimeLine::State::NotRunning; }, 5000);
  }

  static Song MakeSong(const QString &album) {
    Song song;
    song.Init(u"Title"_s, u"Artist"_s, album, 123);
    // Songs are compared by the URL, a song without one is the same as no song.
    song.set_url(QUrl::fromLocalFile(u"/music/"_s + album + u".flac"_s));
    return song;
  }

  static void Play(PlaylistView *view, const Song &song, const QColor &color) {
    QImage image(100, 100, QImage::Format_ARGB32);
    image.fill(color);
    AlbumCoverImageResult album_cover;
    album_cover.image = image;
    const AlbumCoverLoaderResult result(true, AlbumCoverLoaderResult::Type::Manual, album_cover);
    QMetaObject::invokeMethod(view, "SongChanged", Qt::DirectConnection, Q_ARG(Song, song));
    QMetaObject::invokeMethod(view, "AlbumCoverLoaded", Qt::DirectConnection, Q_ARG(Song, song), Q_ARG(AlbumCoverLoaderResult, result));
  }

  static QColor BackgroundColor(PlaylistView *view) {
    const QImage image = view->viewport()->grab().toImage();
    return image.pixelColor(image.width() / 2, image.height() / 2);
  }

  static bool IsRed(const QColor &color) { return color.red() > color.green() + 100; }
  static bool IsGreen(const QColor &color) { return color.green() > color.red() + 100; }
};

// Regression test: when playback was stopped while the window was minimized, the cover of the album played before was shown before the cover of the next album, since the scaled cover is only updated when it's painted.
TEST_F(PlaylistViewTest, StoppedWhileMinimizedDoesNotFadeFromPreviousCover) {

  Playlist playlist(nullptr, nullptr, nullptr, nullptr, nullptr, 1);
  PlaylistView view;
  ShowView(&playlist, &view);
  if (::testing::Test::HasFatalFailure()) return;

  Play(&view, MakeSong(u"First album"_s), Qt::red);
  ASSERT_TRUE(WaitForFade(&view));
  ASSERT_TRUE(IsRed(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

  // Nothing is painted while the window is minimized, it's still visible.
  view.setUpdatesEnabled(false);
  QMetaObject::invokeMethod(&view, "Stopped", Qt::DirectConnection);
  ASSERT_TRUE(WaitForFade(&view));
  view.setUpdatesEnabled(true);
  EXPECT_FALSE(IsRed(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

  // The fading starts with only the previous cover, which is none.
  Play(&view, MakeSong(u"Second album"_s), Qt::green);
  EXPECT_FALSE(IsRed(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

  ASSERT_TRUE(WaitForFade(&view));
  EXPECT_TRUE(IsGreen(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

}

// The cover fades from the cover of the album played before.
TEST_F(PlaylistViewTest, NewCoverFadesFromPreviousCover) {

  Playlist playlist(nullptr, nullptr, nullptr, nullptr, nullptr, 1);
  PlaylistView view;
  ShowView(&playlist, &view);
  if (::testing::Test::HasFatalFailure()) return;

  Play(&view, MakeSong(u"First album"_s), Qt::red);
  ASSERT_TRUE(WaitForFade(&view));
  ASSERT_TRUE(IsRed(BackgroundColor(&view)));

  Play(&view, MakeSong(u"Second album"_s), Qt::green);
  EXPECT_TRUE(IsRed(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

  ASSERT_TRUE(WaitForFade(&view));
  EXPECT_TRUE(IsGreen(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

}

// A cover set again before the cover set before was painted keeps fading out the cover which is shown.
TEST_F(PlaylistViewTest, CoverChangedAgainBeforePaintKeepsFadingFromShownCover) {

  Playlist playlist(nullptr, nullptr, nullptr, nullptr, nullptr, 1);
  PlaylistView view;
  ShowView(&playlist, &view);
  if (::testing::Test::HasFatalFailure()) return;

  Play(&view, MakeSong(u"First album"_s), Qt::red);
  ASSERT_TRUE(WaitForFade(&view));
  ASSERT_TRUE(IsRed(BackgroundColor(&view)));

  Play(&view, MakeSong(u"Second album"_s), Qt::green);
  Play(&view, MakeSong(u"Third album"_s), Qt::blue);
  EXPECT_TRUE(IsRed(BackgroundColor(&view))) << BackgroundColor(&view).name().toStdString();

  ASSERT_TRUE(WaitForFade(&view));
  const QColor color = BackgroundColor(&view);
  EXPECT_GT(color.blue(), color.red() + 100) << color.name().toStdString();

}

}  // namespace
