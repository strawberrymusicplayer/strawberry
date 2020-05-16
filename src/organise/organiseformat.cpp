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

#include "config.h"

#include <QObject>
#include <QApplication>
#include <QList>
#include <QChar>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QRegExp>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QPalette>
#include <QValidator>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFormat>

#include "core/arraysize.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "core/song.h"

#include "organiseformat.h"

const char *OrganiseFormat::kTagPattern = "\\%([a-zA-Z]*)";
const char *OrganiseFormat::kBlockPattern = "\\{([^{}]+)\\}";
const QStringList OrganiseFormat::kKnownTags = QStringList() << "title"
                                                             << "album"
                                                             << "artist"
                                                             << "artistinitial"
                                                             << "albumartist"
                                                             << "composer"
                                                             << "track"
                                                             << "disc"
                                                             << "year"
                                                             << "originalyear"
                                                             << "genre"
                                                             << "comment"
                                                             << "length"
                                                             << "bitrate"
                                                             << "samplerate"
                                                             << "bitdepth"
                                                             << "extension"
                                                             << "performer"
                                                             << "grouping"
                                                             << "lyrics";

const QRegExp OrganiseFormat::kInvalidDirCharacters("[/\\\\]");
const QRegExp OrganiseFormat::kProblematicCharacters("[:?*\"<>|]");
// From http://en.wikipedia.org/wiki/8.3_filename#Directory_table
const QRegExp OrganiseFormat::kInvalidFatCharacters("[^a-zA-Z0-9!#\\$%&'()\\-@\\^_`{}~/. ]");

const char OrganiseFormat::kInvalidPrefixCharacters[] = ".";
const int OrganiseFormat::kInvalidPrefixCharactersCount = arraysize(OrganiseFormat::kInvalidPrefixCharacters) - 1;

const QRgb OrganiseFormat::SyntaxHighlighter::kValidTagColorLight = qRgb(64, 64, 255);
const QRgb OrganiseFormat::SyntaxHighlighter::kInvalidTagColorLight = qRgb(255, 64, 64);
const QRgb OrganiseFormat::SyntaxHighlighter::kBlockColorLight = qRgb(230, 230, 230);

const QRgb OrganiseFormat::SyntaxHighlighter::kValidTagColorDark = qRgb(128, 128, 255);
const QRgb OrganiseFormat::SyntaxHighlighter::kInvalidTagColorDark = qRgb(255, 128, 128);
const QRgb OrganiseFormat::SyntaxHighlighter::kBlockColorDark = qRgb(64, 64, 64);

OrganiseFormat::OrganiseFormat(const QString &format)
    : format_(format),
      remove_problematic_(false),
      remove_non_fat_(false),
      remove_non_ascii_(false),
      allow_ascii_ext_(false),
      replace_spaces_(true) {}

void OrganiseFormat::set_format(const QString &v) {
  format_ = v;
  format_.replace('\\', '/');
}

bool OrganiseFormat::IsValid() const {

  int pos = 0;
  QString format_copy(format_);

  Validator v;
  return v.validate(format_copy, pos) == QValidator::Acceptable;

}

QString OrganiseFormat::GetFilenameForSong(const Song &song) const {

  QString filename = ParseBlock(format_, song);

  if (QFileInfo(filename).completeBaseName().isEmpty()) {
    // Avoid having empty filenames, or filenames with extension only: in this case, keep the original filename.
    // We remove the extension from "filename" if it exists, as song.basefilename() also contains the extension.
    filename = Utilities::PathWithoutFilenameExtension(filename) + song.basefilename();
  }

  if (remove_problematic_) filename = filename.remove(kProblematicCharacters);
  if (remove_non_fat_ || (remove_non_ascii_ && !allow_ascii_ext_)) filename = Utilities::UnicodeToAscii(filename);
  if (remove_non_fat_) filename = filename.remove(kInvalidFatCharacters);

  if (remove_non_ascii_) {
    int ascii = 128;
    if (allow_ascii_ext_) ascii = 255;
    QString stripped;
    for (int i = 0 ; i < filename.length() ; ++i) {
      const QCharRef c = filename[i];
      if (c < ascii) {
        stripped.append(c);
      }
      else {
        const QString decomposition = c.decomposition();
        if (!decomposition.isEmpty() && decomposition[0] < ascii)
          stripped.append(decomposition[0]);
      }
    }
    filename = stripped;
  }

  // Remove repeated whitespaces in the filename.
  filename = filename.simplified();

  QFileInfo info(filename);
  QString extension = info.suffix();
  QString filepath;
  if (!info.path().isEmpty() && info.path() != ".") {
    filepath.append(info.path());
    filepath.append("/");
  }
  filepath.append(info.completeBaseName());

  // Fix any parts of the path that start with dots.
  QStringList parts_old = filepath.split("/");
  QStringList parts_new;
  for (int i = 0 ; i < parts_old.count() ; ++i) {
    QString part = parts_old[i];
    for (int j = 0 ; j < kInvalidPrefixCharactersCount ; ++j) {
      if (part.startsWith(kInvalidPrefixCharacters[j])) {
        part = part.remove(0, 1);
        break;
      }
    }
    part = part.trimmed();
    parts_new.append(part);
  }
  filename = parts_new.join("/");

  if (replace_spaces_) filename.replace(QRegExp("\\s"), "_");

  if (!extension.isEmpty()) {
    filename.append(QString(".%1").arg(extension));
  }

  return filename;

}

QString OrganiseFormat::ParseBlock(QString block, const Song &song, bool *any_empty) const {

  QRegExp tag_regexp(kTagPattern);
  QRegExp block_regexp(kBlockPattern);

  // Find any blocks first
  int pos = 0;
  while ((pos = block_regexp.indexIn(block, pos)) != -1) {
    // Recursively parse the block
    bool empty = false;
    QString value = ParseBlock(block_regexp.cap(1), song, &empty);
    if (empty) value = "";

    // Replace the block's value
    block.replace(pos, block_regexp.matchedLength(), value);
    pos += value.length();
  }

  // Now look for tags
  bool empty = false;
  pos = 0;
  while ((pos = tag_regexp.indexIn(block, pos)) != -1) {
    QString value = TagValue(tag_regexp.cap(1), song);
    if (value.isEmpty()) empty = true;

    block.replace(pos, tag_regexp.matchedLength(), value);
    pos += value.length();
  }

  if (any_empty) *any_empty = empty;
  return block;

}

QString OrganiseFormat::TagValue(const QString &tag, const Song &song) const {

  QString value;

  if (tag == "title")
    value = song.title();
  else if (tag == "album")
    value = song.album();
  else if (tag == "artist")
    value = song.artist();
  else if (tag == "composer")
    value = song.composer();
  else if (tag == "performer")
    value = song.performer();
  else if (tag == "grouping")
    value = song.grouping();
  else if (tag == "lyrics")
    value = song.lyrics();
  else if (tag == "genre")
    value = song.genre();
  else if (tag == "comment")
    value = song.comment();
  else if (tag == "year")
    value = QString::number(song.year());
  else if (tag == "originalyear")
    value = QString::number(song.effective_originalyear());
  else if (tag == "track")
    value = QString::number(song.track());
  else if (tag == "disc")
    value = QString::number(song.disc());
  else if (tag == "length")
    value = QString::number(song.length_nanosec() / kNsecPerSec);
  else if (tag == "bitrate")
    value = QString::number(song.bitrate());
  else if (tag == "samplerate")
    value = QString::number(song.samplerate());
  else if (tag == "bitdepth")
    value = QString::number(song.bitdepth());
  else if (tag == "extension")
    value = QFileInfo(song.url().toLocalFile()).suffix();
  else if (tag == "artistinitial") {
    value = song.effective_albumartist().trimmed();
    if (!value.isEmpty()) {
      value.replace(QRegExp("^the\\s+", Qt::CaseInsensitive), "");
      value = value[0].toUpper();
    }
  }
  else if (tag == "albumartist") {
    value = song.is_compilation() ? "Various Artists" : song.effective_albumartist();
  }

  if (value == "0" || value == "-1") value = "";

  // Prepend a 0 to single-digit track numbers
  if (tag == "track" && value.length() == 1) value.prepend('0');

  // Replace characters that really shouldn't be in paths
  value = value.remove(kInvalidDirCharacters);
  if (remove_problematic_) value = value.remove('.');
  value = value.trimmed();

  return value;

}

OrganiseFormat::Validator::Validator(QObject *parent) : QValidator(parent) {}

QValidator::State OrganiseFormat::Validator::validate(QString &input, int&) const {

  QRegExp tag_regexp(kTagPattern);

  // Make sure all the blocks match up
  int block_level = 0;
  for (int i = 0; i < input.length(); ++i) {
    if (input[i] == '{')
      block_level++;
    else if (input[i] == '}')
      block_level--;

    if (block_level < 0 || block_level > 1) return QValidator::Invalid;
  }

  if (block_level != 0) return QValidator::Invalid;

  // Make sure the tags are valid
  int pos = 0;
  while ((pos = tag_regexp.indexIn(input, pos)) != -1) {
    if (!OrganiseFormat::kKnownTags.contains(tag_regexp.cap(1)))
      return QValidator::Invalid;

    pos += tag_regexp.matchedLength();
  }

  return QValidator::Acceptable;

}

OrganiseFormat::SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent) {}

OrganiseFormat::SyntaxHighlighter::SyntaxHighlighter(QTextEdit *parent)
    : QSyntaxHighlighter(parent) {}

OrganiseFormat::SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void OrganiseFormat::SyntaxHighlighter::highlightBlock(const QString &text) {

  const bool light = QApplication::palette().color(QPalette::Base).value() > 128;
  const QRgb block_color = light ? kBlockColorLight : kBlockColorDark;
  const QRgb valid_tag_color = light ? kValidTagColorLight : kValidTagColorDark;
  const QRgb invalid_tag_color = light ? kInvalidTagColorLight : kInvalidTagColorDark;

  QRegExp tag_regexp(kTagPattern);
  QRegExp block_regexp(kBlockPattern);

  QTextCharFormat block_format;
  block_format.setBackground(QColor(block_color));

  // Reset formatting
  setFormat(0, text.length(), QTextCharFormat());

  // Blocks
  int pos = 0;
  while ((pos = block_regexp.indexIn(text, pos)) != -1) {
    setFormat(pos, block_regexp.matchedLength(), block_format);
    pos += block_regexp.matchedLength();
  }

  // Tags
  pos = 0;
  while ((pos = tag_regexp.indexIn(text, pos)) != -1) {
    QTextCharFormat f = format(pos);
    f.setForeground(QColor(OrganiseFormat::kKnownTags.contains(tag_regexp.cap(1)) ? valid_tag_color : invalid_tag_color));

    setFormat(pos, tag_regexp.matchedLength(), f);
    pos += tag_regexp.matchedLength();
  }

}

