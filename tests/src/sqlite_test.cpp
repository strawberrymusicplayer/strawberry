#include <gtest/gtest.h>
#include <sqlite3.h>

TEST(SqliteTest, FTS5SupportEnabled) {

  sqlite3* db = nullptr;
  int rc = sqlite3_open(":memory:", &db);
  ASSERT_EQ(0, rc);

  char* errmsg = nullptr;
  rc = sqlite3_exec(db, "CREATE VIRTUAL TABLE foo USING fts5(content, TEXT, tokenize = 'unicode61 remove_diacritics 0')", nullptr, nullptr, &errmsg);
  ASSERT_EQ(0, rc) << errmsg;

  sqlite3_close(db);

}
