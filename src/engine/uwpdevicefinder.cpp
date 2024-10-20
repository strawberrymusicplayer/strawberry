/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <utility>
#include <functional>
#include <string>
#include <locale>
#include <codecvt>

#include <QString>

#include <wrl.h>
#include <windows.foundation.h>
#include <windows.media.audio.h>

#include "AsyncOperations.h"

#include "uwpdevicefinder.h"
#include "enginedevice.h"
#include "core/logging.h"

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices::Enumeration;

using namespace Qt::Literals::StringLiterals;

UWPDeviceFinder::UWPDeviceFinder() : DeviceFinder(u"uwpdevice"_s, { u"wasapi2sink"_s }) {}

namespace {

static std::string wstring_to_stdstring(const std::wstring &wstr) {

  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

  return converter.to_bytes(wstr.c_str());

}

static std::string hstring_to_stdstring(HString *hstr) {

  if (!hstr) {
    return std::string();
  }

  const wchar_t *raw_hstr = hstr->GetRawBuffer(nullptr);
  if (!raw_hstr) {
    return std::string();
  }

  return wstring_to_stdstring(std::wstring(raw_hstr));

}

}  // namespace

EngineDeviceList UWPDeviceFinder::ListDevices() {

  ComPtr<IDeviceInformationStatics> device_info_statics;
  HRESULT hr = ABI::Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &device_info_statics);
  if (FAILED(hr)) {
    return EngineDeviceList();
  }

  ComPtr<IAsyncOperation<DeviceInformationCollection*>> async_op;
  hr = device_info_statics->FindAllAsyncDeviceClass(DeviceClass::DeviceClass_AudioRender, &async_op);
  device_info_statics.Reset();
  if (FAILED(hr)) {
    return EngineDeviceList();
  }

  hr = SyncWait<DeviceInformationCollection*>(async_op.Get());
  if (FAILED(hr)) {
    return EngineDeviceList();
  }

  ComPtr<IVectorView<DeviceInformation*>> device_list;
  hr = async_op->GetResults(&device_list);
  async_op.Reset();
  if (FAILED(hr)) {
    return EngineDeviceList();
  }

  unsigned int count = 0;
  hr = device_list->get_Size(&count);
  if (FAILED(hr)) {
    return EngineDeviceList();
  }

  EngineDeviceList devices;

  {
    EngineDevice default_device;
    default_device.description = "Default device"_L1;
    default_device.iconname = default_device.GuessIconName();
    devices.append(default_device);
  }

  for (unsigned int i = 0; i < count; i++) {

    ComPtr<IDeviceInformation> device_info;
    hr = device_list->GetAt(i, &device_info);
    if (FAILED(hr)) {
      continue;
    }

    boolean enabled;
    hr = device_info->get_IsEnabled(&enabled);
    if (FAILED(hr) || !enabled) {
      continue;
    }

    HString id;
    hr = device_info->get_Id(id.GetAddressOf());
    if (FAILED(hr) || !id.IsValid()) {
      continue;
    }

    HString name;
    hr = device_info->get_Name(name.GetAddressOf());
    if (FAILED(hr) || !name.IsValid()) {
      continue;
    }

    EngineDevice device;
    device.value = QString::fromStdString(hstring_to_stdstring(&id));
    device.description = QString::fromStdString(hstring_to_stdstring(&name));
    device.iconname = device.GuessIconName();
    devices.append(device);
  }

  return devices;

}
