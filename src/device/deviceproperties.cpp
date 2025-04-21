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

#include <functional>
#include <utility>

#include <QtGlobal>
#include <QWidget>
#include <QDialog>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QStackedWidget>
#include <QTableWidget>

#include "includes/shared_ptr.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "widgets/freespacebar.h"
#include "connecteddevice.h"
#include "devicelister.h"
#include "devicemanager.h"
#include "deviceproperties.h"
#include "transcoder/transcoder.h"
#include "ui_deviceproperties.h"

using namespace Qt::Literals::StringLiterals;

DeviceProperties::DeviceProperties(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_DeviceProperties),
      device_manager_(nullptr),
      updating_formats_(false) {

  ui_->setupUi(this);

  QObject::connect(ui_->open_device, &QPushButton::clicked, this, &DeviceProperties::OpenDevice);

  // Maximum height of the icon widget
  ui_->icon->setMaximumHeight(ui_->icon->iconSize().height() + ui_->icon->horizontalScrollBar()->sizeHint().height() + ui_->icon->spacing() * 2 + 5);

}

DeviceProperties::~DeviceProperties() { delete ui_; }

void DeviceProperties::Init(const SharedPtr<DeviceManager> device_manager) {

  device_manager_ = device_manager;
  QObject::connect(&*device_manager_, &DeviceManager::dataChanged, this, &DeviceProperties::ModelChanged);
  QObject::connect(&*device_manager_, &DeviceManager::rowsInserted, this, &DeviceProperties::ModelChanged);
  QObject::connect(&*device_manager_, &DeviceManager::rowsRemoved, this, &DeviceProperties::ModelChanged);

}

void DeviceProperties::ShowDevice(const QModelIndex &idx) {

  if (ui_->icon->count() == 0) {
    // Only load the icons the first time the dialog is shown
    const QStringList icon_names = QStringList() << u"device"_s
                                                 << u"device-usb-drive"_s
                                                 << u"device-usb-flash"_s
                                                 << u"media-optical"_s
                                                 << u"device-ipod"_s
                                                 << u"device-ipod-nano"_s
                                                 << u"device-phone"_s;


    for (const QString &icon_name : icon_names) {
      QListWidgetItem *item = new QListWidgetItem(IconLoader::Load(icon_name), QString(), ui_->icon);
      item->setData(Qt::UserRole, icon_name);
    }

    // Load the transcode formats the first time the dialog is shown
    const QList<TranscoderPreset> presets = Transcoder::GetAllPresets();
    for (const TranscoderPreset &preset : presets) {
      ui_->transcode_format->addItem(preset.name_, QVariant::fromValue(preset.filetype_));
    }
    ui_->transcode_format->model()->sort(0);
  }

  index_ = idx;

  // Basic information
  ui_->name->setText(index_.data(DeviceManager::Role_FriendlyName).toString());

  // Find the right icon
  QString icon_name = index_.data(DeviceManager::Role_IconName).toString();
  for (int i = 0; i < ui_->icon->count(); ++i) {
    if (ui_->icon->item(i)->data(Qt::UserRole).toString() == icon_name) {
      ui_->icon->setCurrentRow(i);
      break;
    }
  }

  UpdateHardwareInfo();
  UpdateFormats();

  show();

}

void DeviceProperties::AddHardwareInfo(const int row, const QString &key, const QString &value) {
  ui_->hardware_info->setItem(row, 0, new QTableWidgetItem(key));
  ui_->hardware_info->setItem(row, 1, new QTableWidgetItem(value));
}

void DeviceProperties::ModelChanged() {

  if (!isVisible()) return;

  if (!index_.isValid()) {
    reject();  // Device went away
  }
  else {
    UpdateHardwareInfo();
    UpdateFormats();
  }

}

void DeviceProperties::UpdateHardwareInfo() {

  // Hardware information
  QString id = index_.data(DeviceManager::Role_UniqueId).toString();
  if (DeviceLister *lister = device_manager_->GetLister(index_)) {
    QVariantMap info = lister->DeviceHardwareInfo(id);

    // Remove empty items
    QStringList keys = info.keys();
    for (const QString &key : std::as_const(keys)) {
      if (info[key].isNull() || info[key].toString().isEmpty())
        info.remove(key);
    }

    ui_->hardware_info_stack->setCurrentWidget(ui_->hardware_info_page);
    ui_->hardware_info->clear();
    ui_->hardware_info->setRowCount(2 + static_cast<int>(info.count()));

    int row = 0;
    AddHardwareInfo(row++, tr("Model"), lister->DeviceModel(id));
    AddHardwareInfo(row++, tr("Manufacturer"), lister->DeviceManufacturer(id));
    keys = info.keys();
    for (const QString &key : std::as_const(keys)) {
      AddHardwareInfo(row++, key, info[key].toString());
    }

    ui_->hardware_info->sortItems(0);
  }
  else {
    ui_->hardware_info_stack->setCurrentWidget(ui_->hardware_info_not_connected_page);
  }

  // Size
  quint64 total = index_.data(DeviceManager::Role_Capacity).toULongLong();

  QVariant free_var = index_.data(DeviceManager::Role_FreeSpace);
  if (free_var.isValid()) {
    quint64 free = free_var.toULongLong();

    ui_->free_space_bar->set_total_bytes(total);
    ui_->free_space_bar->set_free_bytes(free);
    ui_->free_space_bar->show();
  }
  else {
    ui_->free_space_bar->hide();
  }

}

void DeviceProperties::UpdateFormats() {

  DeviceLister *lister = device_manager_->GetLister(index_);
  SharedPtr<ConnectedDevice> device = device_manager_->GetConnectedDevice(index_);

  // Transcode mode
  MusicStorage::TranscodeMode mode = static_cast<MusicStorage::TranscodeMode>(index_.data(DeviceManager::Role_TranscodeMode).toInt());
  switch (mode) {
    case MusicStorage::TranscodeMode::Transcode_Always:
      ui_->transcode_all->setChecked(true);
      break;

    case MusicStorage::TranscodeMode::Transcode_Never:
      ui_->transcode_off->setChecked(true);
      break;

    case MusicStorage::TranscodeMode::Transcode_Unsupported:
    default:
      ui_->transcode_unsupported->setChecked(true);
      break;
  }

  // If there's no lister then the device is physically disconnected
  if (!lister) {
    ui_->formats_stack->setCurrentWidget(ui_->formats_page_not_connected);
    ui_->open_device->setEnabled(false);
    return;
  }

  // If there's a lister but no device then the user just needs to open the
  // device.  This will cause a rescan so we don't do it automatically.
  if (!device) {
    ui_->formats_stack->setCurrentWidget(ui_->formats_page_not_connected);
    ui_->open_device->setEnabled(true);
    return;
  }

  if (!updating_formats_) {
    // Get the device's supported formats list.  This takes a long time and it blocks, so do it in the background.
    supported_formats_.clear();

    QFuture<bool> future = QtConcurrent::run(&ConnectedDevice::GetSupportedFiletypes, device, &supported_formats_);
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, &DeviceProperties::UpdateFormatsFinished);
    watcher->setFuture(future);

    ui_->formats_stack->setCurrentWidget(ui_->formats_page_loading);
    updating_formats_ = true;
  }

}

void DeviceProperties::accept() {

  QDialog::accept();

  // Transcode mode
  MusicStorage::TranscodeMode mode = MusicStorage::TranscodeMode::Transcode_Unsupported;
  if (ui_->transcode_all->isChecked())
    mode = MusicStorage::TranscodeMode::Transcode_Always;
  else if (ui_->transcode_off->isChecked())
    mode = MusicStorage::TranscodeMode::Transcode_Never;
  else if (ui_->transcode_unsupported->isChecked())
    mode = MusicStorage::TranscodeMode::Transcode_Unsupported;

  // Transcode format
  Song::FileType format = static_cast<Song::FileType>(ui_->transcode_format->itemData(ui_->transcode_format->currentIndex()).toInt());

  // By default no icon is selected and thus no current item is selected
  QString icon_name;
  if (ui_->icon->currentItem() != nullptr) {
    icon_name = ui_->icon->currentItem()->data(Qt::UserRole).toString();
  }

  device_manager_->SetDeviceOptions(index_, ui_->name->text(), icon_name, mode, format);

}

void DeviceProperties::OpenDevice() { device_manager_->Connect(index_); }

void DeviceProperties::UpdateFormatsFinished() {

  QFutureWatcher<bool> *watcher = static_cast<QFutureWatcher<bool>*>(sender());
  bool result = watcher->result();
  watcher->deleteLater();

  updating_formats_ = false;

  if (!result) {
    supported_formats_.clear();
  }

  // Hide widgets if there are no supported types
  ui_->supported_formats_container->setVisible(!supported_formats_.isEmpty());
  ui_->transcode_unsupported->setEnabled(!supported_formats_.isEmpty());

  if (ui_->transcode_unsupported->isChecked() && supported_formats_.isEmpty()) {
    ui_->transcode_off->setChecked(true);
  }

  // Populate supported types list
  ui_->supported_formats->clear();
  for (Song::FileType type : std::as_const(supported_formats_)) {
    QListWidgetItem *item = new QListWidgetItem(Song::TextForFiletype(type));
    ui_->supported_formats->addItem(item);
  }
  ui_->supported_formats->sortItems();

  // Set the format combobox item
  TranscoderPreset preset = Transcoder::PresetForFileType(static_cast<Song::FileType>(index_.data(DeviceManager::Role_TranscodeFormat).toInt()));
  if (preset.filetype_ == Song::FileType::Unknown) {
    // The user hasn't chosen a format for this device yet,
    // so work our way down a list of some preferred formats, picking the first one that is supported
    preset = Transcoder::PresetForFileType(Transcoder::PickBestFormat(supported_formats_));
  }
  ui_->transcode_format->setCurrentIndex(ui_->transcode_format->findText(preset.name_));

  ui_->formats_stack->setCurrentWidget(ui_->formats_page);

}
