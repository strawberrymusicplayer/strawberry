/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "mmdevicefinder.h"
#include "enginedevice.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

#ifdef _MSC_VER
  DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
  DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
#endif

MMDeviceFinder::MMDeviceFinder() : DeviceFinder(u"mmdevice"_s, { u"wasapisink"_s }) {}

EngineDeviceList MMDeviceFinder::ListDevices() {

  HRESULT hr_coinit = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  EngineDeviceList devices;
  EngineDevice default_device;
  default_device.description = QLatin1String("Default device");
  default_device.iconname = default_device.GuessIconName();
  devices.append(default_device);

  IMMDeviceEnumerator *enumerator = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&enumerator));
  if (hr == S_OK) {
    IMMDeviceCollection *collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (hr == S_OK) {
      UINT count;
      hr = collection->GetCount(&count);
      if (hr == S_OK) {
        for (ULONG i = 0; i < count; i++) {
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
                  EngineDevice device;
                  device.description = QString::fromWCharArray(var_name.pwszVal);
                  device.iconname = device.GuessIconName();
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
