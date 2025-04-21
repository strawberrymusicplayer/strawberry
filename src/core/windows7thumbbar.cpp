/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QAction>
#include <QTimer>

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>

extern HICON qt_pixmapToWinHICON(const QPixmap &p);

#include "core/logging.h"
#include "windows7thumbbar.h"

namespace {
constexpr int kIconSize = 16;
constexpr int kMaxButtonCount = 7;
}  // namespace

Windows7ThumbBar::Windows7ThumbBar(QWidget *widget)
    : QObject(widget),
      widget_(widget),
      timer_(new QTimer(this)),
      button_created_message_id_(0),
      taskbar_list_(nullptr) {

  timer_->setSingleShot(true);
  timer_->setInterval(300);
  QObject::connect(timer_, &QTimer::timeout, this, &Windows7ThumbBar::ActionChanged);

}

void Windows7ThumbBar::SetActions(const QList<QAction*> &actions) {

  qLog(Debug) << "Setting actions";
  Q_ASSERT(actions.count() <= kMaxButtonCount);

  actions_ = actions;
  for (QAction *action : actions) {
    if (action) {
      QObject::connect(action, &QAction::changed, this, &Windows7ThumbBar::ActionChangedTriggered);
    }
  }
  qLog(Debug) << "Done";

}

ITaskbarList3 *Windows7ThumbBar::CreateTaskbarList() {

  ITaskbarList3 *taskbar_list = nullptr;

  // Copied from win7 SDK shobjidl.h
  static const GUID CLSID_ITaskbarList = { 0x56FDF344, 0xFD6D, 0x11d0, { 0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90 } };

  // Create the taskbar list
  HRESULT hr = CoCreateInstance(CLSID_ITaskbarList, nullptr, CLSCTX_ALL, IID_ITaskbarList3, reinterpret_cast<void**>(&taskbar_list));
  if (hr != S_OK) {
    qLog(Warning) << "Error creating the ITaskbarList3 interface" << Qt::hex << DWORD(hr);
    return nullptr;
  }

  hr = taskbar_list->HrInit();
  if (hr != S_OK) {
    qLog(Warning) << "Error initializing taskbar list" << Qt::hex << DWORD(hr);
    taskbar_list->Release();
    return nullptr;
  }

  return taskbar_list;

}

void Windows7ThumbBar::SetupButton(const QAction *action, THUMBBUTTON *button) {

  if (action) {
    button->hIcon = qt_pixmapToWinHICON(action->icon().pixmap(kIconSize));
    button->dwFlags = action->isEnabled() ? THBF_ENABLED : THBF_DISABLED;
    // This is unsafe - doesn't obey 260-char restriction
    action->text().toWCharArray(button->szTip);
    button->szTip[action->text().length()] = L'\0';

    if (!action->isVisible()) {
      button->dwFlags = THUMBBUTTONFLAGS(button->dwFlags | THBF_HIDDEN);
    }
    button->dwMask = THUMBBUTTONMASK(THB_ICON | THB_TOOLTIP | THB_FLAGS);
  }
  else {
    button->hIcon = nullptr;
    button->szTip[0] = L'\0';
    button->dwFlags = THBF_NOBACKGROUND;
    button->dwMask = THUMBBUTTONMASK(THB_FLAGS);
  }

}

void Windows7ThumbBar::HandleWinEvent(MSG *msg) {

  if (button_created_message_id_ == 0) {
    // Compute the value for the TaskbarButtonCreated message
    button_created_message_id_ = RegisterWindowMessageA(LPCSTR("TaskbarButtonCreated"));
    qLog(Debug) << "TaskbarButtonCreated message ID registered" << button_created_message_id_;
  }

  if (msg->message == button_created_message_id_) {

    if (taskbar_list_) {
      taskbar_list_->Release();
    }

    taskbar_list_ = CreateTaskbarList();
    if (!taskbar_list_) return;

    // Add the buttons
    THUMBBUTTON buttons[kMaxButtonCount];
    for (int i = 0; i < actions_.count(); ++i) {
      const QAction *action = actions_[i];
      THUMBBUTTON *button = &buttons[i];
      button->iId = static_cast<UINT>(i);
      SetupButton(action, button);
    }

    qLog(Debug) << "Adding" << actions_.count() << "buttons";
    HRESULT hr = taskbar_list_->ThumbBarAddButtons(reinterpret_cast<HWND>(widget_->winId()), static_cast<UINT>(actions_.count()), buttons);
    if (hr != S_OK) {
      qLog(Debug) << "Failed to add buttons" << Qt::hex << DWORD(hr);
    }

    for (int i = 0; i < actions_.count(); ++i) {
      if (buttons[i].hIcon) {
        DestroyIcon(buttons[i].hIcon);
      }
    }

  }
  else if (msg->message == WM_COMMAND) {
    const int button_id = LOWORD(msg->wParam);

    if (button_id >= 0 && button_id < actions_.count()) {
      if (actions_[button_id]) {
        qLog(Debug) << "Button activated";
        actions_[button_id]->activate(QAction::Trigger);
      }
    }
  }

}

void Windows7ThumbBar::ActionChangedTriggered() {
  if (!timer_->isActive()) timer_->start();
}

void Windows7ThumbBar::ActionChanged() {

  if (!taskbar_list_) return;

  qLog(Debug) << "Updating" << actions_.count() << "buttons";

  THUMBBUTTON buttons[kMaxButtonCount];
  for (int i = 0; i < actions_.count(); ++i) {
    QAction *action = actions_[i];
    THUMBBUTTON *button = &buttons[i];

    button->iId = static_cast<UINT>(i);
    SetupButton(action, button);
  }

  HRESULT hr = taskbar_list_->ThumbBarUpdateButtons(reinterpret_cast<HWND>(widget_->winId()), static_cast<UINT>(actions_.count()), buttons);
  if (hr != S_OK) {
    qLog(Debug) << "Failed to update buttons" << Qt::hex << DWORD(hr);
  }

  for (int i = 0; i < actions_.count(); ++i) {
    if (buttons[i].hIcon) {
      DestroyIcon(buttons[i].hIcon);
    }
  }

}
