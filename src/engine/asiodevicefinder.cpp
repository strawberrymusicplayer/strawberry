/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <windows.h>
#include <string.h>
#include <atlconv.h>
#include <winreg.h>

#include <QString>

#include "asiodevicefinder.h"
#include "enginedevice.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

AsioDeviceFinder::AsioDeviceFinder() : DeviceFinder(u"asio"_s, { u"asiosink"_s }) {}

EngineDeviceList AsioDeviceFinder::ListDevices() {

  EngineDeviceList devices;

  HKEY reg_key = nullptr;
  LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"software\\asio", 0, KEY_READ, &reg_key);

  for (DWORD i = 0; status == ERROR_SUCCESS; i++) {
    WCHAR key_name[256];
    status = RegEnumKeyW(reg_key, i, key_name, sizeof(key_name));
    EngineDevice device = GetDevice(reg_key, key_name);
    if (device.value.isValid()) {
      devices.append(device);
    }
  }

  if (reg_key) {
    RegCloseKey(reg_key);
  }

  return devices;

}

EngineDevice AsioDeviceFinder::GetDevice(HKEY reg_key, LPWSTR key_name) {

  HKEY sub_key = nullptr;
  const QScopeGuard scopeguard_sub_key = qScopeGuard([sub_key]() {
    if (sub_key) {
      RegCloseKey(sub_key);
    }
  });

  LSTATUS status = RegOpenKeyExW(reg_key, key_name, 0, KEY_READ, &sub_key);
  if (status != ERROR_SUCCESS) {
    return EngineDevice();
  }

  DWORD type = REG_SZ;
  WCHAR clsid_data[256]{};
  DWORD clsid_data_size = sizeof(clsid_data);
  status = RegQueryValueExW(sub_key, L"clsid", 0, &type, (LPBYTE)clsid_data, &clsid_data_size);
  if (status != ERROR_SUCCESS) {
    return EngineDevice();
  }

  EngineDevice device;
  device.value = QString::fromStdWString(clsid_data);
  device.description = QString::fromStdWString(key_name);

  WCHAR desc_data[256]{};
  DWORD desc_data_size = sizeof(desc_data);
  status = RegQueryValueExW(sub_key, L"description", 0, &type, (LPBYTE)desc_data, &desc_data_size);
  if (status == ERROR_SUCCESS) {
    device.description = QString::fromStdWString(desc_data);
  }

  return device;

}
