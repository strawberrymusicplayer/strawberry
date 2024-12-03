#ifndef FILECHOOSERWIDGET_H
#define FILECHOOSERWIDGET_H

#include <QWidget>

class QLineEdit;
class QHBoxLayout;

class FileChooserWidget : public QWidget {
  Q_OBJECT

 public:
  enum class Mode { File, Directory };

 public:
  explicit FileChooserWidget(QWidget *parent = nullptr);
  explicit FileChooserWidget(const Mode mode, const QString& initial_path = QString(), QWidget *parent = nullptr);
  explicit FileChooserWidget(const Mode mode, const QString& label, const QString &initial_path = QString(), QWidget *parent = nullptr);
  ~FileChooserWidget() = default;

  void SetFileFilter(const QString &file_filter);

  void SetPath(const QString &path);
  QString Path() const;

 public Q_SLOTS:
  void ChooseFile();

 private:
  void Init(const QString &initial_path = QString());

 private:
  QHBoxLayout *layout_;
  QLineEdit *path_edit_;
  const Mode mode_;
  QString file_filter_;
  QString open_dir_path_;
};

#endif  // FILECHOOSERWIDGET_H
