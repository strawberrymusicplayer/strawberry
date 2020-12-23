/*
Copyright (C) 2011 by Mike McQuaid

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

#include "qsearchfield.h"
#include "qocoa_mac.h"

#import "Foundation/NSAutoreleasePool.h"
#import "Foundation/NSNotification.h"
#import "AppKit/NSSearchField.h"

#include <QApplication>
#include <QWindow>
#include <QString>
#include <QClipboard>
#include <QBoxLayout>
#include <QKeyEvent>

class QSearchFieldPrivate : public QObject {
public:
  QSearchFieldPrivate(QSearchField *qSearchField, NSSearchField *nsSearchField)
    : QObject(qSearchField), qSearchField(qSearchField), nsSearchField(nsSearchField) {}

  void textDidChange(const QString &text) {
    if (qSearchField) emit qSearchField->textChanged(text);
  }

  void textDidEndEditing() {
    if (qSearchField)
      emit qSearchField->editingFinished();
    }

  void returnPressed() {
    if (qSearchField) {
      emit qSearchField->returnPressed();
      QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }
  }

  void keyDownPressed() {
    if (qSearchField) {
      QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }
  }

  void keyUpPressed() {
    if (qSearchField) {
      QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
      QApplication::postEvent(qSearchField, event);
    }
  }

  QPointer<QSearchField> qSearchField;
  NSSearchField *nsSearchField;

};

@interface QSearchFieldDelegate : NSObject<NSTextFieldDelegate> {
@public
  QPointer<QSearchFieldPrivate> pimpl;
}
-(void)controlTextDidChange:(NSNotification*)notification;
-(void)controlTextDidEndEditing:(NSNotification*)notification;
@end

@implementation QSearchFieldDelegate
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
  NSResponder* firstResponder = [[NSApp keyWindow] firstResponder];
  if ([firstResponder isKindOfClass:[NSText class]] && (NSSearchField*)([(NSText*)firstResponder delegate]) == self) {

    if ([event type] == NSEventTypeKeyDown && [event modifierFlags] & NSEventModifierFlagCommand) {
      QString keyString = toQString([event characters]);
      if (keyString == "a")  // Cmd+a
      {
        [self performSelector:@selector(selectText:)];
        return YES;
      }
      else if (keyString == "c")  // Cmd+c
      {
        [[self currentEditor] copy: nil];
        return YES;
      }
      else if (keyString == "v")  // Cmd+v
      {
        [[self currentEditor] paste: nil];
        return YES;
      }
      else if (keyString == "x")  // Cmd+x
      {
        [[self currentEditor] cut: nil];
        return YES;
      }
    }
  }

  return NO;
}
@end

QSearchField::QSearchField(QWidget *parent) : QWidget(parent) {

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSSearchField *search = [[QocoaSearchField alloc] init];
  QSearchFieldDelegate *delegate = [[QSearchFieldDelegate alloc] init];
  pimpl = delegate->pimpl = new QSearchFieldPrivate(this, search);
  [search setDelegate:(id<NSSearchFieldDelegate>)delegate];

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(QWidget::createWindowContainer(QWindow::fromWinId(WId(search)), this));

  setAttribute(Qt::WA_NativeWindow);
  setFixedHeight(24);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  [search release];
  [pool drain];

}

void QSearchField::setIconSize(const int iconsize) {
  Q_UNUSED(iconsize);
}

void QSearchField::setText(const QString &text) {
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

void QSearchField::setPlaceholderText(const QString &text) {
  Q_ASSERT(pimpl);
  if (!pimpl) return;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [[pimpl->nsSearchField cell] setPlaceholderString:fromQString(text)];
  [pool drain];
}

void QSearchField::clear() {
  Q_ASSERT(pimpl);
  if (!pimpl) return;
  [pimpl->nsSearchField setStringValue:@""];
  emit textChanged(QString());
}

void QSearchField::selectAll() {
  Q_ASSERT(pimpl);
  if (!pimpl) return;
  [pimpl->nsSearchField performSelector:@selector(selectText:)];
}

QString QSearchField::text() const {
  Q_ASSERT(pimpl);
  if (!pimpl) return QString();
  return toQString([pimpl->nsSearchField stringValue]);
}

QString QSearchField::placeholderText() const {
  Q_ASSERT(pimpl);
  return toQString([[pimpl->nsSearchField cell] placeholderString]);
}

void QSearchField::setFocus(Qt::FocusReason) {}

void QSearchField::setFocus() {
  setFocus(Qt::OtherFocusReason);
}

void QSearchField::resizeEvent(QResizeEvent *resizeEvent) {
  QWidget::resizeEvent(resizeEvent);
}

bool QSearchField::eventFilter(QObject *o, QEvent *e) {
  return QWidget::eventFilter(o, e);
}

