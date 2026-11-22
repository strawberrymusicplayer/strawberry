// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STREAMINGAUTOSAVE_ANDROID_H
#define STREAMINGAUTOSAVE_ANDROID_H

#include <functional>

class QObject;
class QWidget;
class SettingsPage;

void ConnectStreamingAutoSave(QObject *context, QWidget *page, const std::function<void()> &save);
// Also saves after page buttons, such as Remove folder, and list or checkable group edits that Apply() does not compare by itself.
void ConnectSettingsPageAutoSave(QObject *context, SettingsPage *page, const std::function<void()> &save);

#endif  // STREAMINGAUTOSAVE_ANDROID_H
