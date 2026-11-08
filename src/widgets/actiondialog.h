/*
 * Strawberry Music Player
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

#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include "config.h"

#include <functional>

#include <QDialog>
#include <QList>
#include <QString>

class QBoxLayout;
class QLabel;
class QPushButton;

// A touch dialog for action and confirmation flows, used on Android.
// Android delivers offset touch coordinates to small positioned native dialogs, so the dialog is never a small window:
// an action page inside a tab covers that tab's page as a child widget, leaving the tab bar usable, and switching tabs cancels it;
// otherwise the action page, like the translucent overlay around a compact confirmation, is a frameless modal window that covers the parent's window.
// Back and Escape reject the dialog, or return from a confirmation step to the actions.
// The dialog deletes itself when closed.
class ActionDialog : public QDialog {
  Q_OBJECT

 public:
  enum class TitlePosition {
    Top,
    AboveButtons
  };

  // Creates an action page; add its buttons with AddButton() and AddCancelButton().
  explicit ActionDialog(QWidget *parent, const QString &window_title, const QString &title, const TitlePosition title_position);

  // Creates a compact confirmation over the parent's window with a cancel cross and a confirm checkmark, which accepts the dialog.
  // The confirm text is the accessible name of the checkmark.
  static ActionDialog *CreateConfirmation(QWidget *parent, const QString &window_title, const QString &question, const QString &confirm_text);

  QLabel *title_label() const { return title_; }
  QPushButton *confirm_button() const { return confirm_; }
  QPushButton *cancel_button() const { return cancel_; }
  bool is_confirming() const { return static_cast<bool>(confirm_action_); }

  // Adds an action button to the page, above the confirm and cancel buttons.
  QPushButton *AddButton(const QString &text);

  // Adds the button that rejects the page; it stays visible during a confirmation step.
  QPushButton *AddCancelButton(const QString &text);

  // Replaces the page's actions with the question and a confirm button that runs the action; Back returns to the actions.
  void AskConfirmation(const QString &question, const QString &confirm_text, std::function<void()> action);

  // Opens the dialog without blocking and gives it focus, so that Back and Escape reach it.
  void Open();

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  enum class Style {
    Page,
    Confirmation
  };

  explicit ActionDialog(QWidget *parent, const QString &window_title, const Style style);

  void SetupPage(const QString &title, const TitlePosition title_position);
  void SetupConfirmation(const QString &question, const QString &confirm_text);
  QPushButton *CreatePageButton(const QString &text);
  // Covers the host and fits the confirmation into it.
  void UpdateGeometry();
  void Back();
  void EndConfirmation();
  void Confirm();

  Style style_;
  // The tab page or window that the dialog covers.
  QWidget *host_;
  QBoxLayout *layout_;
  QWidget *confirmation_;
  QLabel *title_;
  QPushButton *confirm_;
  QPushButton *cancel_;
  QList<QPushButton*> actions_;
  QString title_text_;
  std::function<void()> confirm_action_;
};

#endif  // ACTIONDIALOG_H
