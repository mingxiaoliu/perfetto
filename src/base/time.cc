/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/base/time.h"

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"

#include <atomic>

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace perfetto {
namespace base {

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

static uint64_t CachedPerformanceCounterFrequency() {
  static std::atomic<uint64_t> freq(0);
  auto current = freq.load(std::memory_order_relaxed);
  if (current != 0) {
    return current;
  }
  LARGE_INTEGER query_freq;
  ::QueryPerformanceFrequency(&query_freq);

  current = static_cast<uint64_t>(query_freq.QuadPart);
  freq.store(current, std::memory_order_relaxed);

  return current;
}

static uint64_t mul_div_u64(uint64_t value, uint64_t numer, uint64_t denom) {
  uint64_t q = value / denom;
  uint64_t r = value % denom;
  return q * numer + r * numer / denom;
}

TimeNanos GetWallTimeNs() {
  uint64_t freq = CachedPerformanceCounterFrequency();
  LARGE_INTEGER counter;
  ::QueryPerformanceCounter(&counter);
  return TimeNanos(
      mul_div_u64(static_cast<uint64_t>(counter.QuadPart), 1e9, freq));
}

TimeNanos GetThreadCPUTimeNs() {
  FILETIME dummy, kernel_ftime, user_ftime;
  ::GetThreadTimes(GetCurrentThread(), &dummy, &dummy, &kernel_ftime,
                   &user_ftime);
  uint64_t kernel_time =
      kernel_ftime.dwHighDateTime * 0x100000000 + kernel_ftime.dwLowDateTime;
  uint64_t user_time =
      user_ftime.dwHighDateTime * 0x100000000 + user_ftime.dwLowDateTime;

  return TimeNanos((kernel_time + user_time) * 100);
}

void SleepMicroseconds(unsigned interval_us) {
  // The Windows Sleep function takes a millisecond count. Round up so that
  // short sleeps don't turn into a busy wait. Note that the sleep granularity
  // on Windows can dynamically vary from 1 ms to ~16 ms, so don't count on this
  // being a short sleep.
  ::Sleep(static_cast<DWORD>((interval_us + 999) / 1000));
}

#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

void SleepMicroseconds(unsigned interval_us) {
  ::usleep(static_cast<useconds_t>(interval_us));
}

#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

std::string GetTimeFmt(const std::string& fmt) {
  time_t raw_time;
  time(&raw_time);
  struct tm* local_tm;
  local_tm = localtime(&raw_time);
  char buf[128];
  PERFETTO_CHECK(strftime(buf, 80, fmt.c_str(), local_tm) > 0);
  return buf;
}

}  // namespace base
}  // namespace perfetto
