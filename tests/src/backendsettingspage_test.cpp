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

#include <memory>

#include "gtest_include.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListView>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "includes/shared_ptr.h"
#include "core/player.h"
#include "core/urlhandlers.h"
#include "engine/audiodevicelisters.h"
#include "engine/enginebase.h"
#include "engine/enginedevice.h"
#include "engine/gstengine.h"
#include "settings/backendsettingspage.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// The value which is saved is the first value, a device without values has no value.
TEST(EngineDeviceTest, ValueIsFirstValue) {

  EngineDevice device;
  EXPECT_FALSE(device.value().isValid());

  device.values << u"hw:PCH,0"_s << u"hw:0,0"_s;
  EXPECT_EQ(QVariant(u"hw:PCH,0"_s), device.value());

}

// At file scope, since it's a friend of BackendSettingsPage.
class BackendSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FillDevices(&combobox_);
  }

  // The items are like the items for ALSA, with the "Automatically select" and "Custom" items without a device.
  static void FillDevices(QComboBox *combobox) {
    combobox->addItem(u"Automatically select"_s, QVariant());
    AddDevice(combobox, u"hw:PCH,0"_s, u"hw:0,0"_s);
    AddDevice(combobox, u"plughw:PCH,0"_s, u"plughw:0,0"_s);
    AddDevice(combobox, u"hw:NVidia,3"_s, u"hw:1,3"_s);
    AddDevice(combobox, u"plughw:NVidia,3"_s, u"plughw:1,3"_s);
    combobox->addItem(u"Custom"_s, QVariant());
  }

  static void AddDevice(QComboBox *combobox, const QString &value, const QString &alternative_value) {
    EngineDevice device;
    device.description = value;
    device.values << value << alternative_value;
    BackendSettingsPage::AddDevice(combobox, device);
  }

  void AddDevice(const EngineDevice &device) {
    BackendSettingsPage::AddDevice(&combobox_, device);
  }

  int FindDevice(const QVariant &device, const bool visible_only = false) const {
    return BackendSettingsPage::FindDevice(&combobox_, device, visible_only);
  }

  QString ValueAt(const int index) const { return combobox_.itemData(index).toString(); }

#ifdef HAVE_ALSA
  // Creates the page with the ALSA output and the devices, like loaded with the "hw" plugin selected.
  // The engine is not initialized, it's only used for which outputs support ALSA and custom devices.
  void CreatePage() {

    url_handlers_ = make_shared<UrlHandlers>();
    player_ = make_shared<Player>(nullptr, url_handlers_, nullptr);
    page_ = std::make_unique<BackendSettingsPage>(nullptr, player_, make_shared<AudioDeviceListers>());

    QComboBox *combobox_output = page_->findChild<QComboBox*>(u"combobox_output"_s);
    combobox_device_ = page_->findChild<QComboBox*>(u"combobox_device"_s);
    lineedit_device_ = page_->findChild<QLineEdit*>(u"lineedit_device"_s);
    QWidget *widget_alsa_plugin = page_->findChild<QWidget*>(u"widget_alsa_plugin"_s);
    radiobutton_alsa_hw_ = page_->findChild<QRadioButton*>(u"radiobutton_alsa_hw"_s);
    radiobutton_alsa_plughw_ = page_->findChild<QRadioButton*>(u"radiobutton_alsa_plughw"_s);
    ASSERT_NE(nullptr, combobox_output);
    ASSERT_NE(nullptr, combobox_device_);
    ASSERT_NE(nullptr, lineedit_device_);
    ASSERT_NE(nullptr, widget_alsa_plugin);
    ASSERT_NE(nullptr, radiobutton_alsa_hw_);
    ASSERT_NE(nullptr, radiobutton_alsa_plughw_);

    combobox_output->clear();
    combobox_output->addItem(u"ALSA"_s, QVariant::fromValue(EngineBase::OutputDetails{QLatin1String(GstEngine::kALSASink), u"ALSA"_s, QString()}));
    combobox_device_->clear();
    FillDevices(combobox_device_);
    lineedit_device_->setEnabled(true);
    widget_alsa_plugin->setEnabled(true);
    radiobutton_alsa_hw_->setEnabled(true);
    radiobutton_alsa_plughw_->setEnabled(true);
    radiobutton_alsa_hw_->setChecked(true);
    page_->SwitchALSADevices(BackendSettingsPage::ALSAPluginType::HW);
    page_->configloaded_ = true;

  }

  QString CurrentDeviceValue() const { return combobox_device_->currentData().toString(); }
  QVariant CurrentDevice() const { return page_->CurrentDevice(); }

  SharedPtr<UrlHandlers> url_handlers_;
  SharedPtr<Player> player_;
  std::unique_ptr<BackendSettingsPage> page_;
  QComboBox *combobox_device_ = nullptr;
  QLineEdit *lineedit_device_ = nullptr;
  QRadioButton *radiobutton_alsa_hw_ = nullptr;
  QRadioButton *radiobutton_alsa_plughw_ = nullptr;
#endif

  QComboBox combobox_;
};

TEST_F(BackendSettingsPageTest, FindsDeviceByValue) {

  EXPECT_EQ(u"hw:PCH,0"_s, ValueAt(FindDevice(u"hw:PCH,0"_s)));
  EXPECT_EQ(u"plughw:NVidia,3"_s, ValueAt(FindDevice(u"plughw:NVidia,3"_s)));

}

// A setting with the card and device number is mapped to the same item as the setting with the card ID.
TEST_F(BackendSettingsPageTest, FindsDeviceByAlternativeValue) {

  EXPECT_EQ(u"hw:PCH,0"_s, ValueAt(FindDevice(u"hw:0,0"_s)));
  EXPECT_EQ(u"plughw:PCH,0"_s, ValueAt(FindDevice(u"plughw:0,0"_s)));
  EXPECT_EQ(u"hw:NVidia,3"_s, ValueAt(FindDevice(u"hw:1,3"_s)));
  EXPECT_EQ(u"plughw:NVidia,3"_s, ValueAt(FindDevice(u"plughw:1,3"_s)));

}

// No device is not the "Automatically select" or "Custom" item, a device with an empty value, or a device without values, like the default DirectSound device.
TEST_F(BackendSettingsPageTest, DoesNotFindUnknownOrNoDevice) {

  EngineDevice empty_device;
  empty_device.description = u"Empty"_s;
  empty_device.values << QString();
  AddDevice(empty_device);

  EngineDevice default_device;
  default_device.description = u"Default device"_s;
  AddDevice(default_device);

  EXPECT_EQ(-1, FindDevice(u"hw:2,0"_s));
  EXPECT_EQ(-1, FindDevice(u"hw:1,0"_s));
  EXPECT_EQ(-1, FindDevice(QString()));
  EXPECT_EQ(-1, FindDevice(QVariant()));

}

// A number, like the device ID on macOS, is the same device as the same number of another type, which the settings can return, but not as a string.
TEST_F(BackendSettingsPageTest, FindsDeviceWithNonStringValue) {

  EngineDevice device;
  device.description = u"Speakers"_s;
  device.values << 42U;
  AddDevice(device);

  const int index = FindDevice(42U);
  ASSERT_NE(-1, index);
  EXPECT_EQ(QVariant(42U), combobox_.itemData(index));
  EXPECT_EQ(index, FindDevice(static_cast<qint64>(42)));
  EXPECT_EQ(-1, FindDevice(u"42"_s));

}

// The "hw" items are hidden when the "plughw" plugin is selected.
TEST_F(BackendSettingsPageTest, FindsOnlyVisibleDevices) {

  QListView *view = qobject_cast<QListView*>(combobox_.view());
  ASSERT_NE(nullptr, view);
  for (int i = 0; i < combobox_.count(); ++i) {
    if (ValueAt(i).startsWith(u"hw:"_s)) view->setRowHidden(i, true);
  }

  EXPECT_EQ(-1, FindDevice(u"hw:0,0"_s, true));
  EXPECT_EQ(-1, FindDevice(u"hw:PCH,0"_s, true));
  EXPECT_EQ(u"plughw:PCH,0"_s, ValueAt(FindDevice(u"plughw:0,0"_s, true)));
  EXPECT_EQ(u"hw:PCH,0"_s, ValueAt(FindDevice(u"hw:0,0"_s, false)));

}

#ifdef HAVE_ALSA

// A device with the card number is kept when it's typed, and when switching between the "hw" and "plughw" plugins.
TEST_F(BackendSettingsPageTest, SwitchingPluginKeepsDeviceWithCardNumber) {

  CreatePage();
  if (::testing::Test::HasFatalFailure()) return;

  lineedit_device_->setText(u"hw:0,0"_s);
  EXPECT_EQ(u"hw:PCH,0"_s, CurrentDeviceValue());
  EXPECT_EQ(u"hw:0,0"_s, lineedit_device_->text());
  EXPECT_EQ(QVariant(u"hw:0,0"_s), CurrentDevice());

  radiobutton_alsa_plughw_->click();
  EXPECT_EQ(u"plughw:PCH,0"_s, CurrentDeviceValue());
  EXPECT_EQ(u"plughw:0,0"_s, lineedit_device_->text());
  EXPECT_EQ(QVariant(u"plughw:0,0"_s), CurrentDevice());

  radiobutton_alsa_hw_->click();
  EXPECT_EQ(u"hw:PCH,0"_s, CurrentDeviceValue());
  EXPECT_EQ(u"hw:0,0"_s, lineedit_device_->text());
  EXPECT_EQ(QVariant(u"hw:0,0"_s), CurrentDevice());

}

// A device with the card ID stays a device with the card ID when switching plugins.
TEST_F(BackendSettingsPageTest, SwitchingPluginKeepsDeviceWithCardID) {

  CreatePage();
  if (::testing::Test::HasFatalFailure()) return;

  lineedit_device_->setText(u"hw:NVidia,3"_s);
  radiobutton_alsa_plughw_->click();
  EXPECT_EQ(u"plughw:NVidia,3"_s, CurrentDeviceValue());
  EXPECT_EQ(u"plughw:NVidia,3"_s, lineedit_device_->text());
  EXPECT_EQ(QVariant(u"plughw:NVidia,3"_s), CurrentDevice());

}

// Selecting another device replaces a device with the card number of the device selected before.
TEST_F(BackendSettingsPageTest, SelectingOtherDeviceReplacesDeviceWithCardNumber) {

  CreatePage();
  if (::testing::Test::HasFatalFailure()) return;

  lineedit_device_->setText(u"hw:0,0"_s);
  ASSERT_EQ(u"hw:PCH,0"_s, CurrentDeviceValue());

  combobox_device_->setCurrentIndex(FindDevice(u"hw:NVidia,3"_s));
  EXPECT_EQ(u"hw:NVidia,3"_s, lineedit_device_->text());
  EXPECT_EQ(QVariant(u"hw:NVidia,3"_s), CurrentDevice());

}

#endif  // HAVE_ALSA
