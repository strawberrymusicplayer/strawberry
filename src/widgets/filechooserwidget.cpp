#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "filechooserwidget.h"

using namespace Qt::Literals::StringLiterals;

FileChooserWidget::FileChooserWidget(QWidget *parent)
    : QWidget(parent),
      layout_(new QHBoxLayout(this)),
      path_edit_(new QLineEdit(this)),
      mode_(Mode::Directory) {

  Init();

}

FileChooserWidget::FileChooserWidget(const Mode mode, const QString &initial_path, QWidget* parent)
    : QWidget(parent),
      layout_(new QHBoxLayout(this)),
      path_edit_(new QLineEdit(this)),
      mode_(mode) {

  Init(initial_path);

}

FileChooserWidget::FileChooserWidget(const Mode mode, const QString &label, const QString &initial_path, QWidget* parent)
    : QWidget(parent),
      layout_(new QHBoxLayout(this)),
      path_edit_(new QLineEdit(this)),
      mode_(mode) {

  layout_->addWidget(new QLabel(label, this));

  Init(initial_path);

}

void FileChooserWidget::SetFileFilter(const QString &file_filter) {
  file_filter_ = file_filter;
}

void FileChooserWidget::SetPath(const QString &path) {

  QFileInfo fi(path);
  if (fi.exists()) {
    path_edit_->setText(path);
    open_dir_path_ = fi.absolutePath();
  }

}

QString FileChooserWidget::Path() const {

  QString path(path_edit_->text());
  QFileInfo fi(path);
  if (!fi.exists()) return QString();
  if (mode_ == Mode::File) {
    if (!fi.isFile()) return QString();
  }
  else {
    if (!fi.isDir()) return QString();
  }

  return path;

}

void FileChooserWidget::Init(const QString &initial_path) {

  QFileInfo fi(initial_path);
  if (fi.exists()) {
    path_edit_->setText(initial_path);
    open_dir_path_ = fi.absolutePath();
  }
  layout_->addWidget(path_edit_);

  QPushButton* changePath = new QPushButton(QLatin1String("..."), this);
  connect(changePath, &QAbstractButton::clicked, this, &FileChooserWidget::ChooseFile);
  changePath->setFixedWidth(2 * changePath->fontMetrics().horizontalAdvance(" ... "_L1));

  layout_->addWidget(changePath);
  layout_->setContentsMargins(2, 0, 2, 0);

  setFocusProxy(path_edit_);

}

void FileChooserWidget::ChooseFile() {

  QString new_path;

  if (mode_ == Mode::File) {
    new_path = QFileDialog::getOpenFileName(this, tr("Select a file"), open_dir_path_, file_filter_);
  }
  else {
    new_path = QFileDialog::getExistingDirectory(this, tr("Select a directory"), open_dir_path_);
  }

  if (!new_path.isEmpty()) {
    QFileInfo fi(new_path);
    open_dir_path_ = fi.absolutePath();
    if (mode_ == Mode::File) {
      path_edit_->setText(fi.absoluteFilePath());
    }
    else {
      path_edit_->setText(fi.absoluteFilePath() + u"/"_s);
    }
  }

}
