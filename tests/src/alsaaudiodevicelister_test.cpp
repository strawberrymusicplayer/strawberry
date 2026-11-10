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

#include "gtest_include.h"

#include <QString>
#include <QVariant>
#include <QVariantList>

#include "engine/enginedevice.h"
#include "engine/alsaaudiodevicelister.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// The "hw" and "plughw" devices are saved with the card ID, the value with the card and device number selects the same device.
TEST(AlsaAudioDeviceListerTest, DevicesHaveNameAndNumber) {

  const EngineDeviceList devices = AlsaAudioDeviceLister::MakeDevices(u"HDA Intel PCH ALC897 Analog"_s, 1, u"PCH"_s, 3);
  ASSERT_EQ(2, devices.count());

  EXPECT_EQ(QVariantList() << u"hw:PCH,3"_s << u"hw:1,3"_s, devices[0].values);
  EXPECT_EQ(QVariant(u"hw:PCH,3"_s), devices[0].value());
  EXPECT_EQ(QVariantList() << u"plughw:PCH,3"_s << u"plughw:1,3"_s, devices[1].values);
  EXPECT_EQ(QVariant(u"plughw:PCH,3"_s), devices[1].value());

  for (const EngineDevice &device : devices) {
    EXPECT_EQ(u"HDA Intel PCH ALC897 Analog"_s, device.description);
    EXPECT_EQ(u"intel"_s, device.iconname);
  }

}

}  // namespace
