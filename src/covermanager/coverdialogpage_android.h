// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COVERDIALOGPAGE_ANDROID_H
#define COVERDIALOGPAGE_ANDROID_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QString>

class QDialog;
class QLabel;
class QPixmap;
class QWidget;

// Hosts the existing cover editor as an opaque page, retaining its controls and result semantics.
class CoverDialogPage : public QObject {
 public:
  explicit CoverDialogPage(QDialog *dialog);

  int Exec();
  void Open();
  void ShowError(const QString &message);
  static void ShowCover(QWidget *parent, const QString &title, const QPixmap &pixmap);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  void Restore();
  void UpdateGeometry();

  QDialog *dialog_;
  QPointer<QWidget> owner_;
  QPointer<QWidget> window_;
  QPointer<QWidget> host_;
  QPointer<QWidget> focus_;
  QList<QPointer<QWidget>> hidden_;
  QLabel *title_;
  QLabel *error_;
};

#endif
