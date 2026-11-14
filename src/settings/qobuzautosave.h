// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QOBUZAUTOSAVE_H
#define QOBUZAUTOSAVE_H

#include <functional>

class QObject;
class Ui_QobuzSettingsPage;

void ConnectQobuzAutoSave(QObject *context, Ui_QobuzSettingsPage *ui, const std::function<void()> &save);

#endif  // QOBUZAUTOSAVE_H
