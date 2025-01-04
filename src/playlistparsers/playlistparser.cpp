/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <algorithm>

#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "core/logging.h"
#include "playlistparser.h"
#include "parserbase.h"
#include "asxiniparser.h"
#include "asxparser.h"
#include "cueparser.h"
#include "m3uparser.h"
#include "plsparser.h"
#include "wplparser.h"
#include "xspfparser.h"

using namespace Qt::Literals::StringLiterals;

const int PlaylistParser::kMagicSize = 512;

PlaylistParser::PlaylistParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent) : QObject(parent), default_parser_(nullptr) {

  AddParser(new XSPFParser(tagreader_client, collection_backend, this));
  AddParser(new M3UParser(tagreader_client, collection_backend, this));
  AddParser(new PLSParser(tagreader_client, collection_backend, this));
  AddParser(new ASXParser(tagreader_client, collection_backend, this));
  AddParser(new AsxIniParser(tagreader_client, collection_backend, this));
  AddParser(new CueParser(tagreader_client, collection_backend, this));
  AddParser(new WplParser(tagreader_client, collection_backend, this));

}

void PlaylistParser::AddParser(ParserBase *parser) {

  if (!default_parser_) {
    default_parser_ = parser;
  }

  parsers_ << parser;
  QObject::connect(parser, &ParserBase::Error, this, &PlaylistParser::Error);

}

QStringList PlaylistParser::file_extensions(const Type type) const {

  QStringList ret;

  for (ParserBase *parser : parsers_) {
    if (ParserIsSupported(type, parser)) {
      ret << parser->file_extensions();
    }
  }

  std::stable_sort(ret.begin(), ret.end());
  return ret;

}

QStringList PlaylistParser::mime_types(const Type type) const {

  QStringList ret;

  for (ParserBase *parser : parsers_) {
    if (ParserIsSupported(type, parser) && !parser->mime_type().isEmpty()) {
      ret << parser->mime_type();
    }
  }

  std::stable_sort(ret.begin(), ret.end());

  return ret;

}

QString PlaylistParser::filters(const Type type) const {

  QStringList filters;
  filters.reserve(parsers_.count() + 1);
  QStringList all_extensions;
  for (ParserBase *parser : parsers_) {
    if (ParserIsSupported(type, parser)) {
      filters << FilterForParser(parser, &all_extensions);
    }
  }

  if (type == Type::Load) {
    filters.prepend(tr("All playlists (%1)").arg(all_extensions.join(u' ')));
  }

  return filters.join(";;"_L1);

}

QString PlaylistParser::FilterForParser(const ParserBase *parser, QStringList *all_extensions) {

  const QStringList file_extensions = parser->file_extensions();
  QStringList extensions;
  extensions.reserve(file_extensions.count());
  for (const QString &extension : file_extensions) {
    extensions << u"*."_s + extension;
  }

  if (all_extensions) *all_extensions << extensions;

  return tr("%1 playlists (%2)").arg(parser->name(), extensions.join(u' '));

}

QString PlaylistParser::default_extension() const {
  QStringList file_extensions = default_parser_->file_extensions();
  return file_extensions[0];
}

QString PlaylistParser::default_filter() const {
  return FilterForParser(default_parser_);
}

ParserBase *PlaylistParser::ParserForExtension(const Type type, const QString &suffix) const {

  for (ParserBase *parser : parsers_) {
    if (ParserIsSupported(type, parser) && parser->file_extensions().contains(suffix, Qt::CaseInsensitive)) {
      return parser;
    }
  }
  return nullptr;

}

ParserBase *PlaylistParser::ParserForMimeType(const Type type, const QString &mime_type) const {

  for (ParserBase *parser : parsers_) {
    if (ParserIsSupported(type, parser) && !parser->mime_type().isEmpty() && QString::compare(parser->mime_type(), mime_type, Qt::CaseInsensitive) == 0) {
      return parser;
    }
  }
  return nullptr;

}

ParserBase *PlaylistParser::ParserForMagic(const QByteArray &data, const QString &mime_type) const {

  for (ParserBase *parser : parsers_) {
    if ((!mime_type.isEmpty() && mime_type == parser->mime_type()) || parser->TryMagic(data)) {
      return parser;
    }
  }
  return nullptr;

}

SongList PlaylistParser::LoadFromFile(const QString &filename) const {

  QFileInfo fileinfo(filename);

  // Find a parser that supports this file extension
  ParserBase *parser = ParserForExtension(Type::Load, fileinfo.suffix());
  if (!parser) {
    qLog(Error) << "Unknown filetype:" << filename;
    Q_EMIT Error(tr("Unknown filetype: %1").arg(filename));
    return SongList();
  }

  // Open the file
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    Q_EMIT Error(tr("Could not open file %1").arg(filename));
    return SongList();
  }

  const SongList songs = parser->Load(&file, filename, fileinfo.absolutePath(), true).songs;
  file.close();

  return songs;

}

SongList PlaylistParser::LoadFromDevice(QIODevice *device, const QString &path_hint, const QDir &dir_hint) const {

  // Find a parser that supports this data
  ParserBase *parser = ParserForMagic(device->peek(kMagicSize));
  if (!parser) {
    return SongList();
  }

  return parser->Load(device, path_hint, dir_hint).songs;

}

void PlaylistParser::Save(const QString &playlist_name, const SongList &songs, const QString &filename, const PlaylistSettings::PathType path_type) const {

  QFileInfo fileinfo(filename);
  QDir dir(fileinfo.path());

  if (!dir.exists()) {
    qLog(Error) << "Directory" << dir.path() << "does not exist";
    Q_EMIT Error(tr("Directory %1 does not exist.").arg(dir.path()));
    return;
  }

  // Find a parser that supports this file extension
  ParserBase *parser = ParserForExtension(Type::Save, fileinfo.suffix());
  if (!parser) {
    qLog(Error) << "Unknown filetype" << filename;
    Q_EMIT Error(tr("Unknown filetype: %1").arg(filename));
    return;
  }

  if (path_type == PlaylistSettings::PathType::Absolute && dir.path() != dir.absolutePath()) {
    dir.setPath(dir.absolutePath());
  }
  else if (path_type != PlaylistSettings::PathType::Absolute && !dir.canonicalPath().isEmpty() && dir.path() != dir.canonicalPath()) {
    dir.setPath(dir.canonicalPath());
  }

  // Open the file
  QFile file(fileinfo.absoluteFilePath());
  if (!file.open(QIODevice::WriteOnly)) {
    qLog(Error) << "Failed to open" << filename << "for writing.";
    Q_EMIT Error(tr("Failed to open %1 for writing.").arg(filename));
    return;
  }

  parser->Save(playlist_name, songs, &file, dir, path_type);

  file.close();

}

bool PlaylistParser::ParserIsSupported(const Type type, ParserBase *parser) const {

  return ((type == Type::Load && parser->load_supported()) || (type == Type::Save && parser->save_supported()));

}
