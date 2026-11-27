// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCROLLAREA_ANDROID_H
#define SCROLLAREA_ANDROID_H

class QAbstractScrollArea;

namespace AndroidScrollArea {

// Scrolls the area by dragging anywhere in it, including its buttons, which still activate when tapped.
// The gesture is grabbed on the viewport, which stays in place: a scroller on a widget that moves with the scrolled contents follows its own movement and scrolls only about half as far as the finger.
// Drags that start on the contents, nested text views or buttons reach the viewport's gesture as well.
void EnableForContents(QAbstractScrollArea *area);

}  // namespace AndroidScrollArea

#endif
