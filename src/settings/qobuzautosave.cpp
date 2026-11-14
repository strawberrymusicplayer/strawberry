// SPDX-License-Identifier: GPL-3.0-or-later

#include "qobuzautosave.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QObject>
#include <QSpinBox>

#include "ui_qobuzsettingspage.h"

void ConnectQobuzAutoSave(QObject *context, Ui_QobuzSettingsPage *ui, const std::function<void()> &save) {

  for (QCheckBox *checkbox : {ui->enable, ui->checkbox_download_album_covers, ui->checkbox_remove_remastered,
                              ui->checkbox_show_search_album_quality, ui->checkbox_show_search_album_edition}) {
    QObject::connect(checkbox, &QCheckBox::toggled, context, save);
  }
  QObject::connect(ui->format, &QComboBox::currentIndexChanged, context, save);
  for (QSpinBox *spinbox : {ui->searchdelay, ui->artistssearchlimit, ui->albumssearchlimit, ui->songssearchlimit}) {
    spinbox->setKeyboardTracking(false);
    QObject::connect(spinbox, &QSpinBox::valueChanged, context, save);
  }
  for (QLineEdit *field : {ui->app_id, ui->app_secret, ui->private_key}) {
    QObject::connect(field, &QLineEdit::editingFinished, context, save);
  }

}
