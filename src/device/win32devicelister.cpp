/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <windows.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <devpropdef.h>

#include <cstring>
#include <chrono>
#include <utility>

#include <QtGlobal>
#include <QByteArray>
#include <QDir>
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "core/logging.h"
#include "utilities/diskutils.h"
#include "win32devicelister.h"

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace {

// Defined here instead of through initguid.h, so the definitions don't depend on the include order of the Windows headers.
constexpr GUID kGuidDevInterfaceDisk = { 0x53F56307L, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B } };
constexpr GUID kGuidDevInterfaceVolume = { 0x53F5630DL, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B } };
constexpr DEVPROPKEY kDevPKeyDeviceInstanceId = { { 0x78C34FC8, 0x104A, 0x4ACA, { 0x9E, 0xA4, 0x52, 0x4D, 0x52, 0x99, 0x6E, 0x57 } }, 256 };

// Volume arrival is reported before the drive letter is assigned, so rescan a few times after a notification.
constexpr int kRescanCount = 3;
constexpr auto kRescanInterval = 1s;

constexpr auto kLogicalDrivesInterval = 2s;

// Drives that don't report their bus type can be virtual drives that never do, so they are only retried a limited number of times.
constexpr int kUnknownBusRetries = 15;

constexpr int kEjectAttempts = 3;

class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
  ~ScopedHandle() {
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
  }
  HANDLE get() const { return handle_; }
  bool isValid() const { return handle_ != INVALID_HANDLE_VALUE; }

 private:
  Q_DISABLE_COPY(ScopedHandle)
  HANDLE handle_;
};

// Prevents Windows from showing a modal "There is no disk in the drive" dialog when probing a drive without media, such as an empty memory card reader, and makes the call fail instead.
class ScopedCriticalErrorMode {
 public:
  ScopedCriticalErrorMode() : previous_mode_(0), mode_set_(SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous_mode_) != FALSE) {}
  ~ScopedCriticalErrorMode() {
    if (mode_set_) SetThreadErrorMode(previous_mode_, nullptr);
  }

 private:
  Q_DISABLE_COPY(ScopedCriticalErrorMode)
  DWORD previous_mode_;
  bool mode_set_;
};

const wchar_t *ToWChar(const QString &str) {
  return reinterpret_cast<const wchar_t*>(str.utf16());
}

HANDLE OpenDevice(const QString &device_path) {
  // No access rights are needed for querying properties, this avoids requiring elevated privileges.
  return CreateFileW(ToWChar(device_path), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}

bool GetDeviceNumber(HANDLE handle, STORAGE_DEVICE_NUMBER &device_number) {
  DWORD bytes_returned = 0;
  return DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &device_number, sizeof(device_number), &bytes_returned, nullptr) != FALSE;
}

QString DescriptorString(const QByteArray &buffer, const DWORD offset) {

  if (offset == 0 || offset >= static_cast<DWORD>(buffer.size())) return QString();
  const char *str = buffer.constData() + offset;
  return QString::fromLatin1(str, static_cast<qsizetype>(strnlen(str, static_cast<size_t>(buffer.size()) - offset))).trimmed();

}

bool IsHotplugBus(const STORAGE_BUS_TYPE bus_type) {

  switch (bus_type) {
    case BusTypeUsb:
    case BusType1394:
    case BusTypeSd:
    case BusTypeMmc:
      return true;
    default:
      return false;
  }

}

DWORD CALLBACK NotificationCallback(HCMNOTIFICATION notification, PVOID context, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA event_data, DWORD event_data_size) {

  Q_UNUSED(notification)
  Q_UNUSED(event_data)
  Q_UNUSED(event_data_size)

  // Called on a system thread pool thread, so only queue a rescan on the lister's thread.
  if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL || action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
    QMetaObject::invokeMethod(static_cast<QObject*>(context), "ScheduleRescan", Qt::QueuedConnection);
  }

  return ERROR_SUCCESS;

}

DEVINST FindDiskDevInst(const DWORD disk_number) {

  ULONG list_size = 0;
  if (CM_Get_Device_Interface_List_SizeW(&list_size, const_cast<GUID*>(&kGuidDevInterfaceDisk), nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS || list_size <= 1) {
    return 0;
  }

  QList<wchar_t> interface_list(list_size, L'\0');
  if (CM_Get_Device_Interface_ListW(const_cast<GUID*>(&kGuidDevInterfaceDisk), nullptr, interface_list.data(), list_size, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS) {
    return 0;
  }

  for (const wchar_t *interface_path = interface_list.constData(); *interface_path != L'\0'; interface_path += wcslen(interface_path) + 1) {

    STORAGE_DEVICE_NUMBER device_number{};
    {
      const ScopedHandle handle(CreateFileW(interface_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
      if (!handle.isValid() || !GetDeviceNumber(handle.get(), device_number)) continue;
    }
    if (device_number.DeviceNumber != disk_number) continue;

    wchar_t instance_id[MAX_DEVICE_ID_LEN] = {};
    ULONG instance_id_size = sizeof(instance_id);
    DEVPROPTYPE property_type = DEVPROP_TYPE_EMPTY;
    if (CM_Get_Device_Interface_PropertyW(interface_path, &kDevPKeyDeviceInstanceId, &property_type, reinterpret_cast<PBYTE>(instance_id), &instance_id_size, 0) != CR_SUCCESS || property_type != DEVPROP_TYPE_STRING) {
      return 0;
    }

    DEVINST devinst = 0;
    if (CM_Locate_DevNodeW(&devinst, instance_id, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) {
      return 0;
    }

    return devinst;

  }

  return 0;

}

DEVINST FindRemovableDevInst(const DEVINST disk_devinst) {

  // Walk up the device tree, it ends when the root device, which has no parent, is reached.
  DEVINST devinst = disk_devinst;
  while (devinst != 0) {
    ULONG capabilities = 0;
    ULONG capabilities_size = sizeof(capabilities);
    if (CM_Get_DevNode_Registry_PropertyW(devinst, CM_DRP_CAPABILITIES, nullptr, &capabilities, &capabilities_size, 0) == CR_SUCCESS && (capabilities & CM_DEVCAP_REMOVABLE) != 0) {
      return devinst;
    }
    DEVINST parent_devinst = 0;
    if (CM_Get_Parent(&parent_devinst, devinst, 0) != CR_SUCCESS) break;
    devinst = parent_devinst;
  }

  return 0;

}

}  // namespace

Win32DeviceLister::DeviceData::DeviceData()
    : volume_serial(0),
      removable_drive(false),
      capacity(0),
      free_space(0),
      free_space_updates(0) {}

bool Win32DeviceLister::DeviceData::IsSameDevice(const DeviceData &other) const {

  // Free space is not compared, it's updated through UpdateDeviceFreeSpace().
  return unique_id == other.unique_id &&
         root_path == other.root_path &&
         label == other.label &&
         filesystem == other.filesystem &&
         volume_serial == other.volume_serial &&
         capacity == other.capacity;

}

Win32DeviceLister::Win32DeviceLister(QObject *parent)
    : DeviceLister(parent),
      notification_(nullptr),
      shutdown_(false),
      timer_rescan_(new QTimer(this)),
      timer_logical_drives_(new QTimer(this)),
      rescans_pending_(0),
      logical_drives_(0),
      not_ready_drives_(0),
      unknown_bus_drives_(0),
      unknown_bus_retries_(0) {

  // Created here instead of in Init(), so the pointers are never written while ShutDown() can read them from another thread. As children, the timers are moved to the lister's thread together with the lister.
  timer_rescan_->setInterval(kRescanInterval);
  QObject::connect(timer_rescan_, &QTimer::timeout, this, &Win32DeviceLister::RescanTimeout);

  timer_logical_drives_->setInterval(kLogicalDrivesInterval);
  QObject::connect(timer_logical_drives_, &QTimer::timeout, this, &Win32DeviceLister::CheckLogicalDrives);

}

Win32DeviceLister::~Win32DeviceLister() {
  UnregisterNotification();
}

bool Win32DeviceLister::Init() {

  if (shutdown_) return false;

  CM_NOTIFY_FILTER filter{};
  filter.cbSize = sizeof(filter);
  filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
  filter.u.DeviceInterface.ClassGuid = kGuidDevInterfaceVolume;

  HCMNOTIFICATION notification = nullptr;
  const CONFIGRET result = CM_Register_Notification(&filter, this, NotificationCallback, &notification);
  if (result == CR_SUCCESS) {
    notification_ = notification;
    // ShutDown() can run on another thread while registering, if so, it might have missed the notification handle.
    if (shutdown_) {
      UnregisterNotification();
      return false;
    }
  }
  else {
    qLog(Error) << "Failed to register for volume notifications, error" << result;
  }

  Rescan();

  timer_logical_drives_->start();

  return true;

}

void Win32DeviceLister::ShutDown() {

  // This is normally called on the lister's thread through Exit(), but DeviceManager's destructor also calls it from its own thread when the lister's thread is still running, so only do what's safe from any thread there.
  shutdown_ = true;

  UnregisterNotification();

  if (QThread::currentThread() == thread()) {
    timer_rescan_->stop();
    timer_logical_drives_->stop();
  }

}

void Win32DeviceLister::UnregisterNotification() {

  // Exchange, so the handle is only unregistered once if this is called from two threads.
  void *notification = notification_.exchange(nullptr);
  if (notification) {
    // Waits for callbacks in progress to finish.
    CM_Unregister_Notification(static_cast<HCMNOTIFICATION>(notification));
  }

}

void Win32DeviceLister::ScheduleRescan() {

  if (shutdown_ || !notification_) return;

  rescans_pending_ = kRescanCount;
  timer_rescan_->start();

}

void Win32DeviceLister::RescanTimeout() {

  // ShutDown() can't stop the timer when it's called from another thread.
  if (shutdown_) {
    timer_rescan_->stop();
    return;
  }

  if (--rescans_pending_ <= 0) {
    timer_rescan_->stop();
  }

  Rescan();

}

void Win32DeviceLister::CheckLogicalDrives() {

  // Assigning, changing or removing a drive letter, such as through Disk Management, doesn't add or remove a volume interface, so there is no notification for it.
  // Checking the drive letters is cheap, the volumes are only fully read when something changed.
  if (shutdown_) return;

  if (static_cast<quint32>(GetLogicalDrives()) != logical_drives_) {
    Rescan();
    return;
  }

  const ScopedCriticalErrorMode critical_error_mode;

  // The same drive letters can still belong to other volumes, such as when two drive letters are swapped, a memory card is replaced, or a BitLocker protected drive is locked.
  // Check that each listed volume is still readable at its drive letter, with the same volume GUID path and volume serial number.
  QList<DeviceData> listed_devices;
  {
    QReadLocker locker(&device_data_lock_);
    listed_devices = device_data_.values();
  }
  for (const DeviceData &device_data : std::as_const(listed_devices)) {
    const QString root_path = QDir::toNativeSeparators(device_data.root_path);
    DWORD volume_serial = 0;
    if (!GetVolumeInformationW(ToWChar(root_path), nullptr, 0, &volume_serial, nullptr, nullptr, nullptr, 0) || volume_serial != device_data.volume_serial) {
      Rescan();
      return;
    }
    if (!device_data.volume_name.isEmpty()) {
      wchar_t volume_name[MAX_PATH + 1] = {};
      if (!GetVolumeNameForVolumeMountPointW(ToWChar(root_path), volume_name, MAX_PATH + 1) || QString::fromWCharArray(volume_name) != device_data.volume_name) {
        Rescan();
        return;
      }
    }
  }

  // A volume can also become readable without any notification or drive letter change, such as when a memory card is inserted in a card reader, or a BitLocker protected drive is unlocked.
  for (int i = 0; i < 26; ++i) {
    if ((not_ready_drives_ & (1UL << i)) == 0) continue;
    const QString root_path = QString(QChar(u'A' + i)) + ":\\"_L1;
    if (GetVolumeInformationW(ToWChar(root_path), nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0)) {
      Rescan();
      return;
    }
  }

  // Retry fixed drives where the bus type couldn't be read, the query can fail temporarily right after the drive was connected.
  if (unknown_bus_drives_ == 0 || unknown_bus_retries_ >= kUnknownBusRetries) return;
  ++unknown_bus_retries_;
  for (int i = 0; i < 26; ++i) {
    if ((unknown_bus_drives_ & (1UL << i)) == 0) continue;
    DeviceData device_data;
    if (ReadDeviceData(QChar(u'A' + i), device_data) != ReadResult::UnknownBus) {
      Rescan();
      return;
    }
  }

}

void Win32DeviceLister::Rescan() {

  const ScopedCriticalErrorMode critical_error_mode;

  QMap<QString, quint64> free_space_updates;
  {
    QReadLocker locker(&device_data_lock_);
    for (auto it = device_data_.constBegin(); it != device_data_.constEnd(); ++it) {
      free_space_updates.insert(it.key(), it.value().free_space_updates);
    }
  }

  QMap<QString, DeviceData> device_data_list;
  quint32 not_ready_drives = 0;
  quint32 unknown_bus_drives = 0;
  const DWORD drives = GetLogicalDrives();
  logical_drives_ = static_cast<quint32>(drives);
  for (int i = 0; i < 26; ++i) {
    if ((drives & (1UL << i)) == 0) continue;
    DeviceData device_data;
    switch (ReadDeviceData(QChar(u'A' + i), device_data)) {
      case ReadResult::Ready:
        device_data_list.insert(device_data.unique_id, device_data);
        break;
      case ReadResult::NotReady:
        not_ready_drives |= (1UL << i);
        break;
      case ReadResult::UnknownBus:
        unknown_bus_drives |= (1UL << i);
        break;
      case ReadResult::Unsuitable:
        break;
    }
  }
  not_ready_drives_ = not_ready_drives;
  // Start the retries over when other drives have an unknown bus type, but not for the same drives, so they are not retried forever.
  if (unknown_bus_drives != unknown_bus_drives_) {
    unknown_bus_drives_ = unknown_bus_drives;
    unknown_bus_retries_ = 0;
  }

  QStringList added_ids;
  QStringList removed_ids;
  QStringList changed_ids;
  {
    QWriteLocker locker(&device_data_lock_);
    for (auto it = device_data_.constBegin(); it != device_data_.constEnd(); ++it) {
      if (!device_data_list.contains(it.key())) {
        removed_ids << it.key();
      }
    }
    for (auto it = device_data_list.constBegin(); it != device_data_list.constEnd(); ++it) {
      if (!device_data_.contains(it.key())) {
        added_ids << it.key();
      }
      else if (device_data_.value(it.key()).root_path != it.value().root_path) {
        // DeviceManager ignores DeviceChanged, so report a new drive letter as removed and added, that way a connected device is closed and reconnected using the new path.
        removed_ids << it.key();
        added_ids << it.key();
      }
      else if (!device_data_.value(it.key()).IsSameDevice(it.value())) {
        changed_ids << it.key();
      }
    }
    // UpdateDeviceFreeSpace() can be called from another thread while scanning, keep the free space of the devices it updated in the meantime instead of the values read before.
    for (auto it = device_data_list.begin(); it != device_data_list.end(); ++it) {
      if (!device_data_.contains(it.key())) continue;
      const DeviceData &current_data = device_data_.value(it.key());
      if (current_data.root_path != it.value().root_path) continue;
      it.value().free_space_updates = current_data.free_space_updates;
      if (current_data.free_space_updates != free_space_updates.value(it.key(), current_data.free_space_updates)) {
        it.value().free_space = current_data.free_space;
      }
    }
    device_data_ = device_data_list;
  }

  for (const QString &id : std::as_const(removed_ids)) {
    qLog(Debug) << "Removable volume removed" << id;
    Q_EMIT DeviceRemoved(id);
  }
  for (const QString &id : std::as_const(added_ids)) {
    qLog(Debug) << "Removable volume added" << id;
    Q_EMIT DeviceAdded(id);
  }
  for (const QString &id : std::as_const(changed_ids)) {
    Q_EMIT DeviceChanged(id);
  }

}

Win32DeviceLister::ReadResult Win32DeviceLister::ReadDeviceData(const QChar drive_letter, DeviceData &device_data) {

  const QString root_path = QString(drive_letter) + ":\\"_L1;

  const UINT drive_type = GetDriveTypeW(ToWChar(root_path));
  if (drive_type != DRIVE_REMOVABLE && drive_type != DRIVE_FIXED) return ReadResult::Unsuitable;

  // Never list the drive Windows is running from.
  wchar_t system_directory[MAX_PATH + 1] = {};
  if (GetSystemWindowsDirectoryW(system_directory, MAX_PATH) > 0 && QChar(system_directory[0]).toUpper() == drive_letter.toUpper()) {
    return ReadResult::Unsuitable;
  }

  bool bus_type_read = false;
  STORAGE_BUS_TYPE bus_type = BusTypeUnknown;
  {
    const ScopedHandle handle(OpenDevice(u"\\\\.\\%1:"_s.arg(drive_letter)));
    if (handle.isValid()) {
      STORAGE_PROPERTY_QUERY query{};
      query.PropertyId = StorageDeviceProperty;
      query.QueryType = PropertyStandardQuery;
      QByteArray buffer(1024, '\0');
      DWORD bytes_returned = 0;
      if (DeviceIoControl(handle.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_returned, nullptr) && bytes_returned >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        buffer.truncate(static_cast<qsizetype>(bytes_returned));
        const STORAGE_DEVICE_DESCRIPTOR *descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.constData());
        bus_type = descriptor->BusType;
        bus_type_read = true;
        device_data.vendor = DescriptorString(buffer, descriptor->VendorIdOffset);
        device_data.product = DescriptorString(buffer, descriptor->ProductIdOffset);
        device_data.serial = DescriptorString(buffer, descriptor->SerialNumberOffset);
      }
    }
  }

  // Fixed drives are only listed when they are connected through a hotplug bus, such as USB hard drives.
  if (drive_type == DRIVE_FIXED && !IsHotplugBus(bus_type)) return bus_type_read ? ReadResult::Unsuitable : ReadResult::UnknownBus;

  // This fails when there is no media in the drive, such as an empty memory card reader, or when the volume is locked by BitLocker.
  wchar_t label[MAX_PATH + 1] = {};
  wchar_t filesystem[MAX_PATH + 1] = {};
  DWORD volume_serial = 0;
  if (!GetVolumeInformationW(ToWChar(root_path), label, MAX_PATH + 1, &volume_serial, nullptr, nullptr, filesystem, MAX_PATH + 1)) {
    return ReadResult::NotReady;
  }

  wchar_t volume_name[MAX_PATH + 1] = {};
  if (GetVolumeNameForVolumeMountPointW(ToWChar(root_path), volume_name, MAX_PATH + 1)) {
    device_data.volume_name = QString::fromWCharArray(volume_name);
  }

  device_data.root_path = QDir::fromNativeSeparators(root_path);
  device_data.label = QString::fromWCharArray(label);
  device_data.filesystem = QString::fromWCharArray(filesystem);
  device_data.volume_serial = volume_serial;
  device_data.removable_drive = drive_type == DRIVE_REMOVABLE;
  device_data.capacity = Utilities::FileSystemCapacity(root_path);
  device_data.free_space = Utilities::FileSystemFreeSpace(root_path);

  // The volume GUID stays the same for a volume when it's reconnected, also when the drive letter changes.
  const qsizetype guid_start = device_data.volume_name.indexOf(u'{');
  const qsizetype guid_end = device_data.volume_name.indexOf(u'}', guid_start);
  if (guid_start != -1 && guid_end != -1) {
    device_data.unique_id = "Win32/"_L1 + device_data.volume_name.mid(guid_start, guid_end - guid_start + 1);
  }
  else {
    device_data.unique_id = u"Win32/%1/%2/%3/%4"_s.arg(device_data.vendor, device_data.product, device_data.serial).arg(device_data.volume_serial, 8, 16, u'0');
  }

  return ReadResult::Ready;

}

QStringList Win32DeviceLister::DeviceUniqueIDs() {

  QReadLocker locker(&device_data_lock_);
  return device_data_.keys();

}

QVariantList Win32DeviceLister::DeviceIcons(const QString &id) {

  DeviceData device_data;
  {
    QReadLocker locker(&device_data_lock_);
    if (!device_data_.contains(id)) return QVariantList();
    device_data = device_data_.value(id);
  }

  return QVariantList() << GuessIconForPath(device_data.root_path) << GuessIconForModel(device_data.vendor, device_data.product) << (device_data.removable_drive ? u"device-usb-flash"_s : u"device-usb-drive"_s);

}

QString Win32DeviceLister::DeviceManufacturer(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  return device_data_.value(id).vendor;

}

QString Win32DeviceLister::DeviceModel(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  return device_data_.value(id).product;

}

quint64 Win32DeviceLister::DeviceCapacity(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  return device_data_.value(id).capacity;

}

quint64 Win32DeviceLister::DeviceFreeSpace(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  return device_data_.value(id).free_space;

}

QVariantMap Win32DeviceLister::DeviceHardwareInfo(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  if (!device_data_.contains(id)) return QVariantMap();

  const DeviceData device_data = device_data_.value(id);

  QVariantMap result;
  result[QStringLiteral(QT_TR_NOOP("Drive"))] = QDir::toNativeSeparators(device_data.root_path);
  result[QStringLiteral(QT_TR_NOOP("Volume"))] = device_data.volume_name;
  result[QStringLiteral(QT_TR_NOOP("Volume label"))] = device_data.label;
  result[QStringLiteral(QT_TR_NOOP("File system"))] = device_data.filesystem;
  result[QStringLiteral(QT_TR_NOOP("Serial number"))] = device_data.serial;

  return result;

}

QString Win32DeviceLister::MakeFriendlyName(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  if (!device_data_.contains(id)) return QString();

  const DeviceData device_data = device_data_.value(id);
  if (!device_data.label.isEmpty()) return device_data.label;

  const QString vendor_product = u"%1 %2"_s.arg(device_data.vendor, device_data.product).trimmed();
  if (!vendor_product.isEmpty()) return vendor_product;

  return tr("Removable disk (%1)").arg(QDir::toNativeSeparators(device_data.root_path));

}

QList<QUrl> Win32DeviceLister::MakeDeviceUrls(const QString &id) {

  QReadLocker locker(&device_data_lock_);
  if (!device_data_.contains(id)) return QList<QUrl>();

  return QList<QUrl>() << MakeUrlFromLocalPath(device_data_.value(id).root_path);

}

void Win32DeviceLister::UpdateDeviceFreeSpace(const QString &id) {

  QString root_path;
  {
    QReadLocker locker(&device_data_lock_);
    if (!device_data_.contains(id)) return;
    root_path = device_data_.value(id).root_path;
  }

  // Query outside the lock, the call can block for a while on a slow or stalled device, and the GUI thread reads the device data.
  quint64 free_space = 0;
  {
    const ScopedCriticalErrorMode critical_error_mode;
    free_space = Utilities::FileSystemFreeSpace(root_path);
  }

  {
    QWriteLocker locker(&device_data_lock_);
    // The device might have been removed or replaced by another volume with the same drive letter in between.
    if (!device_data_.contains(id) || device_data_.value(id).root_path != root_path) return;
    DeviceData &device_data = device_data_[id];
    device_data.free_space = free_space;
    ++device_data.free_space_updates;
  }

  Q_EMIT DeviceChanged(id);

}

void Win32DeviceLister::UnmountDevice(const QString &id) {

  QString volume_name;
  QString root_path;
  {
    QReadLocker locker(&device_data_lock_);
    if (!device_data_.contains(id)) return;
    const DeviceData &device_data = device_data_.value(id);
    volume_name = device_data.volume_name;
    root_path = device_data.root_path;
  }

  if (EjectDrive(volume_name, root_path)) {
    // Remove the device right away instead of waiting for the removal notification.
    ScheduleRescan();
    Rescan();
  }

}

bool Win32DeviceLister::EjectDrive(const QString &volume_name, const QString &root_path) {

  // Open the volume by its GUID path instead of the drive letter, the drive letter might have been reused by another volume since the last rescan.
  if (!volume_name.startsWith("\\\\?\\Volume{"_L1)) {
    qLog(Error) << "Could not eject" << root_path << ", the volume GUID path is unknown.";
    return false;
  }

  const ScopedCriticalErrorMode critical_error_mode;

  STORAGE_DEVICE_NUMBER device_number{};
  {
    // Opening the volume requires the path without the trailing backslash, with it, the root directory of the file system would be opened.
    QString volume_path = volume_name;
    if (volume_path.endsWith(u'\\')) volume_path.chop(1);
    const ScopedHandle handle(OpenDevice(volume_path));
    if (!handle.isValid() || !GetDeviceNumber(handle.get(), device_number)) {
      qLog(Error) << "Could not get the disk number for" << root_path;
      return false;
    }
  }

  const DEVINST disk_devinst = FindDiskDevInst(device_number.DeviceNumber);
  if (disk_devinst == 0) {
    qLog(Error) << "Could not find the disk device for" << root_path;
    return false;
  }

  // Eject the device "Safely Remove Hardware" would eject: the disk itself or its closest ancestor that can be removed, such as the USB device of a flash drive,
  // or the card itself in an SD card reader, instead of the reader's controller.
  DEVINST devinst = FindRemovableDevInst(disk_devinst);
  if (devinst == 0) {
    qLog(Error) << "Could not find a removable device for" << root_path;
    return false;
  }

  for (int attempt = 1; attempt <= kEjectAttempts; ++attempt) {
    PNP_VETO_TYPE veto_type = PNP_VetoTypeUnknown;
    wchar_t veto_name[MAX_PATH] = {};
    const CONFIGRET result = CM_Request_Device_EjectW(devinst, &veto_type, veto_name, MAX_PATH, 0);
    if (result == CR_SUCCESS && veto_type == PNP_VetoTypeUnknown) {
      qLog(Debug) << "Ejected" << root_path;
      return true;
    }
    qLog(Warning) << "Failed to eject" << root_path << "attempt" << attempt << "result" << result << "veto type" << veto_type << QString::fromWCharArray(veto_name);
    if (attempt < kEjectAttempts) Sleep(500);
  }

  qLog(Error) << "Could not eject" << root_path << ", the device is in use.";

  return false;

}
