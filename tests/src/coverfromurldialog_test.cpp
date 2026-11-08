/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <memory>

#include "gtest_include.h"

#include <QList>
#include <QPointer>
#include <QByteArray>
#include <QBuffer>
#include <QString>
#include <QImage>
#include <QColor>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "covermanager/coverfromurldialog.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// A reply which is finished by the test.
class ControlledNetworkReply : public QNetworkReply {
 public:
  explicit ControlledNetworkReply(const QNetworkRequest &network_request, QObject *parent = nullptr) : QNetworkReply(parent), pos_(0), aborted_(false) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
  }

  // Aborting finishes the reply with an error, like a real reply.
  void abort() override {
    aborted_ = true;
    if (isFinished()) return;
    setError(QNetworkReply::OperationCanceledError, u"Operation canceled"_s);
    setFinished(true);
    Q_EMIT finished();
  }

  void Finish(const QByteArray &data) {
    data_ = data;
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setFinished(true);
    Q_EMIT finished();
  }

  bool aborted() const { return aborted_; }
  qint64 bytesAvailable() const override { return data_.size() - pos_ + QNetworkReply::bytesAvailable(); }

 protected:
  qint64 readData(char *data, const qint64 maxlen) override {
    const qint64 len = qMin(maxlen, data_.size() - pos_);
    if (len <= 0) return -1;
    memcpy(data, data_.constData() + pos_, static_cast<size_t>(len));
    pos_ += len;
    return len;
  }

 private:
  QByteArray data_;
  qint64 pos_;
  bool aborted_;
};

class ControlledNetworkAccessManager : public NetworkAccessManager {
 public:
  QList<QPointer<ControlledNetworkReply>> replies;

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {
    Q_UNUSED(op)
    Q_UNUSED(outgoing_data)
    ControlledNetworkReply *reply = new ControlledNetworkReply(network_request, this);
    replies << reply;
    return reply;
  }
};

QByteArray PngImageData() {

  QImage image(4, 4, QImage::Format_RGB32);
  image.fill(QColor(Qt::red));
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return data;

}

class CoverFromURLDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_ = make_shared<ControlledNetworkAccessManager>();
    dialog_ = std::make_unique<CoverFromURLDialog>(network_);
    QLineEdit *url = dialog_->findChild<QLineEdit*>(u"url"_s);
    ASSERT_NE(nullptr, url);
    url->setText(u"https://example.com/cover.png"_s);
    QDialogButtonBox *button_box = dialog_->findChild<QDialogButtonBox*>(u"buttonBox"_s);
    ASSERT_NE(nullptr, button_box);
    button_ok_ = button_box->button(QDialogButtonBox::Ok);
    button_cancel_ = button_box->button(QDialogButtonBox::Cancel);
    ASSERT_NE(nullptr, button_ok_);
    ASSERT_NE(nullptr, button_cancel_);
  }

  SharedPtr<ControlledNetworkAccessManager> network_;
  std::unique_ptr<CoverFromURLDialog> dialog_;
  QPushButton *button_ok_ = nullptr;
  QPushButton *button_cancel_ = nullptr;
};

// Regression test: Cancel didn't abort the download, and when it finished later, the dialog was accepted with the cover, so the dialog opened again returned it.
TEST_F(CoverFromURLDialogTest, CancelAbortsDownload) {

  button_ok_->click();
  ASSERT_EQ(1, network_->replies.count());
  QPointer<ControlledNetworkReply> reply = network_->replies.first();

  button_cancel_->click();
  ASSERT_TRUE(reply);
  EXPECT_TRUE(reply->aborted());
  EXPECT_EQ(QDialog::Rejected, dialog_->result());

  // A download which is not aborted finishes later, the dialog is not accepted.
  if (!reply->isFinished()) reply->Finish(PngImageData());
  EXPECT_EQ(QDialog::Rejected, dialog_->result());

}

// Pressing OK again aborts the download from before, only the cover from the last URL is used.
TEST_F(CoverFromURLDialogTest, OkAgainAbortsPreviousDownload) {

  button_ok_->click();
  button_ok_->click();
  ASSERT_EQ(2, network_->replies.count());
  QPointer<ControlledNetworkReply> first_reply = network_->replies.first();
  QPointer<ControlledNetworkReply> second_reply = network_->replies.last();
  ASSERT_TRUE(first_reply);
  ASSERT_TRUE(second_reply);
  EXPECT_TRUE(first_reply->aborted());

  if (!first_reply->isFinished()) first_reply->Finish(PngImageData());
  EXPECT_EQ(QDialog::Rejected, dialog_->result());

  second_reply->Finish(PngImageData());
  EXPECT_EQ(QDialog::Accepted, dialog_->result());

}

// Destroying the dialog aborts the download, the reply is owned by the network access manager, which lives on.
TEST_F(CoverFromURLDialogTest, DestroyAbortsDownload) {

  button_ok_->click();
  ASSERT_EQ(1, network_->replies.count());
  QPointer<ControlledNetworkReply> reply = network_->replies.first();

  dialog_.reset();
  ASSERT_TRUE(reply);
  EXPECT_TRUE(reply->aborted());

}

}  // namespace
