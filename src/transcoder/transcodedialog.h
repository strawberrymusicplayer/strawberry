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

#ifndef TRANSCODEDIALOG_H
#define TRANSCODEDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QBasicTimer>
#include <QString>
#include <QStringList>

class QWidget;
class QMainWindow;
class QPushButton;
class QTimerEvent;
class QShowEvent;
class QCloseEvent;
class Transcoder;
class Ui_TranscodeDialog;
class Ui_TranscodeLogDialog;
struct TranscoderPreset;

class TranscodeDialog : public QDialog {
  Q_OBJECT

 public:
  explicit TranscodeDialog(QMainWindow *mainwindow, QWidget *parent = nullptr);
  ~TranscodeDialog() override;

  void SetFilenames(const QStringList &filenames);
  void SetImportFilenames(const QStringList &filenames, const QString &import_dir);

 protected:
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

 private:
  void LoadGeometry();
  void SaveGeometry();
  void SetWorking(bool working);
  void UpdateStatusText();
  void UpdateProgress();
  static QString TrimPath(const QString &path);
  QString GetOutputFileName(const QString &input, const QString &input_import_dir, const TranscoderPreset &preset) const;

 private Q_SLOTS:
  void Add();
  void Import();
  void Remove();
  void Start();
  void Cancel();
  void JobComplete(const QString &input, const QString &output, bool success);
  void AllJobsComplete();
  void LogLine(const QString &message);
  void Options();
  void AddDestination();
  void accept() override;
  void reject() override;

 private:
  QMainWindow *mainwindow_;
  Ui_TranscodeDialog *ui_;
  Ui_TranscodeLogDialog *log_ui_;
  QDialog *log_dialog_;

  QBasicTimer progress_timer_;

  QPushButton *start_button_;
  QPushButton *cancel_button_;
  QPushButton *close_button_;

  QString last_add_dir_;
  QString last_import_dir_;

  Transcoder *transcoder_;
  int queued_;
  int finished_success_;
  int finished_failed_;
};

#endif  // TRANSCODEDIALOG_H
