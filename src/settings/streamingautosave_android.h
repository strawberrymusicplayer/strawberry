// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STREAMINGAUTOSAVE_ANDROID_H
#define STREAMINGAUTOSAVE_ANDROID_H

#include <functional>

class QObject;
class QWidget;

void ConnectStreamingAutoSave(QObject *context, QWidget *page, const std::function<void()> &save);

#endif  // STREAMINGAUTOSAVE_ANDROID_H
