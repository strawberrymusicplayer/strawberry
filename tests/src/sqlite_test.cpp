#include "gtest_include.h"

#include <sqlite3.h>

// clazy:excludeall=returning-void-expression

TEST(SqliteTest, CreateTableTest) {

  sqlite3 *db = nullptr;
  int rc = sqlite3_open(":memory:", &db);
  ASSERT_EQ(0, rc);

  char *errmsg = nullptr;
  rc = sqlite3_exec(db, "CREATE TABLE foo (content TEXT)", nullptr, nullptr, &errmsg);
  ASSERT_EQ(0, rc) << errmsg;

  sqlite3_close(db);

}
