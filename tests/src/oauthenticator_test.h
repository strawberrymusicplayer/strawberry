#ifndef OAUTHENTICATOR_TEST_H
#define OAUTHENTICATOR_TEST_H

#include <QObject>
#include <QUrl>

class OAuthTestBrowser : public QObject {
  Q_OBJECT

 public:
  QUrl url;

 public Q_SLOTS:
  void Open(const QUrl &value) { url = value; }
};

#endif  // OAUTHENTICATOR_TEST_H
