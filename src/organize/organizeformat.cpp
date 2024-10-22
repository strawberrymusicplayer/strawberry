/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QChar>
#include <QStringList>
#include <QRegularExpression>
#include <QFileInfo>
#include <QValidator>

#include "constants/filenameconstants.h"
#include "constants/timeconstants.h"
#include "utilities/transliterate.h"
#include "core/song.h"

#include "organizeformat.h"
#include "organizeformatvalidator.h"

using namespace Qt::Literals::StringLiterals;

const char OrganizeFormat::kBlockPattern[] = "\\{([^{}]+)\\}";
const char OrganizeFormat::kTagPattern[] = "\\%([a-zA-Z]*)";

const QStringList OrganizeFormat::kKnownTags = QStringList() << u"title"_s
                                                             << u"album"_s
                                                             << u"artist"_s
                                                             << u"artistinitial"_s
                                                             << u"albumartist"_s
                                                             << u"composer"_s
                                                             << u"track"_s
                                                             << u"disc"_s
                                                             << u"year"_s
                                                             << u"originalyear"_s
                                                             << u"genre"_s
                                                             << u"comment"_s
                                                             << u"length"_s
                                                             << u"bitrate"_s
                                                             << u"samplerate"_s
                                                             << u"bitdepth"_s
                                                             << u"extension"_s
                                                             << u"performer"_s
                                                             << u"grouping"_s
                                                             << u"lyrics"_s;

const QStringList OrganizeFormat::kUniqueTags = QStringList() << u"title"_s
                                                              << u"track"_s;

OrganizeFormat::OrganizeFormat(const QString &format)
    : format_(format),
      remove_problematic_(false),
      remove_non_fat_(false),
      remove_non_ascii_(false),
      allow_ascii_ext_(false),
      replace_spaces_(true) {}

void OrganizeFormat::set_format(const QString &v) {
  format_ = v;
  format_.replace(u'\\', u'/');
}

bool OrganizeFormat::IsValid() const {

  int pos = 0;
  QString format_copy(format_);

  OrganizeFormatValidator v;
  return v.validate(format_copy, pos) == QValidator::Acceptable;

}

OrganizeFormat::GetFilenameForSongResult OrganizeFormat::GetFilenameForSong(const Song &song, QString extension) const {

  bool unique_filename = false;
  QString filepath = ParseBlock(format_, song, &unique_filename);

  if (filepath.isEmpty()) {
    filepath = song.basefilename();
  }

  {
    QFileInfo fileinfo(filepath);
    if (fileinfo.completeBaseName().isEmpty()) {
      // Avoid having empty filenames, or filenames with extension only: in this case, keep the original filename.
      // We remove the extension from "filename" if it exists, as song.basefilename() also contains the extension.
      QString path = fileinfo.path();
      filepath.clear();
      if (!path.isEmpty()) {
        filepath.append(path);
        if (path.right(1) != u'/') {
          filepath.append(u'/');
        }
      }
      filepath.append(song.basefilename());
    }
  }

  if (filepath.isEmpty() || (filepath.contains(u'/') && (filepath.section(u'/', 0, -2).isEmpty() || filepath.section(u'/', 0, -2).isEmpty()))) {
    return GetFilenameForSongResult();
  }

  if (remove_problematic_) {
    static const QRegularExpression regex_problematic_characters(QLatin1String(kProblematicCharactersRegex), QRegularExpression::PatternOption::CaseInsensitiveOption);
    filepath = filepath.remove(regex_problematic_characters);
  }
  if (remove_non_fat_ || (remove_non_ascii_ && !allow_ascii_ext_)) filepath = Utilities::Transliterate(filepath);
  if (remove_non_fat_) {
    static const QRegularExpression regex_invalid_fat_characters(QLatin1String(kInvalidFatCharactersRegex), QRegularExpression::PatternOption::CaseInsensitiveOption);
    filepath = filepath.remove(regex_invalid_fat_characters);
  }

  if (remove_non_ascii_) {
    int ascii = 128;
    if (allow_ascii_ext_) ascii = 255;
    QString stripped;
    for (int i = 0; i < filepath.length(); ++i) {
      const QChar c = filepath[i];
      if (c.unicode() < ascii) {
        stripped.append(c);
      }
      else {
        const QString decomposition = c.decomposition();
        if (!decomposition.isEmpty() && decomposition[0].unicode() < ascii) {
          stripped.append(decomposition[0]);
        }
      }
    }
    filepath = stripped;
  }

  // Remove repeated whitespaces in the filepath.
  filepath = filepath.simplified();

  // Fixup extension
  QFileInfo info(filepath);
  filepath.clear();
  if (extension.isEmpty()) {
    if (info.suffix().isEmpty()) {
      extension = QFileInfo(song.url().toLocalFile()).suffix();
    }
    else {
      extension = info.suffix();
    }
  }
  if (!info.path().isEmpty() && info.path() != u'.') {
    filepath.append(info.path());
    filepath.append(u'/');
  }
  filepath.append(info.completeBaseName());

  // Fix any parts of the path that start with dots.
  QStringList parts_old = filepath.split(u'/');
  QStringList parts_new;
  for (int i = 0; i < parts_old.count(); ++i) {
    QString part = parts_old[i];
    for (int j = 0; j < kInvalidPrefixCharactersCount; ++j) {
      if (part.startsWith(QLatin1Char(kInvalidPrefixCharacters[j]))) {
        part = part.remove(0, 1);
        break;
      }
    }
    part = part.trimmed();
    parts_new.append(part);
  }
  filepath = parts_new.join(u'/');

  if (replace_spaces_) {
    static const QRegularExpression regex_whitespaces(u"\\s"_s);
    filepath.replace(regex_whitespaces, u"_"_s);
  }

  if (!extension.isEmpty()) {
    filepath.append(u".%1"_s.arg(extension));
  }

  return GetFilenameForSongResult(filepath, unique_filename);

}

QString OrganizeFormat::ParseBlock(QString block, const Song &song, bool *have_tagdata, bool *any_empty) const {

  // Find any blocks first
  qint64 pos = 0;
  static const QRegularExpression block_regexp(QString::fromLatin1(kBlockPattern));
  QRegularExpressionMatch re_match;
  for (re_match = block_regexp.match(block, pos); re_match.hasMatch(); re_match = block_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    // Recursively parse the block
    bool empty = false;
    QString value = ParseBlock(re_match.captured(1), song, have_tagdata, &empty);
    if (empty) value = ""_L1;

    // Replace the block's value
    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  // Now look for tags
  bool empty = false;
  pos = 0;
  static const QRegularExpression tag_regexp(QString::fromLatin1(kTagPattern));
  for (re_match = tag_regexp.match(block, pos); re_match.hasMatch(); re_match = tag_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    const QString tag = re_match.captured(1);
    const QString value = TagValue(tag, song);
    if (value.isEmpty()) {
      empty = true;
    }
    else if (have_tagdata && kUniqueTags.contains(tag)) {
      *have_tagdata = true;
    }

    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  if (any_empty) {
    *any_empty = empty;
  }

  return block;

}

QString OrganizeFormat::TagValue(const QString &tag, const Song &song) const {

  QString value;

  if (tag == "title"_L1) {
    value = song.title();
  }
  else if (tag == "album"_L1) {
    value = song.album();
  }
  else if (tag == "artist"_L1) {
    value = song.artist();
  }
  else if (tag == "composer"_L1) {
    value = song.composer();
  }
  else if (tag == "performer"_L1) {
    value = song.performer();
  }
  else if (tag == "grouping"_L1) {
    value = song.grouping();
  }
  else if (tag == "lyrics"_L1) {
    value = song.lyrics();
  }
  else if (tag == "genre"_L1) {
    value = song.genre();
  }
  else if (tag == "comment"_L1) {
    value = song.comment();
  }
  else if (tag == "year"_L1) {
    value = QString::number(song.year());
  }
  else if (tag == "originalyear"_L1) {
    value = QString::number(song.effective_originalyear());
  }
  else if (tag == "track"_L1) {
    value = QString::number(song.track());
  }
  else if (tag == "disc"_L1) {
    value = QString::number(song.disc());
  }
  else if (tag == "length"_L1) {
    value = QString::number(song.length_nanosec() / kNsecPerSec);
  }
  else if (tag == "bitrate"_L1) {
    value = QString::number(song.bitrate());
  }
  else if (tag == "samplerate"_L1) {
    value = QString::number(song.samplerate());
  }
  else if (tag == "bitdepth"_L1) {
    value = QString::number(song.bitdepth());
  }
  else if (tag == "extension"_L1) {
    value = QFileInfo(song.url().toLocalFile()).suffix();
  }
  else if (tag == "artistinitial"_L1) {
    value = song.effective_albumartist().trimmed();
    if (!value.isEmpty()) {
      static const QRegularExpression regex_the(u"^the\\s+"_s, QRegularExpression::CaseInsensitiveOption);
      value = value.remove(regex_the);
      value = value[0].toUpper();
    }
  }
  else if (tag == "albumartist"_L1) {
    value = song.is_compilation() ? u"Various Artists"_s : song.effective_albumartist();
  }

  if (value == u'0' || value == "-1"_L1) value = ""_L1;

  // Prepend a 0 to single-digit track numbers
  if (tag == "track"_L1 && value.length() == 1) value.prepend(u'0');

  // Replace characters that really shouldn't be in paths
  static const QRegularExpression regex_invalid_dir_characters(QString::fromLatin1(kInvalidDirCharactersRegex), QRegularExpression::PatternOption::CaseInsensitiveOption);
  value = value.remove(regex_invalid_dir_characters);
  if (remove_problematic_) value = value.remove(u'.');
  value = value.trimmed();

  return value;

}
