/*
 * Strawberry Music Player
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <initguid.h>
#include <devpkey.h>
#ifdef _MSC_VER
#  include <functiondiscoverykeys.h>
#else
#  include <functiondiscoverykeys_devpkey.h>
#endif
#include <mmdeviceapi.h>

#include <QVariant>
#include <QString>
#include <QScopeGuard>

#include "mmaudiodevicelister.h"
#include "enginedevice.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

#ifdef _MSC_VER
  DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
  DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
#endif

namespace {

EngineDevice GetDevice(IMMDevice *endpoint) {

  LPWSTR pwszid = nullptr;
  HRESULT hr = endpoint->GetId(&pwszid);
  if (FAILED(hr)) {
    qLog(Error) << "IMMDevice::GetId failed." << Qt::hex << DWORD(hr);
    return EngineDevice();
  }
  const QScopeGuard scopeguard_pwszid = qScopeGuard([pwszid]() { CoTaskMemFree(pwszid); });

  IPropertyStore *props = nullptr;
  hr = endpoint->OpenPropertyStore(STGM_READ, &props);
  if (FAILED(hr)) {
    qLog(Error) << "IPropertyStore::OpenPropertyStore failed." << Qt::hex << DWORD(hr);
    return EngineDevice();
  }
  const QScopeGuard scopeguard_props = qScopeGuard([props]() { props->Release(); });

  PROPVARIANT var_name;
  PropVariantInit(&var_name);
  // Always clear - safe on an inited-but-empty variant, and avoids leaking on the error path.
  const QScopeGuard scopeguard_var_name = qScopeGuard([&var_name]() { PropVariantClear(&var_name); });
  hr = props->GetValue(PKEY_Device_FriendlyName, &var_name);
  if (FAILED(hr)) {
    qLog(Error) << "IPropertyStore::GetValue failed." << Qt::hex << DWORD(hr);
    return EngineDevice();
  }

  EngineDevice device;
  device.value = QString::fromWCharArray(pwszid);
  // The friendly name can be missing (VT_EMPTY), in which case pwszVal is not a valid string, fall back to the endpoint ID if it's missing or empty.
  if (var_name.vt == VT_LPWSTR && var_name.pwszVal) {
    device.description = QString::fromWCharArray(var_name.pwszVal);
  }
  if (device.description.isEmpty()) {
    device.description = device.value.toString();
  }
  device.iconname = device.GuessIconName();

  return device;

}

}  // namespace

MMAudioDeviceLister::MMAudioDeviceLister() : AudioDeviceLister(u"mmdevice"_s, { u"wasapisink"_s, u"wasapi2sink"_s }) {}

EngineDeviceList MMAudioDeviceLister::ListDevices() {

  const HRESULT hr_coinit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const QScopeGuard scopeguard_coinit = qScopeGuard([hr_coinit]() {
    if (SUCCEEDED(hr_coinit)) {
      CoUninitialize();
    }
  });

  EngineDeviceList devices;
  EngineDevice default_device;
  default_device.description = QLatin1String("Default device");
  default_device.iconname = default_device.GuessIconName();
  devices.append(default_device);

  IMMDeviceEnumerator *enumerator = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&enumerator));
  if (FAILED(hr)) {
    qLog(Error) << "CoCreateInstance failed." << Qt::hex << DWORD(hr);
    return devices;
  }
  const QScopeGuard scopeguard_enumerator = qScopeGuard([enumerator]() { enumerator->Release(); });

  IMMDeviceCollection *collection = nullptr;
  hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  if (FAILED(hr)) {
    qLog(Error) << "EnumAudioEndpoints failed." << Qt::hex << DWORD(hr);
    return devices;
  }
  const QScopeGuard scopeguard_collection = qScopeGuard([collection]() { collection->Release(); });

  UINT count = 0;
  hr = collection->GetCount(&count);
  if (FAILED(hr)) {
    qLog(Error) << "IMMDeviceCollection::GetCount failed." << Qt::hex << DWORD(hr);
    return devices;
  }

  for (UINT i = 0; i < count; ++i) {
    IMMDevice *endpoint = nullptr;
    hr = collection->Item(i, &endpoint);
    if (FAILED(hr)) {
      qLog(Error) << "IMMDeviceCollection::Item failed." << Qt::hex << DWORD(hr);
      continue;
    }
    const QScopeGuard scopeguard_endpoint = qScopeGuard([endpoint]() { endpoint->Release(); });
    const EngineDevice device = GetDevice(endpoint);
    if (device.value.isValid()) {
      devices.append(device);
    }
  }

  return devices;

}
