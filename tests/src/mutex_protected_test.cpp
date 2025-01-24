#include "gtest_include.h"

#include "includes/mutex_protected.h"

TEST(MutexProtectedTest, Test1) {

  mutex_protected<int> value1 = 0;
  mutex_protected<int> value2 = 0;

  EXPECT_EQ(value1, 0);
  EXPECT_EQ(value2, 0);
  EXPECT_EQ(value1, value2);
  EXPECT_TRUE(value1 == value2);
  EXPECT_FALSE(value1 != value2);
  EXPECT_FALSE(value1 > value2);
  EXPECT_FALSE(value1 < value2);
  EXPECT_TRUE(value1 >= value2);
  EXPECT_TRUE(value1 <= value2);

  ++value2;

  EXPECT_EQ(value1, 0);
  EXPECT_EQ(value2, 1);
  EXPECT_LE(value1, value2);
  EXPECT_FALSE(value1 == value2);
  EXPECT_TRUE(value1 != value2);
  EXPECT_FALSE(value1 > value2);
  EXPECT_TRUE(value1 < value2);
  EXPECT_FALSE(value1 >= value2);
  EXPECT_TRUE(value1 <= value2);

  value1 += 5;

  EXPECT_EQ(value1, 5);
  EXPECT_EQ(value2, 1);
  EXPECT_GT(value1, value2);
  EXPECT_FALSE(value1 == value2);
  EXPECT_TRUE(value1 != value2);
  EXPECT_TRUE(value1 > value2);
  EXPECT_FALSE(value1 < value2);
  EXPECT_TRUE(value1 >= value2);
  EXPECT_FALSE(value1 <= value2);

}

TEST(MutexProtectedTest, Test2) {

  mutex_protected<int> value1 = 0;
  int value2 = 0;

  EXPECT_EQ(value1, 0);
  EXPECT_EQ(value2, 0);
  EXPECT_EQ(value1, value2);
  EXPECT_TRUE(value1 == value2);
  EXPECT_FALSE(value1 != value2);
  EXPECT_FALSE(value1 > value2);
  EXPECT_FALSE(value1 < value2);
  EXPECT_TRUE(value1 >= value2);
  EXPECT_TRUE(value1 <= value2);

  ++value2;

  EXPECT_EQ(value1, 0);
  EXPECT_EQ(value2, 1);
  EXPECT_LE(value1, value2);
  EXPECT_FALSE(value1 == value2);
  EXPECT_TRUE(value1 != value2);
  EXPECT_FALSE(value1 > value2);
  EXPECT_TRUE(value1 < value2);
  EXPECT_FALSE(value1 >= value2);
  EXPECT_TRUE(value1 <= value2);

  value1 += 5;

  EXPECT_EQ(value1, 5);
  EXPECT_EQ(value2, 1);
  EXPECT_GT(value1, value2);
  EXPECT_FALSE(value1 == value2);
  EXPECT_TRUE(value1 != value2);
  EXPECT_TRUE(value1 > value2);
  EXPECT_FALSE(value1 < value2);
  EXPECT_TRUE(value1 >= value2);
  EXPECT_FALSE(value1 <= value2);

}
