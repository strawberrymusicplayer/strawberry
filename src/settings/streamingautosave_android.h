// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STREAMINGAUTOSAVE_ANDROID_H
#define STREAMINGAUTOSAVE_ANDROID_H

#include <functional>

#include <QList>
#include <QPair>

class QObject;
class QWidget;
class SettingsPage;

void ConnectStreamingAutoSave(QObject *context, QWidget *page, const std::function<void()> &save);
// Also saves after page buttons that mark their page changed, and list or checkable group edits that Apply() does not compare by itself.
void ConnectSettingsPageAutoSave(QObject *context, SettingsPage *page, const std::function<void()> &save);
// Widgets of ui_widget that are shown in open subpages, given as (content, content_parent) pairs in opening order.
// A nested section's content_parent lies inside an earlier subpage's content, which is no longer inside ui_widget.
QList<QWidget*> SubpageWidgetsOf(const QWidget *ui_widget, const QList<QPair<QWidget*, QWidget*>> &subpages);
// Calls flush whenever widget is hidden. ~QDialog hides a visible dialog after its members are destroyed, so delete the returned filter before the data flush uses.
QObject *InstallFlushOnHide(QWidget *widget, const std::function<void()> &flush);

#endif  // STREAMINGAUTOSAVE_ANDROID_H
