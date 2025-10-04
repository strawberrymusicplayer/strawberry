/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "gtest_include.h"

#include <vector>
#include <map>

#include <QApplication>
#include <QWidget>
#include <QTextDocument>
#include <QString>
#include <QSize>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "widgets/resizabletextedit.h"

using namespace Qt::Literals::StringLiterals;

class ResizableTextEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    widget_ = new QWidget();
    text_edit_ = new ResizableTextEdit(widget_);
    test_data_dir_ = u"tests/data/resizabletextedit"_s;
  }

  void TearDown() override {
    delete widget_;
  }

  QString LoadTextFile(const QString &filename) {
    QString filepath = test_data_dir_ + u"/in/"_s + filename;
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Failed to open test file:" << filepath;
      return QString();
    }
    return QString::fromUtf8(file.readAll());
  }

  QJsonObject LoadConfig() {
    QString filepath = test_data_dir_ + u"/test_config.json"_s;
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "Failed to open config file:" << filepath;
      return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object();
  }

  QWidget *widget_;
  ResizableTextEdit *text_edit_;
  QString test_data_dir_;
};

TEST_F(ResizableTextEditTest, InitialState) {
  EXPECT_TRUE(text_edit_->Text().isEmpty());
  EXPECT_EQ(text_edit_->wordWrapMode(), QTextOption::WrapAtWordBoundaryOrAnywhere);
}

TEST_F(ResizableTextEditTest, SetTextUpdatesContent) {
  const QString test_text = u"Test lyrics content"_s;
  text_edit_->SetText(test_text);

  EXPECT_EQ(text_edit_->Text(), test_text);
  EXPECT_EQ(text_edit_->toPlainText(), test_text);
}

TEST_F(ResizableTextEditTest, DocumentWidthRespected) {
  text_edit_->document()->setTextWidth(200);
  EXPECT_EQ(static_cast<int>(text_edit_->document()->textWidth()), 200);

  QSize hint = text_edit_->sizeHint();
  EXPECT_EQ(hint.width(), 200);
}

TEST_F(ResizableTextEditTest, EmptyTextHasMinimalHeight) {
  text_edit_->SetText(u""_s);
  text_edit_->document()->setTextWidth(200);

  QSize hint = text_edit_->sizeHint();
  EXPECT_GE(hint.height(), 10);
}

TEST_F(ResizableTextEditTest, DataDrivenTests) {
  // Load configuration
  QJsonObject config = LoadConfig();
  ASSERT_FALSE(config.isEmpty()) << "Failed to load test configuration";

  // Parse width categories
  QJsonObject test_widths = config[u"test_widths"_s].toObject();
  std::map<QString, std::vector<int>> width_categories;

  for (const QString &key : test_widths.keys()) {
    QJsonArray widths_array = test_widths[key].toArray();
    std::vector<int> widths;
    for (const QJsonValue &val : widths_array) {
      widths.push_back(val.toInt());
    }
    width_categories[key] = widths;
  }

  // Parse text files and run tests
  QJsonArray text_files = config[u"text_files"_s].toArray();

  for (const QJsonValue &file_config : text_files) {
    QJsonObject obj = file_config.toObject();
    QString filename = obj[u"file"_s].toString();
    QString width_category = obj[u"widths"_s].toString();

    // Load text content
    QString text = LoadTextFile(filename);
    ASSERT_FALSE(text.isEmpty()) << "Failed to load text file: " << filename.toStdString();

    // Get widths for this test
    std::vector<int> widths = width_categories[width_category];
    ASSERT_FALSE(widths.empty()) << "No widths found for category: " << width_category.toStdString();

    // Set text
    text_edit_->SetText(text);

    // Test each width
    int prev_height = 0;
    int prev_width = 0;

    for (int width : widths) {
      text_edit_->document()->setTextWidth(width);

      QSize doc_size = text_edit_->document()->size().toSize();
      QSize hint = text_edit_->sizeHint();

      // Basic assertions
      EXPECT_EQ(static_cast<int>(text_edit_->document()->textWidth()), width)
        << "File: " << filename.toStdString() << " at width: " << width;

      EXPECT_EQ(hint.width(), width)
        << "File: " << filename.toStdString() << " sizeHint width at: " << width;

      EXPECT_GT(doc_size.height(), 0)
        << "File: " << filename.toStdString() << " should have height at width: " << width;

      // Check inverse relationship for width changes
      if (prev_width > 0 && width < prev_width) {
        EXPECT_GE(doc_size.height(), prev_height)
          << "File: " << filename.toStdString()
          << " - height should increase when width decreases from " << prev_width << " to " << width;
      }
      else if (prev_width > 0 && width > prev_width) {
        EXPECT_LE(doc_size.height(), prev_height)
          << "File: " << filename.toStdString()
          << " - height should decrease when width increases from " << prev_width << " to " << width;
      }

      prev_height = doc_size.height();
      prev_width = width;
    }
  }
}

TEST_F(ResizableTextEditTest, ScreenResolutionsDataDriven) {
  // Load configuration
  QJsonObject config = LoadConfig();
  ASSERT_FALSE(config.isEmpty()) << "Failed to load test configuration";

  // Load medium text for testing
  QString text = LoadTextFile(u"medium_text.txt"_s);
  ASSERT_FALSE(text.isEmpty()) << "Failed to load medium_text.txt";

  text_edit_->SetText(text);

  // Parse and test screen resolutions
  QJsonArray resolutions = config[u"screen_resolutions"_s].toArray();

  for (const QJsonValue &res_val : resolutions) {
    QJsonObject res = res_val.toObject();
    QString name = res[u"name"_s].toString();
    int width = res[u"width"_s].toInt();
    int min_height = res[u"min_height"_s].toInt();

    text_edit_->document()->setTextWidth(width);
    QSize doc_size = text_edit_->document()->size().toSize();

    EXPECT_EQ(static_cast<int>(text_edit_->document()->textWidth()), width)
      << "Resolution: " << name.toStdString();

    EXPECT_GE(doc_size.height(), min_height)
      << "Resolution: " << name.toStdString()
      << " - expected height >= " << min_height << ", got " << doc_size.height();
  }
}
