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

#include <windows.h>
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

  HRESULT hr_coinit = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  QList<Device> devices;
  Device default_device;
  default_device.description = "Default device";
  default_device.iconname = GuessIconName(default_device.description);
  devices.append(default_device);

  IMMDeviceEnumerator *enumerator = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
  if (hr == S_OK) {
    IMMDeviceCollection *collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (hr == S_OK) {
      UINT count;
      hr = collection->GetCount(&count);
      if (hr == S_OK) {
        for (ULONG i = 0 ; i < count ; i++) {
          IMMDevice *endpoint = nullptr;
          hr = collection->Item(i, &endpoint);
          if (hr == S_OK) {
            LPWSTR pwszid = nullptr;
            hr = endpoint->GetId(&pwszid);
            if (hr == S_OK) {
              IPropertyStore *props = nullptr;
              hr = endpoint->OpenPropertyStore(STGM_READ, &props);
              if (hr == S_OK) {
                PROPVARIANT var_name;
                PropVariantInit(&var_name);
                hr = props->GetValue(PKEY_Device_FriendlyName, &var_name);
                if (hr == S_OK) {
                  Device device;
                  device.description = QString::fromWCharArray(var_name.pwszVal);
                  device.iconname = GuessIconName(device.description);
                  device.value = QString::fromStdWString(pwszid);
                  devices.append(device);
                  PropVariantClear(&var_name);
                }
                else {
                  qLog(Error) << "IPropertyStore::GetValue failed." << Qt::hex << DWORD(hr);
                }
                props->Release();
              }
              else {
                qLog(Error) << "IPropertyStore::OpenPropertyStore failed." << Qt::hex << DWORD(hr);
              }
              CoTaskMemFree(pwszid);
            }
            else {
              qLog(Error) << "IMMDevice::GetId failed." << Qt::hex << DWORD(hr);
            }
            endpoint->Release();
          }
          else {
            qLog(Error) << "IMMDeviceCollection::Item failed." << Qt::hex << DWORD(hr);
          }
        }
      }
      else {
        qLog(Error) << "IMMDeviceCollection::GetCount failed." << Qt::hex << DWORD(hr);
      }
      collection->Release();
    }
    else {
      qLog(Error) << "EnumAudioEndpoints failed." << Qt::hex << DWORD(hr);
    }
    enumerator->Release();
  }
  else {
    qLog(Error) << "CoCreateInstance failed." << Qt::hex << DWORD(hr);
  }

  if (hr_coinit == S_OK || hr_coinit == S_FALSE) CoUninitialize();

  return devices;

}
