/*
 * Copyright 2017 Discord, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef DISCORD_BACKOFF_H
#define DISCORD_BACKOFF_H

#include <algorithm>
#include <random>
#include <cstdint>
#include <ctime>

namespace discord_rpc {

struct Backoff {
  int64_t minAmount;
  int64_t maxAmount;
  int64_t current;
  int fails;
  std::mt19937_64 randGenerator;
  std::uniform_real_distribution<> randDistribution;

  double rand01() { return randDistribution(randGenerator); }

  Backoff(int64_t min, int64_t max)
      : minAmount(min), maxAmount(max), current(min), fails(0), randGenerator(static_cast<uint64_t>(time(0))) {
  }

  void reset() {
    fails = 0;
    current = minAmount;
  }

  int64_t nextDelay() {
    ++fails;
    int64_t delay = static_cast<int64_t>(static_cast<double>(current) * 2.0 * rand01());
    current = std::min(current + delay, maxAmount);
    return current;
  }
};

}  // namespace discord_rpc

#endif  // DISCORD_BACKOFF_H
