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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#  include <gio/gunixmounts.h>
#endif

#include <QtGlobal>
#include <QMutex>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "core/logging.h"
#include "core/signalchecker.h"
#include "devicelister.h"
#include "giolister.h"

using namespace Qt::Literals::StringLiterals;

QString GioLister::DeviceInfo::unique_id() const {

  if (!volume_root_uri.isEmpty()) return volume_root_uri;

  if (mount_ptr) {
    return QStringLiteral("Gio/%1/%2/%3").arg(mount_uuid, filesystem_type).arg(filesystem_size);
  }

  return QStringLiteral("Gio/unmounted/%1").arg(reinterpret_cast<qulonglong>(volume_ptr.get()));

}

bool GioLister::DeviceInfo::is_suitable() const {

  if (!volume_ptr) return false;  // This excludes smb or ssh mounts

  if (is_system_internal) return false;

  if (drive_ptr && !drive_removable) return false;  // This excludes internal drives

  if (filesystem_type.isEmpty()) return true;

  return filesystem_type != "udf"_L1 && filesystem_type != "smb"_L1 && filesystem_type != "cifs"_L1 && filesystem_type != "ssh"_L1 && filesystem_type != "isofs"_L1;

}

template<typename T, typename F>
void OperationFinished(F f, GObject *object, GAsyncResult *result) {

  T *obj = reinterpret_cast<T*>(object);
  GError *error = nullptr;

  f(obj, result, &error);

  if (error) {
    qLog(Error) << "Mount/unmount error:" << error->message;
    g_error_free(error);
  }

}

void GioLister::VolumeMountFinished(GObject *object, GAsyncResult *result, gpointer instance) {
  Q_UNUSED(instance)
  OperationFinished<GVolume>(std::bind(g_volume_mount_finish, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), object, result);
}

bool GioLister::Init() {

  monitor_.reset_without_add(g_volume_monitor_get());

  // Get existing volumes
  GList *const volumes = g_volume_monitor_get_volumes(monitor_);
  for (GList *p = volumes; p; p = p->next) {
    GVolume *volume = static_cast<GVolume*>(p->data);

    VolumeAdded(volume);
    g_object_unref(volume);
  }
  g_list_free(volumes);

  // Get existing mounts
  GList *const mounts = g_volume_monitor_get_mounts(monitor_);
  for (GList *p = mounts; p; p = p->next) {
    GMount *mount = static_cast<GMount*>(p->data);

    MountAdded(mount);
    g_object_unref(mount);
  }
  g_list_free(mounts);

  // Connect signals from the monitor
  signals_.append(CHECKED_GCONNECT(monitor_, "volume-added", &VolumeAddedCallback, this));
  signals_.append(CHECKED_GCONNECT(monitor_, "volume-removed", &VolumeRemovedCallback, this));
  signals_.append(CHECKED_GCONNECT(monitor_, "mount-added", &MountAddedCallback, this));
  signals_.append(CHECKED_GCONNECT(monitor_, "mount-changed", &MountChangedCallback, this));
  signals_.append(CHECKED_GCONNECT(monitor_, "mount-removed", &MountRemovedCallback, this));

  return true;

}

GioLister::~GioLister() {
  for (gulong signal : std::as_const(signals_)) {
    g_signal_handler_disconnect(monitor_, signal);
  }
}

QStringList GioLister::DeviceUniqueIDs() {
  QMutexLocker l(&mutex_);
  return devices_.keys();
}

QVariantList GioLister::DeviceIcons(const QString &id) {

  QVariantList ret;
  QMutexLocker l(&mutex_);
  if (!devices_.contains(id)) return ret;

  const DeviceInfo device_info = devices_.value(id);

  if (device_info.mount_ptr) {
    ret << DeviceLister::GuessIconForPath(device_info.mount_path);
    ret << device_info.mount_icon_names;
  }

  ret << DeviceLister::GuessIconForModel(QString(), device_info.mount_name);

  return ret;

}

QString GioLister::DeviceManufacturer(const QString &id) { Q_UNUSED(id); return QString(); }

QString GioLister::DeviceModel(const QString &id) {

  QMutexLocker l(&mutex_);
  if (!devices_.contains(id)) return QString();
  const DeviceInfo device_info = devices_.value(id);

  return device_info.drive_name.isEmpty() ? device_info.volume_name : device_info.drive_name;

}

quint64 GioLister::DeviceCapacity(const QString &id) {
  return LockAndGetDeviceInfo(id, &DeviceInfo::filesystem_size);
}

quint64 GioLister::DeviceFreeSpace(const QString &id) {
  return LockAndGetDeviceInfo(id, &DeviceInfo::filesystem_free);
}

QString GioLister::MakeFriendlyName(const QString &id) {
  return DeviceModel(id);
}

QVariantMap GioLister::DeviceHardwareInfo(const QString &id) {

  QVariantMap ret;

  QMutexLocker l(&mutex_);
  if (!devices_.contains(id)) return ret;
  const DeviceInfo info = devices_.value(id);

  ret[QStringLiteral(QT_TR_NOOP("Mount point"))] = info.mount_path;
  ret[QStringLiteral(QT_TR_NOOP("Device"))] = info.volume_unix_device;
  ret[QStringLiteral(QT_TR_NOOP("URI"))] = info.mount_uri;
  return ret;

}

QList<QUrl> GioLister::MakeDeviceUrls(const QString &id) {

  QString volume_root_uri;
  QString mount_point;
  QString mount_uri;
  QString unix_device;

  {
    QMutexLocker l(&mutex_);
    volume_root_uri = devices_[id].volume_root_uri;
    mount_point = devices_[id].mount_path;
    mount_uri = devices_[id].mount_uri;
    unix_device = devices_[id].volume_unix_device;
  }

  QStringList uris;
  if (!volume_root_uri.isEmpty()) {
    uris << volume_root_uri;
  }

  if (!mount_uri.isEmpty()) {
    uris << mount_uri;
  }

  QList<QUrl> ret;

  for (QString uri : std::as_const(uris)) {

    // gphoto2 gives invalid hostnames with []:, characters in
    static const QRegularExpression regex_url_usb(u"//\\[usb:(\\d+),(\\d+)\\]"_s);
    uri.replace(regex_url_usb, u"//usb-\\1-\\2"_s);

    QUrl url;

    static const QRegularExpression regex_url_schema(u"..+:.*"_s);
    if (uri.contains(regex_url_schema)) {
      url = QUrl::fromEncoded(uri.toUtf8());
    }
    else {
      url = MakeUrlFromLocalPath(uri);
    }

    if (url.isValid()) {

      // Special case for file:// GIO URIs - we have to check whether they point to an ipod.
      if (url.isLocalFile() && IsIpod(url.path())) {
        url.setScheme(u"ipod"_s);
      }

      static const QRegularExpression regex_usb_digit(u"usb/(\\d+)/(\\d+)"_s);
      QRegularExpression device_re(regex_usb_digit);
      QRegularExpressionMatch re_match = device_re.match(unix_device);
      if (re_match.hasMatch()) {
        QUrlQuery url_query(url);
        url_query.addQueryItem(u"busnum"_s, re_match.captured(1));
        url_query.addQueryItem(u"devnum"_s, re_match.captured(2));
        url.setQuery(url_query);
      }

      ret << url;  // clazy:exclude=reserve-candidates

    }
  }

  if (!mount_point.isEmpty()) {
    QUrl url = MakeUrlFromLocalPath(mount_point);
    if (url.isValid()) ret << url;
  }

  return ret;

}

void GioLister::VolumeAddedCallback(GVolumeMonitor *volume_monitor, GVolume *volume, gpointer instance) {
  Q_UNUSED(volume_monitor)
  static_cast<GioLister*>(instance)->VolumeAdded(volume);
}

void GioLister::VolumeRemovedCallback(GVolumeMonitor *volume_monitor, GVolume *volume, gpointer instance) {
  Q_UNUSED(volume_monitor)
  static_cast<GioLister*>(instance)->VolumeRemoved(volume);
}

void GioLister::MountAddedCallback(GVolumeMonitor *volume_monitor, GMount *mount, gpointer instance) {
  Q_UNUSED(volume_monitor)
  static_cast<GioLister*>(instance)->MountAdded(mount);
}

void GioLister::MountChangedCallback(GVolumeMonitor *volume_monitor, GMount *mount, gpointer instance) {
  Q_UNUSED(volume_monitor)
  static_cast<GioLister*>(instance)->MountChanged(mount);
}

void GioLister::MountRemovedCallback(GVolumeMonitor *volume_monitor, GMount *mount, gpointer instance) {
  Q_UNUSED(volume_monitor)
  static_cast<GioLister*>(instance)->MountRemoved(mount);
}

void GioLister::VolumeAdded(GVolume *volume) {

  g_object_ref(volume);

  DeviceInfo info;
  info.ReadVolumeInfo(volume);
  if (info.volume_root_uri.startsWith("afc://"_L1) || info.volume_root_uri.startsWith("gphoto2://"_L1)) {
    // Handled by iLister.
    return;
  }
#ifdef HAVE_AUDIOCD
  if (info.volume_root_uri.startsWith("cdda"_L1)) {
    // Audio CD devices are already handled by CDDA lister
    return;
  }
#endif
  info.ReadDriveInfo(g_volume_get_drive(volume));
  info.ReadMountInfo(g_volume_get_mount(volume));
  if (!info.is_suitable()) return;

  {
    QMutexLocker l(&mutex_);
    devices_[info.unique_id()] = info;
  }

  Q_EMIT DeviceAdded(info.unique_id());

}

void GioLister::VolumeRemoved(GVolume *volume) {

  QString id;

  {
    QMutexLocker l(&mutex_);
    id = FindUniqueIdByVolume(volume);
    if (id.isNull()) return;

    devices_.remove(id);
  }

  Q_EMIT DeviceRemoved(id);
}

void GioLister::MountAdded(GMount *mount) {

  g_object_ref(mount);

  DeviceInfo info;
  info.ReadVolumeInfo(g_mount_get_volume(mount));
  if (info.volume_root_uri.startsWith("afc://"_L1) || info.volume_root_uri.startsWith("gphoto2://"_L1)) {
    // Handled by iLister.
    return;
  }
#ifdef HAVE_AUDIOCD
  if (info.volume_root_uri.startsWith("cdda"_L1)) {
    // Audio CD devices are already handled by CDDA lister
    return;
  }
#endif
  info.ReadMountInfo(mount);
  info.ReadDriveInfo(g_mount_get_drive(mount));
  if (!info.is_suitable()) return;

  QString old_id;
  {
    QMutexLocker l(&mutex_);

    // The volume might already exist - either mounted or unmounted.
    const QStringList ids = devices_.keys();
    for (const QString &id : ids) {
      if (devices_[id].volume_ptr == info.volume_ptr) {
        old_id = id;
        break;
      }
    }

    if (!old_id.isEmpty() && old_id != info.unique_id()) {
      // If the ID has changed (for example, after it's been mounted), we need
      // to remove the old device.
      devices_.remove(old_id);
      Q_EMIT DeviceRemoved(old_id);

      old_id = QString();
    }
    devices_[info.unique_id()] = info;
  }

  if (old_id.isEmpty()) {
    Q_EMIT DeviceAdded(info.unique_id());
  }
  else {
    Q_EMIT DeviceChanged(old_id);
  }

}

void GioLister::MountChanged(GMount *mount) {

  QString id;
  {
    QMutexLocker l(&mutex_);
    id = FindUniqueIdByMount(mount);
    if (id.isNull()) return;

    g_object_ref(mount);

    DeviceInfo new_info;
    new_info.ReadMountInfo(mount);
    new_info.ReadVolumeInfo(g_mount_get_volume(mount));
    new_info.ReadDriveInfo(g_mount_get_drive(mount));

    // Ignore the change if the new info is useless
    if (new_info.invalid_enclosing_mount || (devices_.value(id).filesystem_size != 0 && new_info.filesystem_size == 0) || (!devices_[id].filesystem_type.isEmpty() && new_info.filesystem_type.isEmpty())) {
      return;
    }

    devices_[id] = new_info;
  }

  Q_EMIT DeviceChanged(id);

}

void GioLister::MountRemoved(GMount *mount) {

  QString id;
  {
    QMutexLocker l(&mutex_);
    id = FindUniqueIdByMount(mount);
    if (id.isNull()) return;

    devices_.remove(id);
  }

  Q_EMIT DeviceRemoved(id);

}

QString GioLister::DeviceInfo::ConvertAndFree(char *str) {

  QString ret = QString::fromUtf8(str);
  g_free(str);
  return ret;

}

void GioLister::DeviceInfo::ReadMountInfo(GMount *mount) {

  // Get basic information
  mount_ptr.reset_without_add(mount);
  if (!mount) return;

  mount_name = ConvertAndFree(g_mount_get_name(mount));

  // Get the icon name(s)
  mount_icon_names.clear();
  GIcon *icon = g_mount_get_icon(mount);
  if (G_IS_THEMED_ICON(icon)) {
    const char *const *icons = g_themed_icon_get_names(G_THEMED_ICON(icon));
    for (const char *const *p = icons; *p; ++p) {
      mount_icon_names << QString::fromUtf8(*p);
    }
  }
  g_object_unref(icon);

  ScopedGObject<GFile> root;
  root.reset_without_add(g_mount_get_root(mount));

  // Get the mount path
  mount_path = ConvertAndFree(g_file_get_path(root));
  mount_uri = ConvertAndFree(g_file_get_uri(root));

  // Do a sanity check to make sure the root is actually this mount
  // when a device is unmounted GIO sends a changed signal before the removed signal,
  // and we end up reading information about the / filesystem by mistake.
  GError *error = nullptr;
  GMount *actual_mount = g_file_find_enclosing_mount(root, nullptr, &error);
  if (error || !actual_mount) {
    g_error_free(error);
    invalid_enclosing_mount = true;
  }
  else if (actual_mount) {
    g_object_unref(actual_mount);
  }

#ifdef HAVE_GIO_UNIX
#ifdef GLIB_VERSION_2_84
  GUnixMountEntry *unix_mount = g_unix_mount_entry_for(g_file_get_path(root), nullptr);
#else
  GUnixMountEntry *unix_mount = g_unix_mount_for(g_file_get_path(root), nullptr);
#endif
  if (unix_mount) {
    // The GIO's definition of system internal mounts include filesystems like autofs, tmpfs, sysfs, etc,
    // and various system directories, including the root, /boot, /var, /home, etc.
#ifdef GLIB_VERSION_2_84
    is_system_internal = g_unix_mount_entry_is_system_internal(unix_mount);
    g_unix_mount_entry_free(unix_mount);
#else
    is_system_internal = g_unix_mount_is_system_internal(unix_mount);
    g_unix_mount_free(unix_mount);
#endif
    // Although checking most of the internal mounts is safe, we really don't want to touch autofs filesystems, as that would trigger automounting.
    if (is_system_internal) return;
  }
#endif

  // Query the filesystem info for size, free space, and type
  error = nullptr;
  GFileInfo *info = g_file_query_filesystem_info(root, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE "," G_FILE_ATTRIBUTE_FILESYSTEM_FREE "," G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, nullptr, &error);
  if (error) {
    qLog(Warning) << error->message;
    g_error_free(error);
  }
  else {
    filesystem_size = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
    filesystem_free = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
    filesystem_type = QString::fromUtf8(g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
    g_object_unref(info);
  }

  // Query the file's info for a filesystem ID
  // Only afc devices (that I know of) give reliably unique IDs
  if (filesystem_type == "afc"_L1) {
    error = nullptr;
    info = g_file_query_info(root, G_FILE_ATTRIBUTE_ID_FILESYSTEM, G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (error) {
      qLog(Warning) << error->message;
      g_error_free(error);
    }
    else {
      mount_uuid = QString::fromUtf8(g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILESYSTEM));
      g_object_unref(info);
    }
  }
}

void GioLister::DeviceInfo::ReadVolumeInfo(GVolume *volume) {

  volume_ptr.reset_without_add(volume);
  if (!volume) return;

  volume_name = ConvertAndFree(g_volume_get_name(volume));
  volume_uuid = ConvertAndFree(g_volume_get_uuid(volume));
  volume_unix_device = ConvertAndFree(g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE));

  GFile *root = g_volume_get_activation_root(volume);
  if (root) {
    volume_root_uri = QString::fromUtf8(g_file_get_uri(root));
    g_object_unref(root);
  }

}

void GioLister::DeviceInfo::ReadDriveInfo(GDrive *drive) {

  drive_ptr.reset_without_add(drive);
  if (!drive) return;

  drive_name = ConvertAndFree(g_drive_get_name(drive));
  drive_removable = g_drive_is_media_removable(drive);
}

QString GioLister::FindUniqueIdByMount(GMount *mount) const {

  for (const DeviceInfo &info : devices_) {
    if (info.mount_ptr == mount) return info.unique_id();
  }
  return QString();

}

QString GioLister::FindUniqueIdByVolume(GVolume *volume) const {

  for (const DeviceInfo &info : devices_) {
    if (info.volume_ptr == volume) return info.unique_id();
  }
  return QString();

}

void GioLister::VolumeEjectFinished(GObject *object, GAsyncResult *result, gpointer instance) {
  Q_UNUSED(instance)
  OperationFinished<GVolume>(std::bind(g_volume_eject_with_operation_finish, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), object, result);
}

void GioLister::MountEjectFinished(GObject *object, GAsyncResult *result, gpointer instance) {
  Q_UNUSED(instance)
  OperationFinished<GMount>(std::bind(g_mount_eject_with_operation_finish, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), object, result);
}

void GioLister::MountUnmountFinished(GObject *object, GAsyncResult *result, gpointer instance) {
  Q_UNUSED(instance)
  OperationFinished<GMount>(std::bind(g_mount_unmount_with_operation_finish, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), object, result);
}

void GioLister::UpdateDeviceFreeSpace(const QString &id) {

  {
    QMutexLocker l(&mutex_);
    if (!devices_.contains(id) || !devices_[id].mount_ptr || devices_.value(id).volume_root_uri.startsWith("mtp://"_L1)) return;

    GFile *root = g_mount_get_root(devices_.value(id).mount_ptr);

    GError *error = nullptr;
    GFileInfo *info = g_file_query_filesystem_info(root, G_FILE_ATTRIBUTE_FILESYSTEM_FREE, nullptr, &error);
    if (error) {
      qLog(Warning) << error->message;
      g_error_free(error);
    }
    else {
      devices_[id].filesystem_free = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
      g_object_unref(info);
    }

    g_object_unref(root);
  }

  Q_EMIT DeviceChanged(id);

}

bool GioLister::DeviceNeedsMount(const QString &id) {

  QMutexLocker l(&mutex_);
  return devices_.contains(id) && !devices_[id].mount_ptr && !devices_[id].volume_root_uri.startsWith("mtp://"_L1) && !devices_[id].volume_root_uri.startsWith("gphoto2://"_L1);

}

void GioLister::MountDevice(const QString &id, const int request_id) {

  QMutexLocker l(&mutex_);
  if (!devices_.contains(id)) {
    Q_EMIT DeviceMounted(id, request_id, false);
    return;
  }

  const DeviceInfo device_info = devices_.value(id);
  if (device_info.mount_ptr) {
    // Already mounted
    Q_EMIT DeviceMounted(id, request_id, true);
    return;
  }

  g_volume_mount(device_info.volume_ptr, G_MOUNT_MOUNT_NONE, nullptr, nullptr, VolumeMountFinished, nullptr);
  Q_EMIT DeviceMounted(id, request_id, true);

}

void GioLister::UnmountDevice(const QString &id) {

  QMutexLocker l(&mutex_);
  if (!devices_.contains(id) || !devices_[id].mount_ptr || devices_.value(id).volume_root_uri.startsWith("mtp://"_L1)) return;

  const DeviceInfo device_info = devices_.value(id);

  if (!device_info.mount_ptr) return;

  if (device_info.volume_ptr) {
    if (g_volume_can_eject(device_info.volume_ptr)) {
      g_volume_eject_with_operation(device_info.volume_ptr, G_MOUNT_UNMOUNT_NONE, nullptr, nullptr, reinterpret_cast<GAsyncReadyCallback>(VolumeEjectFinished), nullptr);
      return;
    }
  }
  else return;

  if (g_mount_can_eject(device_info.mount_ptr)) {
    g_mount_eject_with_operation(device_info.mount_ptr, G_MOUNT_UNMOUNT_NONE, nullptr, nullptr, reinterpret_cast<GAsyncReadyCallback>(MountEjectFinished), nullptr);
  }
  else if (g_mount_can_unmount(device_info.mount_ptr)) {
    g_mount_unmount_with_operation(device_info.mount_ptr, G_MOUNT_UNMOUNT_NONE, nullptr, nullptr, reinterpret_cast<GAsyncReadyCallback>(MountUnmountFinished), nullptr);
  }

}
