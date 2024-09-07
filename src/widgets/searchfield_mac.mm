/*
Copyright (C) 2011 by Mike McQuaid
Copyright (C) 2018-2024 by Jonas Kvinge <jonas@jkvinge.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#import "Foundation/NSAutoreleasePool.h"
#import "Foundation/NSNotification.h"
#import "AppKit/NSSearchField.h"

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QWindow>
#include <QString>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QKeyEvent>

#include "searchfield.h"
#include "qocoa_mac.h"

using namespace Qt::Literals::StringLiterals;

class SearchFieldPrivate : public QObject {

 public:

  explicit SearchFieldPrivate(SearchField *qSearchField, NSSearchField *nsSearchField)
    : QObject(qSearchField), qSearchField(qSearchField), nsSearchField(nsSearchField) {}

  void textDidChange(const QString &text) {

    if (qSearchField) {
      Q_EMIT qSearchField->textChanged(text);
    }

  }

  void textDidEndEditing() {

    if (qSearchField) {
      Q_EMIT qSearchField->editingFinished();
    }

  }

  void returnPressed() {

    if (qSearchField) {
      Q_EMIT qSearchField->returnPressed();
      QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }

  }

  void keyDownPressed() {

    if (qSearchField) {
      QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }

  }

  void keyUpPressed() {

    if (qSearchField) {
      QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }

  }

  QPointer<SearchField> qSearchField;
  NSSearchField *nsSearchField;
};

@interface SearchFieldDelegate : NSObject<NSTextFieldDelegate> {
@public
  QPointer<SearchFieldPrivate> pimpl;
}
-(void)controlTextDidChange:(NSNotification*)notification;
-(void)controlTextDidEndEditing:(NSNotification*)notification;
@end

@implementation SearchFieldDelegate
-(void)controlTextDidChange:(NSNotification*)notification {

  Q_ASSERT(pimpl);

  if (pimpl) pimpl->textDidChange(toQString([[notification object] stringValue]));

}

-(void)controlTextDidEndEditing:(NSNotification*)notification {

  Q_UNUSED(notification);
  // No Q_ASSERT here as it is called on destruction.
  if (!pimpl) return;
  pimpl->textDidEndEditing();
  if ([[[notification userInfo] objectForKey:@"NSTextMovement"] intValue] == NSReturnTextMovement)
    pimpl->returnPressed();

}

-(BOOL)control: (NSControl*)control textView: (NSTextView*)textView doCommandBySelector: (SEL)commandSelector {

  Q_UNUSED(control);
  Q_UNUSED(textView);
  Q_ASSERT(pimpl);

  if (!pimpl) return NO;
  if (commandSelector == @selector(moveDown:)) {
    pimpl->keyDownPressed();
    return YES;
  }
  else if (commandSelector == @selector(moveUp:)) {
    pimpl->keyUpPressed();
    return YES;
  }

  return NO;

}

@end

@interface QocoaSearchField : NSSearchField
-(BOOL)performKeyEquivalent:(NSEvent*)event;
@end

@implementation QocoaSearchField
-(BOOL)performKeyEquivalent:(NSEvent*)event {

  // First, check if we have the focus.
  // If no, it probably means this event isn't for us.
  NSResponder *firstResponder = [[NSApp keyWindow] firstResponder];
  if ([firstResponder isKindOfClass:[NSText class]] && reinterpret_cast<NSSearchField*>([reinterpret_cast<NSText*>(firstResponder) delegate]) == self) {

    if ([event type] == NSEventTypeKeyDown && [event modifierFlags] & NSEventModifierFlagCommand) {
      const QString keyString = toQString([event characters]);
      if (keyString == "a"_L1) {  // Cmd+a
        [self performSelector:@selector(selectText:)];
        return YES;
      }
      else if (keyString == "c"_L1) {  // Cmd+c
        [[self currentEditor] copy: nil];
        return YES;
      }
      else if (keyString == "v"_L1) {  // Cmd+v
        [[self currentEditor] paste: nil];
        return YES;
      }
      else if (keyString == "x"_L1) {  // Cmd+x
        [[self currentEditor] cut: nil];
        return YES;
      }
    }
  }

  return NO;

}
@end

SearchField::SearchField(QWidget *parent) : QWidget(parent) {

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSSearchField *search = [[QocoaSearchField alloc] init];
  SearchFieldDelegate *delegate = [[SearchFieldDelegate alloc] init];
  pimpl = delegate->pimpl = new SearchFieldPrivate(this, search);
  [search setDelegate:static_cast<id<NSSearchFieldDelegate>>(delegate)];

  setAttribute(Qt::WA_NativeWindow);
  setFixedHeight(24);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  new QVBoxLayout(this);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(QWidget::createWindowContainer(QWindow::fromWinId(WId(pimpl->nsSearchField)), this));

  [pool drain];

}

void SearchField::setIconSize(const int iconsize) {
  Q_UNUSED(iconsize);
}

void SearchField::setText(const QString &text) {

  Q_ASSERT(pimpl);

  if (!pimpl) return;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [pimpl->nsSearchField setStringValue:fromQString(text)];
  if (!text.isEmpty()) {
    [pimpl->nsSearchField selectText:pimpl->nsSearchField];
    [[pimpl->nsSearchField currentEditor] setSelectedRange:NSMakeRange([[pimpl->nsSearchField stringValue] length], 0)];
  }

  [pool drain];

}

void SearchField::setPlaceholderText(const QString &text) {

  Q_ASSERT(pimpl);

  if (!pimpl) return;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [[pimpl->nsSearchField cell] setPlaceholderString:fromQString(text)];

  [pool drain];

}

void SearchField::clear() {

  Q_ASSERT(pimpl);

  if (!pimpl) return;

  [pimpl->nsSearchField setStringValue:@""];

  Q_EMIT textChanged(QString());

}

void SearchField::selectAll() {

  Q_ASSERT(pimpl);

  if (!pimpl) return;
  [pimpl->nsSearchField performSelector:@selector(selectText:)];

}

QString SearchField::text() const {

  Q_ASSERT(pimpl);

  if (!pimpl) return QString();
  return toQString([pimpl->nsSearchField stringValue]);

}

QString SearchField::placeholderText() const {

  Q_ASSERT(pimpl);

  return toQString([[pimpl->nsSearchField cell] placeholderString]);

}

void SearchField::setFocus(Qt::FocusReason) {}

void SearchField::setFocus() {
  setFocus(Qt::OtherFocusReason);
}

void SearchField::resizeEvent(QResizeEvent *resizeEvent) {
  QWidget::resizeEvent(resizeEvent);
}

bool SearchField::eventFilter(QObject *o, QEvent *e) {
  return QWidget::eventFilter(o, e);
}
