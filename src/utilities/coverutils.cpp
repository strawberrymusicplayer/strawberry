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
#include <QRegularExpression>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>

#include "filenameconstants.h"
#include "transliterate.h"
#include "coverutils.h"
#include "core/logging.h"

QByteArray CoverUtils::Sha1CoverHash(const QString &artist, const QString &album) {

  QCryptographicHash hash(QCryptographicHash::Sha1);
  hash.addData(artist.toLower().toUtf8());
  hash.addData(album.toLower().toUtf8());

  return hash.result();

}

QString CoverUtils::AlbumCoverFilename(QString artist, QString album, const QString &extension) {

  artist.remove('/').remove('\\');
  album.remove('/').remove('\\');

  QString filename = artist + "-" + album;
  filename = Utilities::Transliterate(filename.toLower());
  filename = filename.replace(' ', '-')
               .replace("--", "-")
               .remove(QRegularExpression(QString(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption))
               .simplified();

  if (!extension.isEmpty()) {
    filename.append('.');
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

  if (path.right(1) == QDir::separator() || path.right(1) == "/") {
    path.chop(1);
  }

  QDir dir;
  if (!dir.mkpath(path)) {
    qLog(Error) << "Unable to create directory" << path;
    path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  }

  QString filename;
  if (source == Song::Source::Collection &&
      options.cover_type == CoverOptions::CoverType::Album &&
      options.cover_filename == CoverOptions::CoverFilename::Pattern &&
      !options.cover_pattern.isEmpty()) {
    filename = CoverFilenameFromVariable(options, artist, album);
    filename.remove(QRegularExpression(QString(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption)).remove('/').remove('\\');
    if (options.cover_lowercase) filename = filename.toLower();
    if (options.cover_replace_spaces) filename.replace(QRegularExpression("\\s"), "-");
    if (!extension.isEmpty()) {
      filename.append('.');
      filename.append(extension);
    }
  }

  if (filename.isEmpty()) {
    filename = CoverFilenameFromSource(source, cover_url, artist, album, album_id, extension);
  }

  QString filepath(path + "/" + filename);

  return filepath;

}

QString CoverUtils::CoverFilenameFromSource(const Song::Source source, const QUrl &cover_url, const QString &artist, const QString &album, const QString &album_id, const QString &extension) {

  QString filename;

  switch (source) {
    case Song::Source::Tidal:
      if (!album_id.isEmpty()) {
        filename = album_id + "-" + cover_url.fileName();
        break;
      }
      [[fallthrough]];
    case Song::Source::Subsonic:
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
      filename = Sha1CoverHash(artist, album).toHex();
      break;
  }

  if (!extension.isEmpty()) {
    filename.append('.');
    filename.append(extension);
  }

  return filename;

}

QString CoverUtils::CoverFilenameFromVariable(const CoverOptions &options, const QString &artist, QString album, const QString &extension) {

  album = album.remove(Song::kAlbumRemoveDisc);

  QString filename(options.cover_pattern);
  filename.replace("%albumartist", artist);
  filename.replace("%artist", artist);
  filename.replace("%album", album);
  if (!extension.isEmpty()) {
    filename.append('.');
    filename.append(extension);
  }
  return filename;

}
