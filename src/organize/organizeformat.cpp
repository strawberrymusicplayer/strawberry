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
#include <QRegularExpression>
#include <QRegularExpressionMatch>
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

#include "organizeformat.h"

const char *OrganizeFormat::kTagPattern = "\\%([a-zA-Z]*)";
const char *OrganizeFormat::kBlockPattern = "\\{([^{}]+)\\}";
const QStringList OrganizeFormat::kKnownTags = QStringList() << "title"
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

const QRegularExpression OrganizeFormat::kInvalidDirCharacters("[/\\\\]");
const QRegularExpression OrganizeFormat::kProblematicCharacters("[:?*\"<>|]");
// From http://en.wikipedia.org/wiki/8.3_filename#Directory_table
const QRegularExpression OrganizeFormat::kInvalidFatCharacters("[^a-zA-Z0-9!#\\$%&'()\\-@\\^_`{}~/. ]");

const char OrganizeFormat::kInvalidPrefixCharacters[] = ".";
const int OrganizeFormat::kInvalidPrefixCharactersCount = arraysize(OrganizeFormat::kInvalidPrefixCharacters) - 1;

const QRgb OrganizeFormat::SyntaxHighlighter::kValidTagColorLight = qRgb(64, 64, 255);
const QRgb OrganizeFormat::SyntaxHighlighter::kInvalidTagColorLight = qRgb(255, 64, 64);
const QRgb OrganizeFormat::SyntaxHighlighter::kBlockColorLight = qRgb(230, 230, 230);

const QRgb OrganizeFormat::SyntaxHighlighter::kValidTagColorDark = qRgb(128, 128, 255);
const QRgb OrganizeFormat::SyntaxHighlighter::kInvalidTagColorDark = qRgb(255, 128, 128);
const QRgb OrganizeFormat::SyntaxHighlighter::kBlockColorDark = qRgb(64, 64, 64);

OrganizeFormat::OrganizeFormat(const QString &format)
    : format_(format),
      remove_problematic_(false),
      remove_non_fat_(false),
      remove_non_ascii_(false),
      allow_ascii_ext_(false),
      replace_spaces_(true) {}

void OrganizeFormat::set_format(const QString &v) {
  format_ = v;
  format_.replace('\\', '/');
}

bool OrganizeFormat::IsValid() const {

  int pos = 0;
  QString format_copy(format_);

  Validator v;
  return v.validate(format_copy, pos) == QValidator::Acceptable;

}

QString OrganizeFormat::GetFilenameForSong(const Song &song, QString extension) const {

  QString filename = ParseBlock(format_, song);

  if (QFileInfo(filename).completeBaseName().isEmpty()) {
    // Avoid having empty filenames, or filenames with extension only: in this case, keep the original filename.
    // We remove the extension from "filename" if it exists, as song.basefilename() also contains the extension.
    QString path = QFileInfo(filename).path();
    filename.clear();
    if (!path.isEmpty()) {
      filename.append(path);
      if (path.right(1) != '/' && path.right(1) != '\\') {
        filename.append('/');
      }
    }
    filename.append(song.basefilename());
  }

  if (remove_problematic_) filename = filename.remove(kProblematicCharacters);
  if (remove_non_fat_ || (remove_non_ascii_ && !allow_ascii_ext_)) filename = Utilities::UnicodeToAscii(filename);
  if (remove_non_fat_) filename = filename.remove(kInvalidFatCharacters);

  if (remove_non_ascii_) {
    int ascii = 128;
    if (allow_ascii_ext_) ascii = 255;
    QString stripped;
    for (int i = 0 ; i < filename.length() ; ++i) {
      const QChar c = filename[i];
      if (c.unicode() < ascii) {
        stripped.append(c);
      }
      else {
        const QString decomposition = c.decomposition();
        if (!decomposition.isEmpty() && decomposition[0].unicode() < ascii)
          stripped.append(decomposition[0]);
      }
    }
    filename = stripped;
  }

  // Remove repeated whitespaces in the filename.
  filename = filename.simplified();

  QFileInfo info(filename);
  if (extension.isEmpty()) extension = info.suffix();
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

  if (replace_spaces_) filename.replace(QRegularExpression("\\s"), "_");

  if (!extension.isEmpty()) {
    filename.append(QString(".%1").arg(extension));
  }

  return filename;

}

QString OrganizeFormat::ParseBlock(QString block, const Song &song, bool *any_empty) const {

  QRegularExpression tag_regexp(kTagPattern);
  QRegularExpression block_regexp(kBlockPattern);

  // Find any blocks first
  int pos = 0;
  QRegularExpressionMatch re_match;
  for (re_match = block_regexp.match(block, pos) ;  re_match.hasMatch() ; re_match = block_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    // Recursively parse the block
    bool empty = false;
    QString value = ParseBlock(re_match.captured(1), song, &empty);
    if (empty) value = "";

    // Replace the block's value
    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  // Now look for tags
  bool empty = false;
  pos = 0;
  for (re_match = tag_regexp.match(block, pos) ; re_match.hasMatch() ; re_match = tag_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    QString value = TagValue(re_match.captured(1), song);
    if (value.isEmpty()) empty = true;

    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  if (any_empty) *any_empty = empty;
  return block;

}

QString OrganizeFormat::TagValue(const QString &tag, const Song &song) const {

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
      value.replace(QRegularExpression("^the\\s+", QRegularExpression::CaseInsensitiveOption), "");
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

OrganizeFormat::Validator::Validator(QObject *parent) : QValidator(parent) {}

QValidator::State OrganizeFormat::Validator::validate(QString &input, int&) const {

  QRegularExpression tag_regexp(kTagPattern);

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
  QRegularExpressionMatch re_match;
  int pos = 0;
  for (re_match = tag_regexp.match(input, pos) ; re_match.hasMatch() ; re_match = tag_regexp.match(input, pos)) {
    pos = re_match.capturedStart();
    if (!OrganizeFormat::kKnownTags.contains(re_match.captured(1)))
      return QValidator::Invalid;

    pos += re_match.capturedLength();
  }

  return QValidator::Acceptable;

}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent) {}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QTextEdit *parent)
    : QSyntaxHighlighter(parent) {}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void OrganizeFormat::SyntaxHighlighter::highlightBlock(const QString &text) {

  const bool light = QApplication::palette().color(QPalette::Base).value() > 128;
  const QRgb block_color = light ? kBlockColorLight : kBlockColorDark;
  const QRgb valid_tag_color = light ? kValidTagColorLight : kValidTagColorDark;
  const QRgb invalid_tag_color = light ? kInvalidTagColorLight : kInvalidTagColorDark;

  QRegularExpression tag_regexp(kTagPattern);
  QRegularExpression block_regexp(kBlockPattern);

  QTextCharFormat block_format;
  block_format.setBackground(QColor(block_color));

  // Reset formatting
  setFormat(0, text.length(), QTextCharFormat());

  // Blocks
  QRegularExpressionMatch re_match;
  int pos = 0;
  for (re_match = block_regexp.match(text, pos) ; re_match.hasMatch() ; re_match = block_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    setFormat(pos, re_match.capturedLength(), block_format);
    pos += re_match.capturedLength();
  }

  // Tags
  pos = 0;
  for (re_match = tag_regexp.match(text, pos) ; re_match.hasMatch() ; re_match = tag_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    QTextCharFormat f = format(pos);
    f.setForeground(QColor(OrganizeFormat::kKnownTags.contains(re_match.captured(1)) ? valid_tag_color : invalid_tag_color));

    setFormat(pos, re_match.capturedLength(), f);
    pos += re_match.capturedLength();
  }

}

