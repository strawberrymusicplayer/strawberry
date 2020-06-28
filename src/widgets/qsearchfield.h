#ifndef QSEARCHFIELD_H
#define QSEARCHFIELD_H

#include <QWidget>
#include <QPointer>
#include <QMenu>

class QSearchFieldPrivate;
class QSearchField : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
  Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)

 public:
  explicit QSearchField(QWidget *parent);

  void setIconSize(const int iconsize);

  QString text() const;
  QString placeholderText() const;
  void setFocus(Qt::FocusReason);

 public slots:
  void setText(const QString &text);
  void setPlaceholderText(const QString &text);
  void clear();
  void selectAll();
  void setFocus();

 signals:
  void textChanged(const QString &text);
  void editingFinished();
  void returnPressed();

 protected:
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;

 private:
  friend class QSearchFieldPrivate;
  QPointer <QSearchFieldPrivate> pimpl;
};

#endif // QSEARCHFIELD_H
