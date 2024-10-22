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

#include <QWidget>
#include <QDialog>
#include <QVariant>
#include <QString>
#include <QSettings>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>

#include "albumcoverexport.h"
#include "ui_albumcoverexport.h"

#include "core/settings.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "AlbumCoverExport";
}

AlbumCoverExport::AlbumCoverExport(QWidget *parent) : QDialog(parent), ui_(new Ui_AlbumCoverExport) {

  ui_->setupUi(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QObject::connect(ui_->forceSize, &QCheckBox::checkStateChanged, this, &AlbumCoverExport::ForceSizeToggled);
#else
  QObject::connect(ui_->forceSize, &QCheckBox::stateChanged, this, &AlbumCoverExport::ForceSizeToggled);
#endif

}

AlbumCoverExport::~AlbumCoverExport() { delete ui_; }

AlbumCoverExport::DialogResult AlbumCoverExport::Exec() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Restore last accepted settings
  ui_->fileName->setText(s.value("fileName", u"cover"_s).toString());
  ui_->doNotOverwrite->setChecked(static_cast<OverwriteMode>(s.value("overwrite", static_cast<int>(OverwriteMode::None)).toInt()) == OverwriteMode::None);
  ui_->overwriteAll->setChecked(static_cast<OverwriteMode>(s.value("overwrite", static_cast<int>(OverwriteMode::All)).toInt()) == OverwriteMode::All);
  ui_->overwriteSmaller->setChecked(static_cast<OverwriteMode>(s.value("overwrite", static_cast<int>(OverwriteMode::Smaller)).toInt()) == OverwriteMode::Smaller);
  ui_->forceSize->setChecked(s.value("forceSize", false).toBool());
  ui_->width->setText(s.value("width", ""_L1).toString());
  ui_->height->setText(s.value("height", ""_L1).toString());
  ui_->export_downloaded->setChecked(s.value("export_downloaded", true).toBool());
  ui_->export_embedded->setChecked(s.value("export_embedded", false).toBool());

  ForceSizeToggled(ui_->forceSize->checkState());

  DialogResult result = DialogResult();
  result.cancelled_ = (exec() == QDialog::Rejected);

  if (!result.cancelled_) {
    QString fileName = ui_->fileName->text();
    if (fileName.isEmpty()) {
      fileName = u"cover"_s;
    }
    OverwriteMode overwrite_mode = ui_->doNotOverwrite->isChecked() ? OverwriteMode::None : (ui_->overwriteAll->isChecked() ? OverwriteMode::All : OverwriteMode::Smaller);
    bool forceSize = ui_->forceSize->isChecked();
    QString width = ui_->width->text();
    QString height = ui_->height->text();

    s.setValue("fileName", fileName);
    s.setValue("overwrite", static_cast<int>(overwrite_mode));
    s.setValue("forceSize", forceSize);
    s.setValue("width", width);
    s.setValue("height", height);
    s.setValue("export_downloaded", ui_->export_downloaded->isChecked());
    s.setValue("export_embedded", ui_->export_embedded->isChecked());

    result.filename_ = fileName;
    result.overwrite_ = overwrite_mode;
    result.forcesize_ = forceSize;
    result.width_ = width.toInt();
    result.height_ = height.toInt();
    result.export_downloaded_ = ui_->export_downloaded->isChecked();
    result.export_embedded_ = ui_->export_embedded->isChecked();
  }

  return result;

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void AlbumCoverExport::ForceSizeToggled(Qt::CheckState state) {
#else
void AlbumCoverExport::ForceSizeToggled(int state) {
#endif
  ui_->width->setEnabled(state == Qt::Checked);
  ui_->height->setEnabled(state == Qt::Checked);
}
