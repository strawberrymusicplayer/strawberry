// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MENUPAGE_ANDROID_H
#define MENUPAGE_ANDROID_H

#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QWidget>

class QAction;
class QEvent;
class QKeyEvent;
class QLabel;
class QMenu;
class QScrollArea;
class QVBoxLayout;

// Shows a menu as full-page navigation in the current sidebar tab, instead of a desktop popup.
// Submenus with checkable actions, and runs of actions in one exclusive group, are shown as value buttons opening a subpage of choices.
// Back returns one level and closes the page from its root; choosing returns to the parent, and other actions close the page before they are triggered.
class MenuPage : public QWidget {
 public:
  explicit MenuPage(QMenu *menu, QWidget *owner);
  ~MenuPage() override;

  void Open();
  void Close();
  void Back();

  // Number of open levels, 0 while closed.
  int depth() const { return static_cast<int>(levels_.size()); }
  QString title() const;

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private:
  struct Level {
    QString title;
    // A menu level shows the menu; a choice level shows the choices.
    QPointer<QMenu> menu;
    QList<QPointer<QAction>> choices;
    int scroll = 0;
  };

  void Push(const Level &level);
  void ShowLevel();
  void AddMenuRows(QMenu *menu, QVBoxLayout *layout);
  void AddChoiceRows(const QList<QPointer<QAction>> &choices, QVBoxLayout *layout);
  void AddChoiceButton(const QString &title, const QList<QPointer<QAction>> &choices, bool enabled, QVBoxLayout *layout);
  void TriggerAndClose(QAction *action);

  QPointer<QMenu> menu_;
  QPointer<QWidget> owner_;
  QPointer<QWidget> host_;
  QPointer<QWidget> focus_;
  QList<QPointer<QWidget>> hidden_;
  QMetaObject::Connection tab_clicked_;
  QString parent_title_;
  QList<Level> levels_;
  QLabel *title_;
  QVBoxLayout *layout_;
  QPointer<QScrollArea> area_;
};

#endif  // MENUPAGE_ANDROID_H
