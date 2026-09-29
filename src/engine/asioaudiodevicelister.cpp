/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <winreg.h>

#include <vector>

#include <QString>
#include <QScopeGuard>

#include "asioaudiodevicelister.h"
#include "enginedevice.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Reads a REG_SZ value of any length, returns a null QString if the value is missing, has another type or can't be read.
QString ReadRegistryString(HKEY key, LPCWSTR value_name) {

  // The value can change between querying the size and reading it, so retry if it grew in the meantime.
  for (int attempt = 0; attempt < 3; ++attempt) {
    DWORD data_size = 0;
    LSTATUS status = RegGetValueW(key, nullptr, value_name, RRF_RT_REG_SZ, nullptr, nullptr, &data_size);
    if (status != ERROR_SUCCESS || data_size == 0) {
      return QString();
    }

    // The returned size is in bytes and includes the terminator, add room for one more in case the stored value lacks it.
    std::vector<WCHAR> data((data_size / sizeof(WCHAR)) + 1, 0);
    data_size = static_cast<DWORD>(data.size() * sizeof(WCHAR));
    status = RegGetValueW(key, nullptr, value_name, RRF_RT_REG_SZ, nullptr, data.data(), &data_size);
    if (status == ERROR_MORE_DATA) {
      continue;
    }
    if (status != ERROR_SUCCESS) {
      return QString();
    }

    // RegGetValueW guarantees null-termination, so this stops at the terminator.
    return QString::fromWCharArray(data.data());
  }

  return QString();

}

}  // namespace

AsioAudioDeviceLister::AsioAudioDeviceLister() : AudioDeviceLister(u"asio"_s, { u"asiosink"_s }) {}

EngineDeviceList AsioAudioDeviceLister::ListDevices() {

  EngineDeviceList devices;

  HKEY reg_key = nullptr;
  LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"software\\asio", 0, KEY_READ, &reg_key);

  for (DWORD i = 0; status == ERROR_SUCCESS; i++) {
    WCHAR key_name[256];
    // RegEnumKeyW expects the buffer size in characters, not bytes.
    status = RegEnumKeyW(reg_key, i, key_name, sizeof(key_name) / sizeof(key_name[0]));
    if (status != ERROR_SUCCESS) break;  // Don't call GetDevice with a stale key_name on ERROR_NO_MORE_ITEMS / errors.
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

EngineDevice AsioAudioDeviceLister::GetDevice(HKEY reg_key, LPWSTR key_name) {

  HKEY sub_key = nullptr;
  const QScopeGuard scopeguard_sub_key = qScopeGuard([&sub_key]() {
    if (sub_key) {
      RegCloseKey(sub_key);
    }
  });

  LSTATUS status = RegOpenKeyExW(reg_key, key_name, 0, KEY_READ, &sub_key);
  if (status != ERROR_SUCCESS) {
    return EngineDevice();
  }

  const QString clsid = ReadRegistryString(sub_key, L"clsid");
  if (clsid.isEmpty()) {
    return EngineDevice();
  }

  EngineDevice device;
  device.value = clsid;
  device.description = QString::fromWCharArray(key_name);

  const QString description = ReadRegistryString(sub_key, L"description");
  if (!description.isEmpty()) {
    device.description = description;
  }

  return device;

}
