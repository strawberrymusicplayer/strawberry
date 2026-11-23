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

#include <QList>
#include <QPair>
#include <QString>

#include "core/song.h"

using namespace Qt::Literals::StringLiterals;

namespace {

TEST(SongTest, FiletypeByExtension) {

  const QList<QPair<QString, Song::FileType>> extensions = QList<QPair<QString, Song::FileType>>() << qMakePair(u"wav"_s, Song::FileType::WAV)
                                                                                                   << qMakePair(u"wave"_s, Song::FileType::WAV)
                                                                                                   << qMakePair(u"flac"_s, Song::FileType::FLAC)
                                                                                                   << qMakePair(u"wavpack"_s, Song::FileType::WavPack)
                                                                                                   << qMakePair(u"wv"_s, Song::FileType::WavPack)
                                                                                                   << qMakePair(u"opus"_s, Song::FileType::OggOpus)
                                                                                                   << qMakePair(u"speex"_s, Song::FileType::OggSpeex)
                                                                                                   << qMakePair(u"spx"_s, Song::FileType::OggSpeex)
                                                                                                   << qMakePair(u"mp2"_s, Song::FileType::MPEG)
                                                                                                   << qMakePair(u"mp3"_s, Song::FileType::MPEG)
                                                                                                   << qMakePair(u"mp4"_s, Song::FileType::MP4)
                                                                                                   << qMakePair(u"m4a"_s, Song::FileType::MP4)
                                                                                                   << qMakePair(u"aac"_s, Song::FileType::MP4)
                                                                                                   << qMakePair(u"asf"_s, Song::FileType::ASF)
                                                                                                   << qMakePair(u"wma"_s, Song::FileType::ASF)
                                                                                                   << qMakePair(u"aiff"_s, Song::FileType::AIFF)
                                                                                                   << qMakePair(u"aif"_s, Song::FileType::AIFF)
                                                                                                   << qMakePair(u"aifc"_s, Song::FileType::AIFF)
                                                                                                   << qMakePair(u"mpc"_s, Song::FileType::MPC)
                                                                                                   << qMakePair(u"mp+"_s, Song::FileType::MPC)
                                                                                                   << qMakePair(u"mpp"_s, Song::FileType::MPC)
                                                                                                   << qMakePair(u"dsf"_s, Song::FileType::DSF)
                                                                                                   << qMakePair(u"dsd"_s, Song::FileType::DSDIFF)
                                                                                                   << qMakePair(u"dff"_s, Song::FileType::DSDIFF)
                                                                                                   << qMakePair(u"ape"_s, Song::FileType::APE)
                                                                                                   << qMakePair(u"mod"_s, Song::FileType::MOD)
                                                                                                   << qMakePair(u"module"_s, Song::FileType::MOD)
                                                                                                   << qMakePair(u"nst"_s, Song::FileType::MOD)
                                                                                                   << qMakePair(u"wow"_s, Song::FileType::MOD)
                                                                                                   << qMakePair(u"s3m"_s, Song::FileType::S3M)
                                                                                                   << qMakePair(u"xm"_s, Song::FileType::XM)
                                                                                                   << qMakePair(u"it"_s, Song::FileType::IT)
                                                                                                   << qMakePair(u"spc"_s, Song::FileType::SPC)
                                                                                                   << qMakePair(u"vgm"_s, Song::FileType::VGM)
                                                                                                   << qMakePair(u"FLAC"_s, Song::FileType::FLAC)
                                                                                                   << qMakePair(u"Mp3"_s, Song::FileType::MPEG)
                                                                                                   << qMakePair(u"ogg"_s, Song::FileType::Unknown)
                                                                                                   << qMakePair(u"tta"_s, Song::FileType::Unknown)
                                                                                                   << qMakePair(u"dat"_s, Song::FileType::Unknown)
                                                                                                   << qMakePair(QString(), Song::FileType::Unknown);

  for (const QPair<QString, Song::FileType> &extension : extensions) {
    EXPECT_EQ(extension.second, Song::FiletypeByExtension(extension.first)) << extension.first.toStdString();
  }

}

}  // namespace
