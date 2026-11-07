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

#include <utility>

#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include "actiondialog.h"

namespace {

constexpr int kButtonHeight = 48;
constexpr int kIconSize = 48;
constexpr int kIconButtonSize = 64;
constexpr int kConfirmationMaximumWidth = 360;
constexpr int kConfirmationWindowMargin = 16;
const QColor kBackdropColor(0, 0, 0, 80);
const QColor kConfirmColor(0x16, 0x80, 0x3c);
const QColor kCancelColor(0xd3, 0x2f, 0x2f);

QIcon ConfirmationIcon(const bool confirm) {

  QPixmap pixmap(kIconSize, kIconSize);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(confirm ? kConfirmColor : kCancelColor, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if (confirm) {
    QPainterPath path;
    path.moveTo(9, 25);
    path.lineTo(20, 36);
    path.lineTo(39, 12);
    painter.drawPath(path);
  }
  else {
    painter.drawLine(12, 12, 36, 36);
    painter.drawLine(36, 12, 12, 36);
  }
  return QIcon(pixmap);

}

}  // namespace

ActionDialog::ActionDialog(QWidget *parent, const QString &window_title, const Style style)
    : QDialog(parent, Qt::FramelessWindowHint),
      style_(style),
      window_(parent ? parent->window() : nullptr),
      layout_(nullptr),
      confirmation_(nullptr),
      title_(nullptr),
      confirm_(nullptr),
      cancel_(nullptr) {

  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(window_title);
  // An application-modal window over the parent window receives both touch input at the right position and the Back key on Android.
  setWindowModality(Qt::ApplicationModal);
  if (window_) window_->installEventFilter(this);

  QShortcut *back = new QShortcut(this);
  back->setKeys({QKeySequence(Qt::Key_Back), QKeySequence(Qt::Key_Escape)});
  QObject::connect(back, &QShortcut::activated, this, &ActionDialog::Back);

}

ActionDialog::ActionDialog(QWidget *parent, const QString &window_title, const QString &title, const TitlePosition title_position)
    : ActionDialog(parent, window_title, Style::Page) {

  SetupPage(title, title_position);

}

ActionDialog *ActionDialog::CreateConfirmation(QWidget *parent, const QString &window_title, const QString &question, const QString &confirm_text) {

  ActionDialog *dialog = new ActionDialog(parent, window_title, Style::Confirmation);
  dialog->SetupConfirmation(question, confirm_text);
  return dialog;

}

bool ActionDialog::eventFilter(QObject *object, QEvent *event) {

  if (object == window_) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
      UpdateGeometry();
    }
    // Releasing the touch that opened the dialog from a long press reactivates the parent window, which would then receive Back.
    else if (event->type() == QEvent::WindowActivate && isVisible()) {
      activateWindow();
    }
  }
  return QDialog::eventFilter(object, event);

}

void ActionDialog::UpdateGeometry() {

  if (!window_) return;
  if (confirmation_) confirmation_->setFixedWidth(qMin(kConfirmationMaximumWidth, window_->width() - 2 * kConfirmationWindowMargin));
  setGeometry(window_->geometry());

}

void ActionDialog::SetupPage(const QString &title, const TitlePosition title_position) {

  title_text_ = title;
  title_ = new QLabel(title, this);
  title_->setTextFormat(Qt::PlainText);
  title_->setWordWrap(true);

  confirm_ = CreatePageButton(QString());
  confirm_->hide();
  QObject::connect(confirm_, &QPushButton::clicked, this, &ActionDialog::Confirm);

  layout_ = new QVBoxLayout(this);
  if (title_position == TitlePosition::Top) {
    layout_->addWidget(title_);
    layout_->addStretch();
  }
  else {
    layout_->addStretch();
    layout_->addWidget(title_);
  }
  layout_->addWidget(confirm_);
  layout_->addStretch();
  UpdateGeometry();

}

void ActionDialog::SetupConfirmation(const QString &question, const QString &confirm_text) {

  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(true);
  QPalette backdrop_palette = palette();
  backdrop_palette.setColor(QPalette::Window, kBackdropColor);
  setPalette(backdrop_palette);

  QWidget *confirmation = new QWidget(this);
  confirmation_ = confirmation;
  confirmation->setAutoFillBackground(true);
  if (parentWidget()) confirmation->setPalette(parentWidget()->palette());
  QVBoxLayout *confirmation_layout = new QVBoxLayout(confirmation);
  confirmation_layout->setContentsMargins(20, 20, 20, 16);
  confirmation_layout->setSpacing(16);

  title_text_ = question;
  title_ = new QLabel(question, confirmation);
  title_->setTextFormat(Qt::PlainText);
  title_->setWordWrap(true);
  confirmation_layout->addWidget(title_);

  cancel_ = new QPushButton(confirmation);
  cancel_->setIcon(ConfirmationIcon(false));
  cancel_->setAccessibleName(tr("Cancel"));
  cancel_->setToolTip(tr("Cancel"));
  confirm_ = new QPushButton(confirmation);
  confirm_->setIcon(ConfirmationIcon(true));
  confirm_->setAccessibleName(confirm_text);
  confirm_->setToolTip(confirm_text);

  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addStretch();
  for (QPushButton *button : {cancel_, confirm_}) {
    button->setFixedSize(kIconButtonSize, kIconButtonSize);
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setFlat(true);
    button->setAutoDefault(false);
    buttons->addWidget(button);
  }
  buttons->addStretch();
  confirmation_layout->addLayout(buttons);
  QObject::connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
  QObject::connect(confirm_, &QPushButton::clicked, this, &ActionDialog::Confirm);

  layout_ = new QVBoxLayout(this);
  layout_->addWidget(confirmation, 0, Qt::AlignCenter);
  UpdateGeometry();

}

QPushButton *ActionDialog::CreatePageButton(const QString &text) {

  QPushButton *button = new QPushButton(text, this);
  button->setMinimumHeight(kButtonHeight);
  button->setAutoDefault(false);
  return button;

}

QPushButton *ActionDialog::AddButton(const QString &text) {

  Q_ASSERT(style_ == Style::Page);
  QPushButton *button = CreatePageButton(text);
  layout_->insertWidget(layout_->indexOf(confirm_), button);
  actions_ << button;
  return button;

}

QPushButton *ActionDialog::AddCancelButton(const QString &text) {

  Q_ASSERT(style_ == Style::Page);
  cancel_ = CreatePageButton(text);
  layout_->insertWidget(layout_->indexOf(confirm_) + 1, cancel_);
  QObject::connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
  return cancel_;

}

void ActionDialog::AskConfirmation(const QString &question, const QString &confirm_text, std::function<void()> action) {

  Q_ASSERT(style_ == Style::Page);
  confirm_action_ = std::move(action);
  title_->setText(question);
  for (QPushButton *button : std::as_const(actions_)) {
    button->hide();
  }
  confirm_->setText(confirm_text);
  confirm_->show();

}

void ActionDialog::EndConfirmation() {

  confirm_action_ = nullptr;
  confirm_->hide();
  title_->setText(title_text_);
  for (QPushButton *button : std::as_const(actions_)) {
    button->show();
  }

}

void ActionDialog::Back() {

  if (is_confirming()) {
    EndConfirmation();
  }
  else {
    reject();
  }

}

void ActionDialog::Confirm() {

  if (style_ == Style::Confirmation) {
    accept();
    return;
  }

  if (!confirm_action_) return;
  // Copy the action, since it may close the dialog.
  const std::function<void()> action = confirm_action_;
  action();

}

void ActionDialog::Open() {

  show();
  raise();
  activateWindow();
  if (cancel_) {
    cancel_->setFocus(Qt::OtherFocusReason);
  }
  else {
    setFocus(Qt::OtherFocusReason);
  }

}
