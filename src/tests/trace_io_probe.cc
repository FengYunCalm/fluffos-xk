#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <new>
#include <string>
#include <thread>

#include <event2/event.h>

#include "backend.h"
#include "base/internal/tracing.h"
#include "mainlib.h"
#include "test_mudlib.h"
#include "vm/vm.h"

namespace {
std::thread::id main_thread;
std::atomic<bool> fail_worker_allocation{false};
bool main_thread_reported = false;
}

extern "C" {
void* __real__Znwm(size_t size);
void* __wrap__Znwm(size_t size) {
  if (fail_worker_allocation.load() && std::this_thread::get_id() != main_thread &&
      fail_worker_allocation.exchange(false)) {
    throw std::bad_alloc();
  }
  return __real__Znwm(size);
}

void __real__Z13debug_messagePKcz(const char*, ...);
void __wrap__Z13debug_messagePKcz(const char* format, ...) {
  char message[1024];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (std::this_thread::get_id() != main_thread) {
    std::fputs("TRACE_OFF_MAIN_LOG\n", stderr);
  } else if (std::strstr(format, "Dump trace successfully")) {
    main_thread_reported = true;
  }
  __real__Z13debug_messagePKcz("%s", message);
}
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  main_thread = std::this_thread::get_id();
  const std::string mode = argv[1];
  const auto mudlib = fluffos_test_mudlib::root_string();
  if (fluffos_test_mudlib::change_directory(mudlib) != 0) {
    return 2;
  }
  init_main("etc/config.test");
  vm_start();
  std::puts("TRACE_PROBE_READY");
  const char* filename = mode == "open" ? "." : mode == "full" ? "/dev/full" : "trace.json";
  Tracer::start(filename);
  if (mode == "json") {
    Tracer::logSimpleEvent(std::string(1, '\xff'), EventCategory::DEFAULT);
  } else {
    Tracer::logSimpleEvent("before-stop", EventCategory::DEFAULT);
  }
  if (mode == "concurrent") {
    std::thread producer([] {
      for (int i = 0; i < 2000; ++i) {
        Tracer::logSimpleEvent("producer", EventCategory::DEFAULT);
      }
    });
    for (int i = 0; i < 4; ++i) {
      const auto path = "trace-" + std::to_string(i) + ".json";
      Tracer::start(path.c_str());
      Tracer::logSimpleEvent("main", EventCategory::DEFAULT);
    }
    producer.join();
  }
  if (mode == "stop") {
    Tracer::stop();
    Tracer::logSimpleEvent("after-stop", EventCategory::DEFAULT);
  }
  if (mode == "allocation") {
    fail_worker_allocation.store(true);
  }
  if (mode != "uncollected") {
    Tracer::collect();
  }
  if (mode == "drain") {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!main_thread_reported && std::chrono::steady_clock::now() < deadline) {
      event_base_loop(g_event_base, EVLOOP_NONBLOCK);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!main_thread_reported) {
      return 3;
    }
    std::puts("TRACE_DRAINED_BEFORE_EXIT");
  }
  return 0;
}
