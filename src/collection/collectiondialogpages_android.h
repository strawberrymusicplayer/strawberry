// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLLECTIONDIALOGPAGES_ANDROID_H
#define COLLECTIONDIALOGPAGES_ANDROID_H

#include <QString>

class QDialog;
class QWidget;

// Shows the collection's grouping dialogs as pages in the current sidebar tab instead of desktop dialogs.
namespace CollectionDialogPages {

// Opens the dialog as a page; combo boxes become labelled value buttons that open their choices as a page.
void Show(QDialog *dialog);

// Like QInputDialog::getText(), but shown as a page; returns an empty string when cancelled.
QString GetText(QWidget *parent, const QString &title, const QString &label);

}  // namespace CollectionDialogPages

#endif  // COLLECTIONDIALOGPAGES_ANDROID_H
