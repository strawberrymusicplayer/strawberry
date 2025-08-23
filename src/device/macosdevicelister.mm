/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifdef HAVE_MTP
#  include <libmtp.h>
#endif

#include <AvailabilityMacros.h>
#include <CoreFoundation/CFRunLoop.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/kext/KextManager.h>
#include <IOKit/IOCFPlugin.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>

#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QUrlQuery>
#include <QUrl>
#include <QScopeGuard>

#include "macosdevicelister.h"
#include "includes/scoped_cftyperef.h"
#include "includes/scoped_nsobject.h"
#include "core/logging.h"
#include "core/scoped_nsautorelease_pool.h"

#ifdef HAVE_MTP
#  include "mtpconnection.h"
#endif

#import <AppKit/NSWorkspace.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>

using namespace Qt::Literals::StringLiterals;

#ifndef kUSBSerialNumberString
#define kUSBSerialNumberString "USB Serial Number"
#endif

#ifndef kUSBVendorString
#define kUSBVendorString "USB Vendor Name"
#endif

#ifndef kUSBProductString
#define kUSBProductString "USB Product Name"
#endif

// io_object_t, io_service_t, io_iterator_t etc. are all typedef'd to unsigned int,
// hence the lack of templating here.
class ScopedIOObject {
 public:
  explicit ScopedIOObject(io_object_t object = 0) : object_(object) {}

  ~ScopedIOObject() {
    if (object_) IOObjectRelease(object_);
  }

  io_object_t get() const { return object_; }

 private:
  io_object_t object_;

  Q_DISABLE_COPY(ScopedIOObject);
};

// Helpful MTP & USB links:
// Apple USB device interface guide:
// http://developer.apple.com/mac/library/documentation/DeviceDrivers/Conceptual/USBBook/USBDeviceInterfaces/USBDevInterfaces.html
// Example Apple code for requesting a USB device descriptor:
// http://www.opensource.apple.com/source/IOUSBFamily/IOUSBFamily-208.4.5/USBProber/BusProbeClass.m
// Libmtp's detection code:
// http://libmtp.cvs.sourceforge.net/viewvc/libmtp/libmtp/src/libusb-glue.c?view=markup
// Libusb's Mac code:
// http://www.libusb.org/browser/libusb/libusb/os/darwin_usb.c
// Microsoft OS Descriptors:
// http://www.microsoft.com/whdc/connect/usb/os_desc.mspx
// Symbian docs for implementing the device side:
// http://developer.symbian.org/main/documentation/reference/s3/pdk/GUID-3FF0F248-EDF0-5348-BC43-869CE1B5B415.html
// Libgphoto2 MTP detection code:
// http://www.sfr-fresh.com/unix/privat/libgphoto2-2.4.10.1.tar.gz:a/libgphoto2-2.4.10.1/libgphoto2_port/usb/check-mtp-device.c

#ifdef HAVE_MTP
QSet<MacOsDeviceLister::MTPDevice> MacOsDeviceLister::sMTPDeviceList;
#endif

#ifdef HAVE_MTP
size_t qHash(const MacOsDeviceLister::MTPDevice &mtp_device) {
  return qHash(mtp_device.vendor_id) ^ qHash(mtp_device.product_id);
}
#endif

MacOsDeviceLister::MacOsDeviceLister(QObject *parent) : DeviceLister(parent) {}

MacOsDeviceLister::~MacOsDeviceLister() { CFRelease(loop_session_); }

bool MacOsDeviceLister::Init() {

  ScopedNSAutoreleasePool pool;

#ifdef HAVE_MTP
  // Populate MTP Device list.
  if (sMTPDeviceList.empty()) {
    LIBMTP_device_entry_t *devices = nullptr;
    int num = 0;
    if (LIBMTP_Get_Supported_Devices_List(&devices, &num) != 0) {
      qLog(Warning) << "Failed to get MTP device list";
    }
    else {
      for (int i = 0; i < num; ++i) {
        LIBMTP_device_entry_t device = devices[i];
        MTPDevice mtp_device;
        mtp_device.vendor = QString::fromLatin1(device.vendor);
        mtp_device.vendor_id = device.vendor_id;
        mtp_device.product = QString::fromLatin1(device.product);
        mtp_device.product_id = device.product_id;
        mtp_device.quirks = device.device_flags;
        sMTPDeviceList << mtp_device;
      }
    }

    MTPDevice mtp_device;
    mtp_device.vendor = "SanDisk"_L1;
    mtp_device.vendor_id = 0x781;
    mtp_device.product = "Sansa Clip+"_L1;
    mtp_device.product_id = 0x74d0;

    mtp_device.quirks = 0x2 | 0x4 | 0x40 | 0x4000;
    sMTPDeviceList << mtp_device;
  }
#endif  // HAVE_MTP

  run_loop_ = CFRunLoopGetCurrent();

  // Register for disk mounts/unmounts.
  loop_session_ = DASessionCreate(kCFAllocatorDefault);
  DARegisterDiskAppearedCallback(loop_session_, kDADiskDescriptionMatchVolumeMountable, &DiskAddedCallback, reinterpret_cast<void*>(this));
  DARegisterDiskDisappearedCallback(loop_session_, nullptr, &DiskRemovedCallback, reinterpret_cast<void*>(this));
  DASessionScheduleWithRunLoop(loop_session_, run_loop_, kCFRunLoopDefaultMode);

  // Register for USB device connection/disconnection.
  IONotificationPortRef notification_port = IONotificationPortCreate(
#if defined(MAC_OS_VERSION_12_0) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_VERSION_12_0)
        kIOMainPortDefault
#else
        kIOMasterPortDefault
#endif
        );
  CFMutableDictionaryRef matching_dict = IOServiceMatching(kIOUSBDeviceClassName);
  // IOServiceAddMatchingNotification decreases reference count.
  CFRetain(matching_dict);
  io_iterator_t it;
  kern_return_t err = IOServiceAddMatchingNotification(
      notification_port,
      kIOFirstMatchNotification,
      matching_dict,
      &USBDeviceAddedCallback,
      reinterpret_cast<void*>(this),
      &it);
  if (err == KERN_SUCCESS) {
    USBDeviceAddedCallback(this, it);
  }
  else {
    qLog(Warning) << "Could not add notification on USB device connection";
  }

  err = IOServiceAddMatchingNotification(
      notification_port,
      kIOTerminatedNotification,
      matching_dict,
      &USBDeviceRemovedCallback,
      reinterpret_cast<void*>(this),
      &it);
  if (err == KERN_SUCCESS) {
    USBDeviceRemovedCallback(this, it);
  }
  else {
    qLog(Warning) << "Could not add notification USB device removal";
  }

  CFRunLoopSourceRef io_source = IONotificationPortGetRunLoopSource(notification_port);
  CFRunLoopAddSource(run_loop_, io_source, kCFRunLoopDefaultMode);

  CFRunLoopRun();
  return true;

}

void MacOsDeviceLister::ExitAsync() {
  Q_EMIT ExitFinished();
}

void MacOsDeviceLister::ShutDown() { CFRunLoopStop(run_loop_); }

// IOKit helpers.
namespace {

// Caller is responsible for calling CFRelease().
CFTypeRef GetUSBRegistryEntry(io_object_t device, CFStringRef key) {
  io_iterator_t it;
  if (IORegistryEntryGetParentIterator(device, kIOServicePlane, &it) == KERN_SUCCESS) {
    io_object_t next;
    while ((next = IOIteratorNext(it))) {
      CFTypeRef registry_entry = reinterpret_cast<CFStringRef>(IORegistryEntryCreateCFProperty(next, key, kCFAllocatorDefault, 0));
      if (registry_entry) {
        IOObjectRelease(next);
        IOObjectRelease(it);
        return registry_entry;
      }

      CFTypeRef ret = GetUSBRegistryEntry(next, key);
      if (ret) {
        IOObjectRelease(next);
        IOObjectRelease(it);
        return ret;
      }

      IOObjectRelease(next);
    }
  }

  IOObjectRelease(it);
  return nullptr;
}

QString GetUSBRegistryEntryString(io_object_t device, CFStringRef key) {

  ScopedCFTypeRef<CFStringRef> registry_string(reinterpret_cast<CFStringRef>(GetUSBRegistryEntry(device, key)));
  if (registry_string) {
    return QString::fromUtf8([reinterpret_cast<NSString*>(registry_string.get()) UTF8String]);
  }

  return QString();

}

NSObject *GetPropertyForDevice(io_object_t device, CFStringRef key) {

  CFMutableDictionaryRef properties;
  kern_return_t ret = IORegistryEntryCreateCFProperties(device, &properties, kCFAllocatorDefault, 0);

  if (ret != KERN_SUCCESS) {
    return nil;
  }

  scoped_nsobject<NSDictionary> dict(reinterpret_cast<NSDictionary*>(properties));  // Takes ownership.
  NSObject *prop = [dict objectForKey:reinterpret_cast<NSString*>(key)];
  if (prop) {
    // The dictionary goes out of scope so we should retain this object.
    [prop retain];
    return prop;
  }

  io_object_t parent;
  ret = IORegistryEntryGetParentEntry(device, kIOServicePlane, &parent);
  if (ret == KERN_SUCCESS) {
    return GetPropertyForDevice(parent, key);
  }

  return nil;

}

int GetUSBDeviceClass(io_object_t device) {

  ScopedCFTypeRef<CFTypeRef> interface_class(IORegistryEntrySearchCFProperty(device, kIOServicePlane, CFSTR(kUSBInterfaceClass), kCFAllocatorDefault, kIORegistryIterateRecursively));
  NSNumber *number = reinterpret_cast<NSNumber*>(interface_class.get());
  if (number) {
    int ret = [number unsignedShortValue];
    return ret;
  }

  return 0;

}

QString GetIconForDevice(io_object_t device) {

  scoped_nsobject<NSDictionary> media_icon(reinterpret_cast<NSDictionary*>(GetPropertyForDevice(device, CFSTR("IOMediaIcon"))));
  if (media_icon) {
    NSString *bundle = reinterpret_cast<NSString*>([media_icon objectForKey:@"CFBundleIdentifier"]);
    NSString *file = reinterpret_cast<NSString*>([media_icon objectForKey:@"IOBundleResourceFile"]);

    scoped_nsobject<NSURL> bundle_url(reinterpret_cast<NSURL*>(KextManagerCreateURLForBundleIdentifier(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(bundle))));

    QString path = QString::fromUtf8([[bundle_url path] UTF8String]);
    path += "/Contents/Resources/"_L1;
    path += QString::fromUtf8([file UTF8String]);
    return path;
  }

  return QString();

}

QString GetSerialForDevice(io_object_t device) {

  const QString serial = GetUSBRegistryEntryString(device, CFSTR(kUSBSerialNumberString));
  if (!serial.isEmpty()) {
    return "USB/"_L1 + serial;
  }

  return QString();

}

#ifdef HAVE_MTP
QString GetSerialForMTPDevice(io_object_t device) {

  scoped_nsobject<NSString> serial(reinterpret_cast<NSString*>(GetPropertyForDevice(device, CFSTR(kUSBSerialNumberString))));
  return "MTP/"_L1 + QString::fromUtf8([serial UTF8String]);

}
#endif  // HAVE_MTP

QString FindDeviceProperty(const QString &bsd_name, CFStringRef property) {

  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  ScopedIOObject device(DADiskCopyIOMedia(disk.get()));
  QString ret = GetUSBRegistryEntryString(device.get(), property);

  return ret;

}

}  // namespace

#ifdef HAVE_MTP
quint64 MacOsDeviceLister::GetFreeSpace(const QUrl &url) {

  QMutexLocker l(&libmtp_mutex_);
  MtpConnection connection(url);
  if (!connection.is_valid()) {
    qLog(Warning) << "Error connecting to MTP device, couldn't get device free space";
    return -1;
  }
  LIBMTP_devicestorage_t *storage = connection.device()->storage;
  quint64 free_bytes = 0;
  while (storage) {
    free_bytes += storage->FreeSpaceInBytes;
    storage = storage->next;
  }

  return free_bytes;

}

quint64 MacOsDeviceLister::GetCapacity(const QUrl &url) {

  QMutexLocker l(&libmtp_mutex_);
  MtpConnection connection(url);
  if (!connection.is_valid()) {
    qLog(Warning) << "Error connecting to MTP device, couldn't get device capacity";
    return -1;
  }
  LIBMTP_devicestorage_t *storage = connection.device()->storage;
  quint64 capacity_bytes = 0;
  while (storage) {
    capacity_bytes += storage->MaxCapacity;
    storage = storage->next;
  }
  return capacity_bytes;

}

#endif  // HAVE_MTP

void MacOsDeviceLister::DiskAddedCallback(DADiskRef disk, void *context) {

  MacOsDeviceLister *me = reinterpret_cast<MacOsDeviceLister*>(context);

  scoped_nsobject<NSDictionary> properties(reinterpret_cast<NSDictionary*>(DADiskCopyDescription(disk)));

#ifdef HAVE_AUDIOCD
  NSString *kind = [properties objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionMediaKindKey)];
  if (kind && strcmp([kind UTF8String], kIOCDMediaClass) == 0) {
    // CD inserted.
    const QString bsd_name = QString::fromLatin1(DADiskGetBSDName(disk));
    me->cd_devices_ << bsd_name;
    Q_EMIT me->DeviceAdded(bsd_name);
    return;
  }
#endif  // HAVE_AUDIOCD

  NSURL *volume_path = [[properties objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionVolumePathKey)] copy];

  if (volume_path) {
    ScopedIOObject device(DADiskCopyIOMedia(disk));
    ScopedCFTypeRef<CFStringRef> class_name(IOObjectCopyClass(device.get()));
    if (class_name && CFStringCompare(class_name.get(), CFSTR(kIOMediaClass), 0) == kCFCompareEqualTo) {
      const QString vendor = GetUSBRegistryEntryString(device.get(), CFSTR(kUSBVendorString));
      const QString product = GetUSBRegistryEntryString(device.get(), CFSTR(kUSBProductString));

      CFMutableDictionaryRef cf_properties;
      kern_return_t ret = IORegistryEntryCreateCFProperties(device.get(), &cf_properties, kCFAllocatorDefault, 0);

      if (ret == KERN_SUCCESS) {
        scoped_nsobject<NSDictionary> dict(reinterpret_cast<NSDictionary*>(cf_properties));  // Takes ownership.
        if ([[dict objectForKey:@"Removable"] intValue] == 1) {
          const QString serial = GetSerialForDevice(device.get());
          if (!serial.isEmpty()) {
            me->current_devices_[serial] = QString::fromLatin1(DADiskGetBSDName(disk));
            Q_EMIT me->DeviceAdded(serial);
          }
        }
      }
    }
  }

}

void MacOsDeviceLister::DiskRemovedCallback(DADiskRef disk, void *context) {

  MacOsDeviceLister *me = reinterpret_cast<MacOsDeviceLister*>(context);
  // We cannot access the USB tree when the disk is removed but we still get the BSD disk name.

  const QString bsd_name = QString::fromLatin1(DADiskGetBSDName(disk));
  if (me->cd_devices_.remove(bsd_name)) {
    Q_EMIT me->DeviceRemoved(bsd_name);
    return;
  }

  for (QMap<QString, QString>::iterator it = me->current_devices_.begin(); it != me->current_devices_.end(); ++it) {
    if (it.value() == bsd_name) {
      Q_EMIT me->DeviceRemoved(it.key());
      me->current_devices_.erase(it);
      break;
    }
  }

}

bool DeviceRequest(IOUSBDeviceInterface **dev,
                   quint8 direction,
                   quint8 type,
                   quint8 recipient,
                   quint8 request_code,
                   quint16 value,
                   quint16 index,
                   quint16 length,
                   QByteArray *data) {

  IOUSBDevRequest req;
  req.bmRequestType = USBmakebmRequestType(direction, type, recipient);
  req.bRequest = request_code;
  req.wValue = value;
  req.wIndex = index;
  req.wLength = length;
  data->resize(256);
  req.pData = data->data();
  kern_return_t err = (*dev)->DeviceRequest(dev, &req);
  if (err != kIOReturnSuccess) {
    return false;
  }
  data->resize(req.wLenDone);

  return true;

}

int GetBusNumber(io_object_t o) {

  io_iterator_t it;
  kern_return_t err = IORegistryEntryGetParentIterator(o, kIOServicePlane, &it);
  if (err != KERN_SUCCESS) {
    return -1;
  }
  while ((o = IOIteratorNext(it))) {
    NSObject *bus = GetPropertyForDevice(o, CFSTR("USBBusNumber"));
    if (bus) {
      NSNumber *bus_num = reinterpret_cast<NSNumber*>(bus);
      return [bus_num intValue];
    }
  }

  return -1;

}

void MacOsDeviceLister::USBDeviceAddedCallback(void *refcon, io_iterator_t it) {

  MacOsDeviceLister *me = reinterpret_cast<MacOsDeviceLister*>(refcon);
  Q_UNUSED(me)

  io_object_t object;
  while ((object = IOIteratorNext(it))) {
    ScopedCFTypeRef<CFStringRef> class_name(IOObjectCopyClass(object));
    const QScopeGuard io_object_release = qScopeGuard([object]() { IOObjectRelease(object); });

    if (CFStringCompare(class_name.get(), CFSTR(kIOUSBDeviceClassName), 0) == kCFCompareEqualTo) {

      const int interface_class = GetUSBDeviceClass(object);
      qLog(Debug) << "Interface class:" << interface_class;

#ifdef HAVE_MTP

      NSString *vendor = reinterpret_cast<NSString*>(GetPropertyForDevice(object, CFSTR(kUSBVendorString)));
      NSString *product = reinterpret_cast<NSString*>(GetPropertyForDevice(object, CFSTR(kUSBProductString)));
      NSNumber *vendor_id = reinterpret_cast<NSNumber*>(GetPropertyForDevice(object, CFSTR(kUSBVendorID)));
      NSNumber *product_id = reinterpret_cast<NSNumber*>(GetPropertyForDevice(object, CFSTR(kUSBProductID)));

      const QString serial = GetSerialForMTPDevice(object);

      MTPDevice mtp_device;
      mtp_device.vendor = QString::fromUtf8([vendor UTF8String]);
      mtp_device.product = QString::fromUtf8([product UTF8String]);
      mtp_device.vendor_id = [vendor_id unsignedShortValue];
      mtp_device.product_id = [product_id unsignedShortValue];
      mtp_device.quirks = 0;

      mtp_device.bus = -1;
      mtp_device.address = -1;

      if (mtp_device.vendor_id == kAppleVendorID || // I think we can safely skip Apple products.
          // Blacklist ilok2 as this probe may be breaking it.
          (mtp_device.vendor_id == 0x088e && mtp_device.product_id == 0x5036) ||
          // Blacklist eLicenser
          (mtp_device.vendor_id == 0x0819 && mtp_device.product_id == 0x0101) ||
          // Skip HID devices, printers and hubs.
          interface_class == kUSBHIDInterfaceClass ||
          interface_class == kUSBPrintingInterfaceClass ||
          interface_class == kUSBHubClass) {
        continue;
      }

      NSNumber *addr = reinterpret_cast<NSNumber*>(GetPropertyForDevice(object, CFSTR("USB Address")));
      const int bus = GetBusNumber(object);
      if (!addr || bus == -1) {
        // Failed to get bus or address number.
        continue;
      }
      mtp_device.bus = bus;
      mtp_device.address = [addr intValue];

      // First check the libmtp device list.
      QSet<MTPDevice>::const_iterator it2 = sMTPDeviceList.find(mtp_device);
      if (it2 != sMTPDeviceList.end()) {
        // Fill in quirks flags from libmtp.
        mtp_device.quirks = it2->quirks;
        me->FoundMTPDevice(mtp_device, GetSerialForMTPDevice(object));
        continue;
      }

#endif  // HAVE_MTP

      IOCFPlugInInterface **plugin_interface = nullptr;
      SInt32 score;
      kern_return_t err = IOCreatePlugInInterfaceForService(object, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin_interface, &score);
      if (err != KERN_SUCCESS) {
        continue;
      }

      IOUSBDeviceInterface **dev = nullptr;
      HRESULT result = (*plugin_interface)->QueryInterface(plugin_interface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), reinterpret_cast<LPVOID*>(&dev));

      (*plugin_interface)->Release(plugin_interface);

      if (result || !dev) {
        continue;
      }

      err = (*dev)->USBDeviceOpen(dev);
      if (err != kIOReturnSuccess) {
        continue;
      }

      // Automatically close & release usb device at scope exit.
      const QScopeGuard dev_close_release = qScopeGuard([dev]() {
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
      });

      // Request the string descriptor at 0xee.
      // This is a magic string that indicates whether this device supports MTP.
      QByteArray data;
      bool ret = DeviceRequest(dev, kUSBIn, kUSBStandard, kUSBDevice, kUSBRqGetDescriptor, (kUSBStringDesc << 8) | 0xee, 0x0409, 2, &data);
      if (!ret) continue;

      const UInt8 string_len = data[0];

      ret = DeviceRequest(dev, kUSBIn, kUSBStandard, kUSBDevice, kUSBRqGetDescriptor, (kUSBStringDesc << 8) | 0xee, 0x0409, string_len, &data);
      if (!ret) continue;

      // The device actually returned something. That's a good sign.
      // Because this was designed by MS, the characters are in UTF-16 (LE?).
      QString str = QString::fromUtf16(reinterpret_cast<char16_t*>(data.data() + 2), (data.size() / 2) - 2);

#ifdef HAVE_MTP
      if (str.startsWith("MSFT100"_L1)) {
        // We got the OS descriptor!
        char vendor_code = data[16];
        ret = DeviceRequest(dev, kUSBIn, kUSBVendor, kUSBDevice, vendor_code, 0, 4, 256, &data);
        if (!ret || data.at(0) != 0x28)
          continue;

        if (QString::fromLatin1(data.data() + 0x12, 3) != "MTP"_L1) {
          // Not quite.
          continue;
        }

        ret = DeviceRequest(dev, kUSBIn, kUSBVendor, kUSBDevice, vendor_code, 0, 5, 256, &data);
        if (!ret || data.at(0) != 0x28) {
          continue;
        }

        if (QString::fromLatin1(data.data() + 0x12, 3) != "MTP"_L1) {
          // Not quite.
          continue;
        }
        // Hurray! We made it!
        me->FoundMTPDevice(mtp_device, serial);
      }
#endif  // HAVE_MTP

    }
  }

}

void MacOsDeviceLister::USBDeviceRemovedCallback(void *refcon, io_iterator_t it) {

  MacOsDeviceLister *me = reinterpret_cast<MacOsDeviceLister*>(refcon);
  Q_UNUSED(me)

  io_object_t object;
  while ((object = IOIteratorNext(it))) {
    ScopedCFTypeRef<CFStringRef> class_name(IOObjectCopyClass(object));
    const QScopeGuard io_object_release = qScopeGuard([object]() { IOObjectRelease(object); });

    if (CFStringCompare(class_name.get(), CFSTR(kIOUSBDeviceClassName), 0) == kCFCompareEqualTo) {

#ifdef HAVE_MTP
      NSString *vendor = reinterpret_cast<NSString*>(GetPropertyForDevice(object, CFSTR(kUSBVendorString)));
      NSString *product = reinterpret_cast<NSString*>(GetPropertyForDevice(object, CFSTR(kUSBProductString)));
      NSNumber *vendor_id = reinterpret_cast<NSNumber*>(GetPropertyForDevice(object, CFSTR(kUSBVendorID)));
      NSNumber *product_id = reinterpret_cast<NSNumber*>(GetPropertyForDevice(object, CFSTR(kUSBProductID)));

      const QString serial = GetSerialForMTPDevice(object);

      MTPDevice mtp_device;
      mtp_device.vendor = QString::fromUtf8([vendor UTF8String]);
      mtp_device.product = QString::fromUtf8([product UTF8String]);
      mtp_device.vendor_id = [vendor_id unsignedShortValue];
      mtp_device.product_id = [product_id unsignedShortValue];

      me->RemovedMTPDevice(serial);
#endif // HAVE_MTP

    }
  }

}

#ifdef HAVE_MTP

void MacOsDeviceLister::RemovedMTPDevice(const QString &serial) {

  int count = mtp_devices_.remove(serial);
  if (count) {
    qLog(Debug) << "MTP device removed:" << serial;
    Q_EMIT DeviceRemoved(serial);
  }

}

void MacOsDeviceLister::FoundMTPDevice(const MTPDevice &device, const QString &serial) {

  qLog(Debug) << "New MTP device detected!" << device.bus << device.address;

  mtp_devices_[serial] = device;
  const QList<QUrl> urls = MakeDeviceUrls(serial);
  MTPDevice *mtp_device = &mtp_devices_[serial];
  mtp_device->capacity = GetCapacity(urls[0]);
  mtp_device->free_space = GetFreeSpace(urls[0]);

  Q_EMIT DeviceAdded(serial);

}

bool IsMTPSerial(const QString &serial) { return serial.startsWith("MTP"_L1); }

#endif  // HAVE_MTP

bool MacOsDeviceLister::IsCDDevice(const QString &serial) const {
  return cd_devices_.contains(serial);
}

QString MacOsDeviceLister::MakeFriendlyName(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    const MTPDevice &mtp_device = mtp_devices_[serial];
    if (mtp_device.vendor.isEmpty()) {
      return mtp_device.product;
    }
    else {
      return mtp_device.vendor + QLatin1Char(' ') + mtp_device.product;
    }
  }
#endif  // HAVE_MTP

  const QString bsd_name = IsCDDevice(serial) ? *cd_devices_.find(serial) : current_devices_[serial];
  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  if (IsCDDevice(serial)) {
    scoped_nsobject<NSDictionary> properties(reinterpret_cast<NSDictionary*>(DADiskCopyDescription(disk.get())));
    NSString *device_name = reinterpret_cast<NSString*>([properties.get() objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionMediaNameKey)]);

    return QString::fromUtf8([device_name UTF8String]);
  }

  ScopedIOObject device(DADiskCopyIOMedia(disk));

  const QString vendor = GetUSBRegistryEntryString(device.get(), CFSTR(kUSBVendorString));
  const QString product = GetUSBRegistryEntryString(device.get(), CFSTR(kUSBProductString));

  if (vendor.isEmpty()) {
    return product;
  }

  return vendor + QLatin1Char(' ') + product;

}

QList<QUrl> MacOsDeviceLister::MakeDeviceUrls(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    const MTPDevice &mtp_device = mtp_devices_[serial];
    const QString str = QString::asprintf("gphoto2://usb-%d-%d/", mtp_device.bus, mtp_device.address);
    QUrlQuery url_query;
    url_query.addQueryItem(u"vendor"_s, mtp_device.vendor);
    url_query.addQueryItem(u"vendor_id"_s, QString::number(mtp_device.vendor_id));
    url_query.addQueryItem(u"product"_s, mtp_device.product);
    url_query.addQueryItem(u"product_id"_s, QString::number(mtp_device.product_id));
    url_query.addQueryItem(u"quirks"_s, QString::number(mtp_device.quirks));
    QUrl url(str);
    url.setQuery(url_query);
    return QList<QUrl>() << url;
  }
#endif  // HAVE_MTP

  if (IsCDDevice(serial)) {
    return QList<QUrl>() << QUrl(u"cdda:///dev/r"_s + serial);
  }

  const QString bsd_name = current_devices_[serial];
  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  scoped_nsobject<NSDictionary> properties(reinterpret_cast<NSDictionary*>(DADiskCopyDescription(disk.get())));
  scoped_nsobject<NSURL> volume_path([[properties objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionVolumePathKey)] copy]);

  const QString path = QString::fromUtf8([[volume_path path] UTF8String]);
  const QUrl ret = MakeUrlFromLocalPath(path);

  return QList<QUrl>() << ret;

}

QStringList MacOsDeviceLister::DeviceUniqueIDs() {

#ifdef HAVE_MTP
  return current_devices_.keys() + mtp_devices_.keys();
#else
  return current_devices_.keys();
#endif

}

QVariantList MacOsDeviceLister::DeviceIcons(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    return QVariantList();
  }
#endif  // HAVE_MTP

  if (IsCDDevice(serial)) {
    return QVariantList() << u"media-optical"_s;
  }

  const QString bsd_name = current_devices_[serial];
  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  ScopedIOObject device(DADiskCopyIOMedia(disk.get()));
  const QString icon = GetIconForDevice(device.get());

  scoped_nsobject<NSDictionary> properties(reinterpret_cast<NSDictionary*>(DADiskCopyDescription(disk)));
  scoped_nsobject<NSURL> volume_path([[properties objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionVolumePathKey)] copy]);

  const QString path = QString::fromUtf8([[volume_path path] UTF8String]);

  QVariantList ret;
  ret << GuessIconForPath(path);
  ret << GuessIconForModel(DeviceManufacturer(serial), DeviceModel(serial));
  if (!icon.isEmpty()) {
    ret << icon;
  }

  return ret;

}

QString MacOsDeviceLister::DeviceManufacturer(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    return mtp_devices_[serial].vendor;
  }
#endif  // HAVE_MTP

  return FindDeviceProperty(current_devices_[serial], CFSTR(kUSBVendorString));

}

QString MacOsDeviceLister::DeviceModel(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    return mtp_devices_[serial].product;
  }
#endif  // HAVE_MTP

  return FindDeviceProperty(current_devices_[serial], CFSTR(kUSBProductString));

}

quint64 MacOsDeviceLister::DeviceCapacity(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    QList<QUrl> urls = MakeDeviceUrls(serial);
    return mtp_devices_[serial].capacity;
  }
#endif  // HAVE_MTP

  const QString bsd_name = current_devices_[serial];
  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  io_object_t device = DADiskCopyIOMedia(disk);

  NSNumber *capacity = reinterpret_cast<NSNumber*>(GetPropertyForDevice(device, CFSTR("Size")));

  const quint64 ret = [capacity unsignedLongLongValue];

  IOObjectRelease(device);

  return ret;

}

quint64 MacOsDeviceLister::DeviceFreeSpace(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    QList<QUrl> urls = MakeDeviceUrls(serial);
    return mtp_devices_[serial].free_space;
  }
#endif  // HAVE_MTP

  QString bsd_name = current_devices_[serial];
  ScopedCFTypeRef<DASessionRef> session(DASessionCreate(kCFAllocatorDefault));
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, session.get(), bsd_name.toLatin1().constData()));

  scoped_nsobject<NSDictionary> properties(reinterpret_cast<NSDictionary*>(DADiskCopyDescription(disk)));
  scoped_nsobject<NSURL> volume_path([[properties objectForKey:reinterpret_cast<NSString*>(kDADiskDescriptionVolumePathKey)] copy]);

  NSNumber *value = nil;
  NSError *error = nil;
  if ([volume_path getResourceValue:&value forKey: NSURLVolumeAvailableCapacityKey error: &error] && value) {
    return [value unsignedLongLongValue];
  }
  return 0;

}

QVariantMap MacOsDeviceLister::DeviceHardwareInfo(const QString &serial){
  Q_UNUSED(serial);
  return QVariantMap();
}

bool MacOsDeviceLister::AskForScan(const QString &serial) const {
  return !IsCDDevice(serial);
}

void MacOsDeviceLister::UnmountDevice(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) return;
#endif

  const QString bsd_name = current_devices_[serial];
  ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(kCFAllocatorDefault, loop_session_, bsd_name.toLatin1().constData()));

  DADiskUnmount(disk, kDADiskUnmountOptionDefault, &DiskUnmountCallback, this);

}

void MacOsDeviceLister::DiskUnmountCallback(DADiskRef disk, DADissenterRef dissenter, void *context) {

  if (dissenter) {
    qLog(Warning) << "Another app blocked the unmount";
  }
  else {
    DiskRemovedCallback(disk, context);
  }

}

void MacOsDeviceLister::UpdateDeviceFreeSpace(const QString &serial) {

#ifdef HAVE_MTP
  if (IsMTPSerial(serial)) {
    if (mtp_devices_.contains(serial)) {
      QList<QUrl> urls = MakeDeviceUrls(serial);
      MTPDevice *mtp_device = &mtp_devices_[serial];
      mtp_device->free_space = GetFreeSpace(urls[0]);
    }
  }
#endif

  Q_EMIT DeviceChanged(serial);

}
