#include <functional>

#include "gtest_include.h"

#include <QtConcurrentRun>
#include <QThreadPool>
#include <QEventLoop>
#include <QFutureWatcher>

#include "test_utils.h"

int f();
int f() {
  return 1337;
}

TEST(ConcurrentRunTest, ConcurrentRun0StartAndWait) {

  QThreadPool threadpool;
  QFuture<int> future = QtConcurrent::run(&threadpool, &f);
  QFutureWatcher<int> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(1337, watcher.result());

}

int g(int i);
int g(int i) {
  return ++i;
}

TEST(ConcurrentRunTest, ConcurrentRun1StartAndWait) {

  QThreadPool threadpool;
  int i = 1336;
  QFuture<int> future = QtConcurrent::run(&threadpool, &g, i);
  QFutureWatcher<int> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(1337, watcher.result());

}

int max(int i, int j);
int max(int i, int j) {
  return (i > j ? i : j);
}

TEST(ConcurrentRunTest, ConcurrentRun2StartAndWait) {

  int i = 10;
  int j = 42;
  QThreadPool threadpool;
  QFuture<int> future = QtConcurrent::run(&threadpool, &max, i, j);
  QFutureWatcher<int> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(42, watcher.result());

}

int sum(int a, int b, int c);
int sum(int a, int b, int c) {
  return a + b + c;
}

TEST(ConcurrentRunTest, ConcurrentRun3StartAndWait) {

  int i = 10;
  int j = 42;
  int k = 50;
  QThreadPool threadpool;
  QFuture<int> future = QtConcurrent::run(&threadpool, &sum, i, j, k);
  QFutureWatcher<int> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(102, watcher.result());

}

void aFunction(int* n);
void aFunction(int* n) {
  *n = 1337;
}

void bFunction(int* n, int *m);
void bFunction(int* n, int *m) {
  aFunction(n);
  *m = 1338;
}

void cFunction(int* n, int *m, int *o);
void cFunction(int* n, int *m, int *o) {
  bFunction(n, m);
  *o = 1339;
}

TEST(ConcurrentRunTest, ConcurrentRunVoidFunction1Start) {

  QThreadPool threadpool;

  int n = 10;
  QFuture<void> future = QtConcurrent::run(&threadpool, &aFunction, &n);
  QFutureWatcher<void> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(1337, n);

}

TEST(ConcurrentRunTest, ConcurrentRunVoidFunction2Start) {

  QThreadPool threadpool;

  int n = 10, m = 11;
  QFuture<void> future = QtConcurrent::run(&threadpool, &bFunction, &n, &m);
  QFutureWatcher<void> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(1337, n);
  EXPECT_EQ(1338, m);

}

TEST(ConcurrentRunTest, ConcurrentRunVoidFunction3Start) {

  QThreadPool threadpool;

  int n = 10, m = 11, o = 12;
  QFuture<void> future = QtConcurrent::run(&threadpool, &cFunction, &n, &m, &o);
  QFutureWatcher<void> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(1337, n);
  EXPECT_EQ(1338, m);
  EXPECT_EQ(1339, o);

}

class A {
 public:
  void f(int* i) {
    *i = *i + 1;
  }
};

TEST(ConcurrentRunTest, ConcurrentRunVoidBindFunctionStart) {

  QThreadPool threadpool;

  A a;
  int nb = 10;
  QFuture<void> future = QtConcurrent::run(&threadpool, std::bind(&A::f, &a, &nb));
  QFutureWatcher<void> watcher;
  QEventLoop loop;
  QObject::connect(&watcher, &QFutureWatcher<int>::finished, &loop, &QEventLoop::quit);
  watcher.setFuture(future);
  loop.exec();
  EXPECT_EQ(11, nb);

}
