#ifndef QOBUZSERVICE_TEST_H
#define QOBUZSERVICE_TEST_H

#include <QObject>
#include <QUrl>

class QobuzTestBrowser : public QObject {
  Q_OBJECT

 public:
  QUrl url;

 public Q_SLOTS:
  void Open(const QUrl &value) { url = value; }
};

#endif  // QOBUZSERVICE_TEST_H
