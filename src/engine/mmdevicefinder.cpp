/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <initguid.h>
#include <devpkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

#include <QList>
#include <QVariant>
#include <QString>

#include "mmdevicefinder.h"
#include "core/logging.h"

MMDeviceFinder::MMDeviceFinder() : DeviceFinder("mmdevice", { "wasapisink" }) {}

QList<DeviceFinder::Device> MMDeviceFinder::ListDevices() {

  HRESULT hr = S_OK;

  IMMDeviceEnumerator *enumerator = nullptr;
  hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
  if (FAILED(hr)) {
    return QList<Device>();
  }

  IMMDeviceCollection *collection = nullptr;
  hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  if (FAILED(hr)) {
    enumerator->Release();
    return QList<Device>();
  }

  UINT count;
  hr = collection->GetCount(&count);
  if (FAILED(hr)) {
    collection->Release();
    enumerator->Release();
    return QList<Device>();
  }

  QList<Device> devices;
  Device default_device;
  default_device.description = "Default device";
  default_device.iconname = GuessIconName(default_device.description);
  devices.append(default_device);

  for (ULONG i = 0 ; i < count ; i++) {

    IMMDevice *endpoint = nullptr;
    hr = collection->Item(i, &endpoint);
    if (FAILED(hr)) { return devices; }

    LPWSTR pwszid = nullptr;
    hr = endpoint->GetId(&pwszid);
    if (FAILED(hr)) {
      endpoint->Release();
      continue;
    }

    IPropertyStore *props = nullptr;
    hr = endpoint->OpenPropertyStore(STGM_READ, &props);
    if (FAILED(hr)) {
      CoTaskMemFree(pwszid);
      endpoint->Release();
      continue;
    }

    PROPVARIANT var_name;
    PropVariantInit(&var_name);
    hr = props->GetValue(PKEY_Device_FriendlyName, &var_name);
    if (FAILED(hr)) {
      props->Release();
      CoTaskMemFree(pwszid);
      endpoint->Release();
      continue;
    }

    Device device;
    device.description = QString::fromWCharArray(var_name.pwszVal);
    device.iconname = GuessIconName(device.description);
    device.value = QString::fromStdWString(pwszid);
    devices.append(device);

    PropVariantClear(&var_name);
    props->Release();
    CoTaskMemFree(pwszid);
    endpoint->Release();

  }
  collection->Release();
  enumerator->Release();

  return devices;

}
