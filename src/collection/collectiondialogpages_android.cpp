// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectiondialogpages_android.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "covermanager/coverdialogpage_android.h"
#include "widgets/androidwidgets.h"
#include "widgets/scrollarea_android.h"

namespace {

void ShowChoices(QComboBox *combo, const QString &title, QDialog *dialog) {

  auto *choices = new QDialog(dialog);
  choices->setAttribute(Qt::WA_DeleteOnClose);
  choices->setWindowTitle(dialog->windowTitle() + QStringLiteral(" > ") + title);
  auto *layout = new QVBoxLayout(choices);
  const AndroidWidgets::ScrollPage scroll = AndroidWidgets::CreateScrollPage(choices);
  layout->addWidget(scroll.area, 1);
  QButtonGroup *group = AndroidWidgets::AddComboChoiceRows(combo, scroll.layout);
  scroll.layout->addStretch();
  AndroidScrollArea::EnableForContents(scroll.area);
  QObject::connect(group, &QButtonGroup::idClicked, choices, &QDialog::accept);
  (new CoverDialogPage(choices))->Open();

}

// Replaces the combo boxes of form layouts with value buttons below their labels; the hidden combo boxes keep the values.
void AdaptComboBoxes(QDialog *dialog) {

  for (QComboBox *combo : dialog->findChildren<QComboBox*>()) {
    auto *form = qobject_cast<QFormLayout*>(combo->parentWidget()->layout());
    if (!form) continue;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    auto *label = qobject_cast<QLabel*>(form->labelForField(combo));
    const QString title = label ? label->text().remove(QLatin1Char('&')) : combo->accessibleName();
    auto *button = new AndroidWidgets::ValueButton(QString(), combo->parentWidget());
    button->setAccessibleName(title);
    delete form->replaceWidget(combo, button);
    combo->hide();
    if (label) {
      label->setWordWrap(true);
      label->setBuddy(button);
    }
    button->SetValue(combo->currentText());
    QObject::connect(combo, &QComboBox::currentTextChanged, button, [button](const QString &text) { button->SetValue(text); });
    QObject::connect(button, &QPushButton::clicked, dialog, [combo, title, dialog]() { ShowChoices(combo, title, dialog); });
  }

}

}  // namespace

namespace CollectionDialogPages {

void Show(QDialog *dialog) {

  // CoverDialogPage has no meta-object of its own, so findChild() would match any child object.
  CoverDialogPage *page = nullptr;
  for (QObject *child : dialog->children()) {
    if ((page = dynamic_cast<CoverDialogPage*>(child))) break;
  }
  if (!page) {
    AdaptComboBoxes(dialog);
    page = new CoverDialogPage(dialog);
  }
  page->Open();

}

}  // namespace CollectionDialogPages
