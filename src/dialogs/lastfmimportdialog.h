/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LASTFMIMPORTDIALOG_H
#define LASTFMIMPORTDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

#include "includes/shared_ptr.h"

#include "ui_lastfmimportdialog.h"

class QCloseEvent;
class LastFMImport;

class LastFMImportDialog : public QDialog {
  Q_OBJECT

 public:
  explicit LastFMImportDialog(SharedPtr<LastFMImport> lastfm_import, QWidget *parent = nullptr);
  ~LastFMImportDialog() override;

 protected:
  void closeEvent(QCloseEvent *e) override;

 private:
  void ResetFinished();
  void Reset();

 private Q_SLOTS:
  void Start();
  void Cancel();
  void Close();
  void UpdateGoButtonState();

 public Q_SLOTS:
  void Finished();
  void FinishedWithError(const QString &error);
  void UpdateTotal(const int lastplayed_total, const int playcount_total);
  void UpdateProgress(const int lastplayed_received, const int playcount_received);

 private:
  Ui_LastFMImportDialog *ui_;
  SharedPtr<LastFMImport> lastfm_import_;

  bool finished_;
  int playcount_total_;
  int lastplayed_total_;
};

#endif  // LASTFMIMPORTDIALOG_H
