// SPDX-License-Identifier: GPL-3.0-or-later

#include "coverdialogpage_android.h"

#include <utility>
#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QInputMethod>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QKeyEvent>
#include <QTabWidget>

#include "widgets/actiondialog.h"
#include "widgets/fancytabwidget.h"

namespace {

constexpr int kTouchSize = 48;

class CoverImage : public QWidget {
 public:
  CoverImage(const QPixmap &pixmap, QWidget *parent) : QWidget(parent), pixmap_(pixmap) {
    setObjectName(QStringLiteral("full_cover_image"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
    setAccessibleName(QObject::tr("Album cover"));
  }

 protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    const QSize size = pixmap_.size().scaled(this->size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(target, pixmap_);
  }

 private:
  QPixmap pixmap_;
};

}  // namespace

CoverDialogPage::CoverDialogPage(QDialog *dialog)
    : QObject(dialog),
      dialog_(dialog),
      owner_(dialog->parentWidget()),
      window_(dialog->parentWidget() ? dialog->parentWidget()->window() : nullptr),
      title_(new QLabel(dialog)),
      error_(new QLabel(dialog)) {
  auto *layout = qobject_cast<QVBoxLayout*>(dialog->layout());
  Q_ASSERT(layout);
  layout->setSizeConstraint(QLayout::SetNoConstraint);
  dialog->setMinimumSize(0, 0);
  dialog->setAutoFillBackground(true);

  QWidget *buttons = dialog->findChild<QDialogButtonBox*>();
  if (buttons) {
    auto *body = new QWidget(dialog);
    auto *body_layout = new QVBoxLayout;
    body_layout->setContentsMargins(0, 0, 0, 0);
    while (QLayoutItem *item = layout->takeAt(0)) {
      if (item->widget() == buttons) delete item;
      else if (QLayout *fields = item->layout()) {
        fields->setParent(nullptr);
        body_layout->addLayout(fields);
      }
      else body_layout->addItem(item);
    }
    body->setLayout(body_layout);
    auto *scroll = new QScrollArea(dialog);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);
  }
  title_->setTextFormat(Qt::PlainText);
  title_->setWordWrap(true);
  QFont font = title_->font();
  font.setBold(true);
  title_->setFont(font);
  auto *back = new QPushButton(tr("Back"), dialog);
  back->setMinimumHeight(kTouchSize);
  back->setAutoDefault(false);
  QObject::connect(back, &QPushButton::clicked, dialog, &QDialog::reject);
  auto *header = new QHBoxLayout;
  header->addWidget(title_, 1);
  header->addWidget(back);
  layout->insertLayout(0, header);

  error_->setObjectName(QStringLiteral("cover_error"));
  error_->setTextFormat(Qt::PlainText);
  error_->setWordWrap(true);
  layout->insertWidget(1, error_);
  error_->hide();
  if (auto *label = dialog->findChild<QLabel*>(QStringLiteral("urlLabel"))) label->setWordWrap(true);
  if (auto *fields = dialog->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout"))) {
    if (dialog->findChild<QWidget*>(QStringLiteral("artist"))) fields->setDirection(QBoxLayout::TopToBottom);
  }
  for (QPushButton *button : dialog->findChildren<QPushButton*>()) {
    button->setMinimumHeight(kTouchSize);
    button->setAutoDefault(false);
  }
  for (QLineEdit *edit : dialog->findChildren<QLineEdit*>()) {
    edit->setMinimumHeight(kTouchSize);
    edit->setMinimumWidth(0);
  }
  QObject::connect(dialog, &QDialog::finished, this, [this]() { Restore(); });
  QObject::connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged, this, [this]() { UpdateGeometry(); });
  QObject::connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this]() { UpdateGeometry(); });
  dialog->installEventFilter(this);
}

void CoverDialogPage::Open() {
  if (dialog_->isVisible()) return;
  focus_ = QApplication::focusWidget();
  host_ = window_;
  QString parent_title;
  if (window_) {
    if (auto *tabs = window_->findChild<FancyTabWidget*>(); tabs && tabs->currentWidget() && tabs->isVisible()) {
      host_ = tabs->currentWidget();
      parent_title = tabs->tabText(tabs->currentIndex()).remove(QLatin1Char('&'));
    }
    // Cover actions remain the parent level while a cover editor is open.
    for (ActionDialog *actions : window_->findChildren<ActionDialog*>()) {
      if (actions->isVisible() && actions->title_label()) {
        host_ = actions;
        parent_title = actions->title_label()->text();
        break;
      }
    }
  }
  title_->setText(parent_title.isEmpty() ? dialog_->windowTitle() : parent_title + QStringLiteral(" >\n") + dialog_->windowTitle());
  dialog_->setParent(host_, host_ ? Qt::Widget : Qt::Dialog | Qt::FramelessWindowHint);
  dialog_->setWindowModality(host_ ? Qt::NonModal : Qt::ApplicationModal);
  dialog_->setResult(QDialog::Rejected);
  error_->hide();
  if (host_) {
    host_->installEventFilter(this);
    for (QWidget *child : host_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
      if (child != dialog_ && !child->isWindow() && !child->isHidden()) hidden_.append(child);
    }
    for (const QPointer<QWidget> &child : std::as_const(hidden_)) {
      if (child) child->hide();
    }
  }
  UpdateGeometry();
  dialog_->show();
  dialog_->raise();
  dialog_->activateWindow();
  dialog_->setFocus(Qt::OtherFocusReason);
}

int CoverDialogPage::Exec() {
  QEventLoop loop;
  const QMetaObject::Connection connection = QObject::connect(dialog_, &QDialog::finished, &loop, &QEventLoop::quit);
  Open();
  loop.exec(QEventLoop::DialogExec);
  QObject::disconnect(connection);
  return dialog_->result();
}

void CoverDialogPage::Restore() {
  if (host_) host_->removeEventFilter(this);
  for (const QPointer<QWidget> &child : std::as_const(hidden_)) {
    if (child) child->show();
  }
  hidden_.clear();
  host_.clear();
  // Keep reusable editors out of Android's native window and touch-coordinate handling.
  dialog_->setParent(owner_, Qt::Widget);
  QGuiApplication::inputMethod()->hide();
  if (focus_ && focus_->isVisible()) focus_->setFocus(Qt::OtherFocusReason);
  focus_.clear();
}

void CoverDialogPage::UpdateGeometry() {
  if (!host_) return;
  QRect available = host_->rect();
  const QInputMethod *input = QGuiApplication::inputMethod();
  if (input->isVisible() && !input->keyboardRectangle().isEmpty()) {
#ifdef Q_OS_ANDROID
    // Qt's Android input context reports native screen pixels, not logical window coordinates.
    const int keyboard_top = host_->mapFromGlobal((input->keyboardRectangle().topLeft() / host_->devicePixelRatioF()).toPoint()).y();
#else
    const int keyboard_top = host_->mapFrom(host_->window(), input->keyboardRectangle().topLeft().toPoint()).y();
#endif
    if (keyboard_top > 0) available.setHeight(qMin(available.height(), keyboard_top));
  }
  dialog_->setGeometry(available);
}

bool CoverDialogPage::eventFilter(QObject *object, QEvent *event) {
  if (object == host_) {
    if (event->type() == QEvent::Resize) UpdateGeometry();
    else if (event->type() == QEvent::Hide) dialog_->reject();
  }
  else if (object == dialog_ && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
    auto *key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Back || key->key() == Qt::Key_Escape) {
      event->accept();
      if (event->type() == QEvent::KeyPress) dialog_->reject();
      return true;
    }
  }
  return QObject::eventFilter(object, event);
}

void CoverDialogPage::ShowError(const QString &message) {
  error_->setText(message);
  error_->setVisible(!message.isEmpty());
}

void CoverDialogPage::ShowCover(QWidget *parent, const QString &title, const QPixmap &pixmap) {
  auto *dialog = new QDialog(parent);
  dialog->setWindowTitle(title);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  auto *layout = new QVBoxLayout(dialog);
  layout->addWidget(new CoverImage(pixmap, dialog), 1);
  auto *page = new CoverDialogPage(dialog);
  page->Open();
}
