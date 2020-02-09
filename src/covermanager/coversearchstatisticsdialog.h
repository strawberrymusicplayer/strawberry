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

#ifndef COVERSEARCHSTATISTICSDIALOG_H
#define COVERSEARCHSTATISTICSDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

class QWidget;
class QVBoxLayout;

class Ui_CoverSearchStatisticsDialog;
struct CoverSearchStatistics;

class CoverSearchStatisticsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CoverSearchStatisticsDialog(QWidget *parent = nullptr);
  ~CoverSearchStatisticsDialog();

  void Show(const CoverSearchStatistics& statistics);

 private:
  void AddLine(const QString &label, const QString &value);
  void AddSpacer();

 private:
  Ui_CoverSearchStatisticsDialog *ui_;
  QVBoxLayout *details_layout_;
};

#endif  // COVERSEARCHSTATISTICSDIALOG_H

