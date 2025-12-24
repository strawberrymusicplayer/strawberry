/*
 * Strawberry Music Player
 * Copyright 2019-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QCryptographicHash>

#include "constants/filenameconstants.h"
#include "core/logging.h"
#include "core/standardpaths.h"
#include "transliterate.h"
#include "coverutils.h"

using namespace Qt::Literals::StringLiterals;

QByteArray CoverUtils::Sha1CoverHash(const QString &artist, const QString &album) {

  QCryptographicHash hash(QCryptographicHash::Sha1);
  hash.addData(artist.toLower().toUtf8());
  hash.addData(album.toLower().toUtf8());

  return hash.result();

}

QString CoverUtils::AlbumCoverFilename(QString artist, QString album, const QString &extension) {

  artist.remove(u'/').remove(u'\\');
  album.remove(u'/').remove(u'\\');

  QString filename = artist + QLatin1Char('-') + album;
  filename = Utilities::Transliterate(filename.toLower());
  filename = filename.replace(u' ', u'-')
               .replace("--"_L1, "-"_L1)
               .remove(QRegularExpression(QLatin1String(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption))
               .simplified();

  if (!extension.isEmpty()) {
    filename.append(u'.');
    filename.append(extension);
  }

  return filename;

}

QString CoverUtils::CoverFilePath(const CoverOptions &options, const Song &song, const QString &album_dir, const QUrl &cover_url, const QString &extension) {
  return CoverFilePath(options, song.source(), song.effective_albumartist(), song.album(), song.album_id(), album_dir, cover_url, extension);
}

QString CoverUtils::CoverFilePath(const CoverOptions &options, const Song::Source source, const QString &artist, const QString &album, const QString &album_id, const QString &album_dir, const QUrl &cover_url, const QString &extension) {

  QString path;
  if (source == Song::Source::Collection && options.cover_type == CoverOptions::CoverType::Album && !album_dir.isEmpty()) {
    path = album_dir;
  }
  else {
    path = Song::ImageCacheDir(source);
  }

  if (path.right(1) == QDir::separator() || path.right(1) == u'/') {
    path.chop(1);
  }

  QDir dir;
  if (!QFileInfo::exists(path) && !dir.mkpath(path)) {
    qLog(Error) << "Unable to create directory" << path;
    path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::TempLocation);
  }

  QString filename;
  if (source == Song::Source::Collection &&
      options.cover_type == CoverOptions::CoverType::Album &&
      options.cover_filename == CoverOptions::CoverFilename::Pattern &&
      !options.cover_pattern.isEmpty()) {
    filename = CoverFilenameFromVariable(options, artist, album);
    filename.remove(QRegularExpression(QLatin1String(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption)).remove(u'/').remove(u'\\');
    if (options.cover_lowercase) filename = filename.toLower();
    if (options.cover_replace_spaces) {
      static const QRegularExpression regex_whitespaces(u"\\s"_s);
      filename.replace(regex_whitespaces, u"-"_s);
    }
    if (!extension.isEmpty()) {
      filename.append(u'.');
      filename.append(extension);
    }
  }

  if (filename.isEmpty()) {
    filename = CoverFilenameFromSource(source, cover_url, artist, album, album_id, extension);
  }

  QString filepath(path + QLatin1Char('/') + filename);

  return filepath;

}

QString CoverUtils::CoverFilenameFromSource(const Song::Source source, const QUrl &cover_url, const QString &artist, const QString &album, const QString &album_id, const QString &extension) {

  QString filename;

  switch (source) {
    case Song::Source::Tidal:
      if (!album_id.isEmpty()) {
        filename = album_id + QLatin1Char('-') + cover_url.fileName();
        break;
      }
      [[fallthrough]];
    case Song::Source::Subsonic:
    case Song::Source::Spotify:
    case Song::Source::Qobuz:
      if (!album_id.isEmpty()) {
        filename = album_id;
        break;
      }
      [[fallthrough]];
    case Song::Source::Collection:
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Stream:
    case Song::Source::SomaFM:
    case Song::Source::RadioParadise:
    case Song::Source::Unknown:
      filename = QString::fromLatin1(Sha1CoverHash(artist, album).toHex());
      break;
  }

  if (!extension.isEmpty()) {
    filename.append(u'.');
    filename.append(extension);
  }

  return filename;

}

QString CoverUtils::CoverFilenameFromVariable(const CoverOptions &options, const QString &artist, QString album, const QString &extension) {

  album = Song::AlbumRemoveDisc(album);

  QString filename(options.cover_pattern);
  filename.replace("%albumartist"_L1, artist);
  filename.replace("%artist"_L1, artist);
  filename.replace("%album"_L1, album);
  if (!extension.isEmpty()) {
    filename.append(u'.');
    filename.append(extension);
  }
  return filename;

}

QString CoverUtils::PickBestImageFromList(const QStringList &image_list, const QStringList &filter_patterns) {

  // This is used when there is more than one image in a directory.
  // Pick the biggest image that matches the most important filter pattern

  QStringList filtered;

  for (const QString &filter_text : filter_patterns) {
    // The images in the image_list are represented by a full path, so we need to isolate just the filename
    for (const QString &image_path : image_list) {
      QFileInfo fileinfo(image_path);
      QString filename(fileinfo.fileName());
      if (filename.contains(filter_text, Qt::CaseInsensitive)) {
        filtered << image_path;
      }
    }

    // We assume the filters are given in the order best to worst, so if we've got a result, we go with it.
    // Otherwise we might start capturing more generic rules
    if (!filtered.isEmpty()) break;
  }

  if (filtered.isEmpty()) {
    // The filter was too restrictive, just use the original list
    filtered = image_list;
  }

  int biggest_size = 0;
  QString biggest_path;

  for (const QString &path : std::as_const(filtered)) {
    QImageReader reader(path);
    if (!reader.canRead()) continue;

    QSize size = reader.size();
    if (!size.isValid()) continue;

    int pixel_count = size.width() * size.height();
    if (pixel_count > biggest_size) {
      biggest_size = pixel_count;
      biggest_path = path;
    }
  }

  return biggest_path;

}
