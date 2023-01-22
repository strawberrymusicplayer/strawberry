/* This file is part of Strawberry.
   Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include "tagreadertagparser.h"

#include <string>
#include <memory>
#include <algorithm>
#include <sys/stat.h>

#include <tagparser/mediafileinfo.h>
#include <tagparser/diagnostics.h>
#include <tagparser/progressfeedback.h>
#include <tagparser/tag.h>
#include <tagparser/abstracttrack.h>

#include <QtGlobal>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"
#include "utilities/timeconstants.h"

TagReaderTagParser::TagReaderTagParser() = default;

TagReaderTagParser::~TagReaderTagParser() = default;

bool TagReaderTagParser::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  QFileInfo fileinfo(filename);
  if (!fileinfo.exists() || fileinfo.suffix().compare("bak", Qt::CaseInsensitive) == 0) return false;

  try {
    TagParser::MediaFileInfo taginfo;
    TagParser::Diagnostics diag;
    TagParser::AbortableProgressFeedback progress;

    taginfo.setPath(QFile::encodeName(filename).toStdString());
    taginfo.open(true);

    taginfo.parseContainerFormat(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTracks(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    for (const TagParser::DiagMessage &msg : diag) {
      qLog(Debug) << QString::fromStdString(msg.message());
    }

    const auto tracks = taginfo.tracks();
    for (const auto track : tracks) {
      if (track->mediaType() == TagParser::MediaType::Audio) {
        taginfo.close();
        return true;
      }
    }
    taginfo.close();
  }
  catch(...) {}

  return false;

}

bool TagReaderTagParser::ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const {

  qLog(Debug) << "Reading tags from" << filename;

  const QFileInfo fileinfo(filename);

  if (!fileinfo.exists() || fileinfo.suffix().compare("bak", Qt::CaseInsensitive) == 0) return false;

  const QByteArray url(QUrl::fromLocalFile(filename).toEncoded());

  song->set_basefilename(DataCommaSizeFromQString(fileinfo.fileName()));
  song->set_url(url.constData(), url.size());
  song->set_filesize(fileinfo.size());
  song->set_mtime(fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
  song->set_ctime(fileinfo.birthTime().isValid() ? std::max(fileinfo.birthTime().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
#else
  song->set_ctime(fileinfo.created().isValid() ? std::max(fileinfo.created().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
#endif

  if (song->ctime() <= 0) {
    song->set_ctime(song->mtime());
  }

  song->set_lastseen(QDateTime::currentDateTime().toSecsSinceEpoch());

  try {
    TagParser::MediaFileInfo taginfo;
    TagParser::Diagnostics diag;
    TagParser::AbortableProgressFeedback progress;

#ifdef Q_OS_WIN32
    taginfo.setPath(filename.toStdWString().toStdString());
#else
    taginfo.setPath(QFile::encodeName(filename).toStdString());
#endif

    taginfo.open(true);

    taginfo.parseContainerFormat(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTracks(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTags(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    for (const TagParser::DiagMessage &msg : diag) {
      qLog(Debug) << QString::fromStdString(msg.message());
    }

    const auto tracks = taginfo.tracks();
    for (const auto track : tracks) {
      switch (track->format().general) {
        case TagParser::GeneralMediaFormat::Flac:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_FLAC);
          break;
        case TagParser::GeneralMediaFormat::WavPack:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_WAVPACK);
          break;
        case TagParser::GeneralMediaFormat::MonkeysAudio:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_APE);
          break;
        case TagParser::GeneralMediaFormat::WindowsMediaAudio:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_ASF);
          break;
        case TagParser::GeneralMediaFormat::Vorbis:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_OGGVORBIS);
          break;
        case TagParser::GeneralMediaFormat::Opus:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_OGGOPUS);
          break;
        case TagParser::GeneralMediaFormat::Speex:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_OGGSPEEX);
          break;
        case TagParser::GeneralMediaFormat::Mpeg1Audio:
          switch (track->format().sub) {
            case TagParser::SubFormats::Mpeg1Layer3:
              song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_MPEG);
              break;
            case TagParser::SubFormats::None:
            default:
              break;
          }
          break;
        case TagParser::GeneralMediaFormat::Mpc:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_MPC);
          break;
        case TagParser::GeneralMediaFormat::Pcm:
          song->set_filetype(spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_PCM);
          break;
        case TagParser::GeneralMediaFormat::Unknown:
        default:
          break;
      }
      song->set_length_nanosec(track->duration().totalMilliseconds() * kNsecPerMsec);
      song->set_samplerate(track->samplingFrequency());
      song->set_bitdepth(track->bitsPerSample());
      song->set_bitrate(std::max(track->bitrate(), track->maxBitrate()));
    }

    if (song->filetype() == spb::tagreader::SongMetadata_FileType::SongMetadata_FileType_UNKNOWN) {
      taginfo.close();
      return false;
    }

    for (const auto tag : taginfo.tags()) {
      song->set_albumartist(tag->value(TagParser::KnownField::AlbumArtist).toString(TagParser::TagTextEncoding::Utf8));
      song->set_artist(tag->value(TagParser::KnownField::Artist).toString(TagParser::TagTextEncoding::Utf8));
      song->set_album(tag->value(TagParser::KnownField::Album).toString(TagParser::TagTextEncoding::Utf8));
      song->set_title(tag->value(TagParser::KnownField::Title).toString(TagParser::TagTextEncoding::Utf8));
      song->set_genre(tag->value(TagParser::KnownField::Genre).toString(TagParser::TagTextEncoding::Utf8));
      song->set_composer(tag->value(TagParser::KnownField::Composer).toString(TagParser::TagTextEncoding::Utf8));
      song->set_performer(tag->value(TagParser::KnownField::Performers).toString(TagParser::TagTextEncoding::Utf8));
      song->set_grouping(tag->value(TagParser::KnownField::Grouping).toString(TagParser::TagTextEncoding::Utf8));
      song->set_comment(tag->value(TagParser::KnownField::Comment).toString(TagParser::TagTextEncoding::Utf8));
      song->set_lyrics(tag->value(TagParser::KnownField::Lyrics).toString(TagParser::TagTextEncoding::Utf8));
      song->set_year(tag->value(TagParser::KnownField::RecordDate).toInteger());
      song->set_originalyear(tag->value(TagParser::KnownField::ReleaseDate).toInteger());
      song->set_track(tag->value(TagParser::KnownField::TrackPosition).toInteger());
      song->set_disc(tag->value(TagParser::KnownField::DiskPosition).toInteger());
      if (!tag->value(TagParser::KnownField::Cover).empty() && tag->value(TagParser::KnownField::Cover).dataSize() > 0) {
        song->set_art_automatic(kEmbeddedCover);
      }
      const float rating = ConvertPOPMRating(tag->value(TagParser::KnownField::Rating));
      if (song->rating() <= 0 && rating > 0.0 && rating <= 1.0) {
        song->set_rating(rating);
      }
    }

    // Set integer fields to -1 if they're not valid
    if (song->track() <= 0) { song->set_track(-1); }
    if (song->disc() <= 0) { song->set_disc(-1); }
    if (song->year() <= 0) { song->set_year(-1); }
    if (song->originalyear() <= 0) { song->set_originalyear(-1); }
    if (song->samplerate() <= 0) { song->set_samplerate(-1); }
    if (song->bitdepth() <= 0) { song->set_bitdepth(-1); }
    if (song->bitrate() <= 0) { song->set_bitrate(-1); }
    if (song->lastplayed() <= 0) { song->set_lastplayed(-1); }

    song->set_valid(true);

    taginfo.close();

    return true;

  }
  catch(...) {
    return false;
  }

}

bool TagReaderTagParser::SaveFile(const QString &filename, const spb::tagreader::SongMetadata &song) const {

  if (filename.isEmpty()) return false;

  qLog(Debug) << "Saving tags to" << filename;

  try {
    TagParser::MediaFileInfo taginfo;
    TagParser::Diagnostics diag;
    TagParser::AbortableProgressFeedback progress;
#ifdef Q_OS_WIN32
    taginfo.setPath(filename.toStdWString().toStdString());
#else
    taginfo.setPath(QFile::encodeName(filename).toStdString());
#endif
    taginfo.open(false);

    taginfo.parseContainerFormat(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTracks(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTags(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    if (taginfo.tags().size() <= 0) {
      taginfo.createAppropriateTags();
    }

    for (const auto tag : taginfo.tags()) {
      tag->setValue(TagParser::KnownField::AlbumArtist, TagParser::TagValue(song.albumartist(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Artist, TagParser::TagValue(song.artist(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Album, TagParser::TagValue(song.album(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Title, TagParser::TagValue(song.title(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Genre, TagParser::TagValue(song.genre(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Composer, TagParser::TagValue(song.composer(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Performers, TagParser::TagValue(song.performer(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Grouping, TagParser::TagValue(song.grouping(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Comment, TagParser::TagValue(song.comment(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::Lyrics, TagParser::TagValue(song.lyrics(), TagParser::TagTextEncoding::Utf8, tag->proposedTextEncoding()));
      tag->setValue(TagParser::KnownField::TrackPosition, TagParser::TagValue(song.track()));
      tag->setValue(TagParser::KnownField::DiskPosition, TagParser::TagValue(song.disc()));
      tag->setValue(TagParser::KnownField::RecordDate, TagParser::TagValue(song.year()));
      tag->setValue(TagParser::KnownField::ReleaseDate, TagParser::TagValue(song.originalyear()));
    }
    taginfo.applyChanges(diag, progress);
    taginfo.close();

    for (const TagParser::DiagMessage &msg : diag) {
      qLog(Debug) << QString::fromStdString(msg.message());
    }

    return true;
  }
  catch(...) {}

  return false;

}

QByteArray TagReaderTagParser::LoadEmbeddedArt(const QString &filename) const {

  if (filename.isEmpty()) return QByteArray();

  qLog(Debug) << "Loading art from" << filename;

  try {

    TagParser::MediaFileInfo taginfo;
    TagParser::Diagnostics diag;
    TagParser::AbortableProgressFeedback progress;

#ifdef Q_OS_WIN32
    taginfo.setPath(filename.toStdWString().toStdString());
#else
    taginfo.setPath(QFile::encodeName(filename).toStdString());
#endif

    taginfo.open();

    taginfo.parseContainerFormat(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return QByteArray();
    }

    taginfo.parseTags(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return QByteArray();
    }

    for (const auto tag : taginfo.tags()) {
      if (!tag->value(TagParser::KnownField::Cover).empty() && tag->value(TagParser::KnownField::Cover).dataSize() > 0) {
        QByteArray data(tag->value(TagParser::KnownField::Cover).dataPointer(), tag->value(TagParser::KnownField::Cover).dataSize());
        taginfo.close();
        return data;
      }
    }

    taginfo.close();

    for (const TagParser::DiagMessage &msg : diag) {
      qLog(Debug) << QString::fromStdString(msg.message());
    }

  }
  catch(...) {}

  return QByteArray();

}

bool TagReaderTagParser::SaveEmbeddedArt(const QString &filename, const QByteArray &data) {

  if (filename.isEmpty()) return false;

  qLog(Debug) << "Saving art to" << filename;

  try {

    TagParser::MediaFileInfo taginfo;
    TagParser::Diagnostics diag;
    TagParser::AbortableProgressFeedback progress;

#ifdef Q_OS_WIN32
    taginfo.setPath(filename.toStdWString().toStdString());
#else
    taginfo.setPath(QFile::encodeName(filename).toStdString());
#endif

    taginfo.open();

    taginfo.parseContainerFormat(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    taginfo.parseTags(diag, progress);
    if (progress.isAborted()) {
      taginfo.close();
      return false;
    }

    if (taginfo.tags().size() <= 0) {
      taginfo.createAppropriateTags();
    }

    for (const auto tag : taginfo.tags()) {
      tag->setValue(TagParser::KnownField::Cover, TagParser::TagValue(data.toStdString()));
    }

    taginfo.applyChanges(diag, progress);
    taginfo.close();

    for (const TagParser::DiagMessage &msg : diag) {
      qLog(Debug) << QString::fromStdString(msg.message());
    }

    return true;

  }
  catch(...) {}

  return false;

}

bool TagReaderTagParser::SaveSongPlaycountToFile(const QString&, const spb::tagreader::SongMetadata&) const { return false; }

bool TagReaderTagParser::SaveSongRatingToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const {

    if (filename.isEmpty()) return false;

    qLog(Debug) << "Saving song rating to" << filename;

    try {
      TagParser::MediaFileInfo taginfo;
      TagParser::Diagnostics diag;
      TagParser::AbortableProgressFeedback progress;
  #ifdef Q_OS_WIN32
      taginfo.setPath(filename.toStdWString().toStdString());
  #else
      taginfo.setPath(QFile::encodeName(filename).toStdString());
  #endif
      taginfo.open(false);

      taginfo.parseContainerFormat(diag, progress);
      if (progress.isAborted()) {
        taginfo.close();
        return false;
      }

      taginfo.parseTracks(diag, progress);
      if (progress.isAborted()) {
        taginfo.close();
        return false;
      }

      taginfo.parseTags(diag, progress);
      if (progress.isAborted()) {
        taginfo.close();
        return false;
      }

      if (taginfo.tags().size() <= 0) {
        taginfo.createAppropriateTags();
      }

      for (const auto tag : taginfo.tags()) {
        tag->setValue(TagParser::KnownField::Rating, TagParser::TagValue(ConvertToPOPMRating(song.rating())));
      }
      taginfo.applyChanges(diag, progress);
      taginfo.close();

      for (const TagParser::DiagMessage &msg : diag) {
        qLog(Debug) << QString::fromStdString(msg.message());
      }

      return true;
    }
    catch(...) {}

    return false;

}
