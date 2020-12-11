/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ORGANISEFORMAT_H
#define ORGANISEFORMAT_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QRgb>
#include <QSyntaxHighlighter>
#include <QValidator>

class QTextDocument;
class QTextEdit;
class Song;

class OrganizeFormat {

 public:
  explicit OrganizeFormat(const QString &format = QString());

  static const char *kTagPattern;
  static const char *kBlockPattern;
  static const QStringList kKnownTags;
  static const QRegularExpression kInvalidDirCharacters;
  static const QRegularExpression kProblematicCharacters;
  static const QRegularExpression kInvalidFatCharacters;

  static const char kInvalidPrefixCharacters[];
  static const int kInvalidPrefixCharactersCount;

  QString format() const { return format_; }
  bool remove_problematic() const { return remove_problematic_; }
  bool remove_non_fat() const { return remove_non_fat_; }
  bool remove_non_ascii() const { return remove_non_ascii_; }
  bool allow_ascii_ext() const { return allow_ascii_ext_; }
  bool replace_spaces() const { return replace_spaces_; }

  void set_format(const QString &v);
  void set_remove_problematic(const bool v) { remove_problematic_ = v; }
  void set_remove_non_fat(const bool v) { remove_non_fat_ = v; }
  void set_remove_non_ascii(const bool v) { remove_non_ascii_ = v; }
  void set_allow_ascii_ext(const bool v) { allow_ascii_ext_ = v; }
  void set_replace_spaces(const bool v) { replace_spaces_ = v; }

  bool IsValid() const;
  QString GetFilenameForSong(const Song &song, QString extension = QString()) const;

  class Validator : public QValidator {
   public:
    explicit Validator(QObject *parent = nullptr);
    QValidator::State validate(QString &input, int&) const override;
  };

  class SyntaxHighlighter : public QSyntaxHighlighter {
   public:
    static const QRgb kValidTagColorLight;
    static const QRgb kInvalidTagColorLight;
    static const QRgb kBlockColorLight;
    static const QRgb kValidTagColorDark;
    static const QRgb kInvalidTagColorDark;
    static const QRgb kBlockColorDark;

    explicit SyntaxHighlighter(QObject *parent = nullptr);
    explicit SyntaxHighlighter(QTextEdit *parent);
    explicit SyntaxHighlighter(QTextDocument *parent);
    void highlightBlock(const QString &text) override;
  };

 private:
  QString ParseBlock(QString block, const Song &song, bool *any_empty = nullptr) const;
  QString TagValue(const QString &tag, const Song &song) const;

  QString format_;
  bool remove_problematic_;
  bool remove_non_fat_;
  bool remove_non_ascii_;
  bool allow_ascii_ext_;
  bool replace_spaces_;

};

#endif  // ORGANISEFORMAT_H

