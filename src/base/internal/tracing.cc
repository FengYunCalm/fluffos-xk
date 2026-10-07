// Yucong Sun (sunyucong@gmail.com)
//
// Generate driver tracing data to be viewed in chrome http://about:tracing

#include <vector>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <sys/types.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "tracing.h"

#include "base/internal/log.h"
#include "backend.h"
#include <cstdio>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
const int MAX_EVENTS = 1'000'000;
std::mutex timestamp_mutex;

unsigned long get_current_process_id() {
  static unsigned long const current_process_id =
#ifdef _WIN32
      GetCurrentProcessId();
#else
      ::getpid();
#endif
  return current_process_id;
}

unsigned long thread_id_to_string(std::thread::id id) {
  std::ostringstream os;
  os << id;
  std::string const res = os.str();

  return std::strtoul(res.c_str(), nullptr, 10);
}

unsigned long get_current_thread_id() {
  static thread_local unsigned long const current_thread_id =
      thread_id_to_string(std::this_thread::get_id());
  return current_thread_id;
}

}  // namespace

Event::Event(std::string_view name, EventCategory category, const char* phase,
             std::optional<json>&& args)
    : process_id(::get_current_process_id()),
      thread_id(::get_current_thread_id()),
      timestamp(Tracer::timestamp()),
      category(category),
      phase(phase),
      name(name),
      args(args) {}

class TraceWriter {
 public:
  ~TraceWriter();

  void log(Event&& e) {
    std::lock_guard<std::mutex> const guard(lock_);
    if (!Tracer::enabled()) {
      return;
    }
    if (!buffer_) {
      buffer_ = std::make_unique<std::vector<Event>>();
      buffer_->reserve(MAX_EVENTS);
    }
    if (buffer_->size() >= MAX_EVENTS) {
      Tracer::stop();
    }
    buffer_->push_back(std::move(e));
  }
  void flush(const std::string& file);
  void drain(bool wait = false);

 private:
  struct DumpJob {
    std::string filename;
    std::thread thread;
    std::atomic<bool> done{false};
    bool success = false;
    long long milliseconds = 0;
    const char* operation = "opening";
    char error[192]{};
  };

  std::mutex lock_;
  std::unique_ptr<std::vector<Event>> buffer_;
  // Only the main thread changes this list; each worker owns its result until done.
  std::vector<std::unique_ptr<DumpJob>> dumps_;
};

void TraceWriter::drain(bool wait) {
  for (auto it = dumps_.begin(); it != dumps_.end();) {
    auto& job = **it;
    if (!wait && !job.done.load(std::memory_order_acquire)) {
      ++it;
      continue;
    }
    job.thread.join();
    if (job.success) {
      debug_message("Dump trace successfully to file %s, cost %lld ms.\n",
                    job.filename.c_str(), job.milliseconds);
    } else {
      debug_message("Error %s trace file %s: %s.\n", job.operation,
                    job.filename.c_str(), job.error);
    }
    it = dumps_.erase(it);
  }
}

TraceWriter::~TraceWriter() {
  Tracer::stop();
  drain(true);
  std::lock_guard<std::mutex> const lock(lock_);
  if (buffer_ && !buffer_->empty()) {
    debug_message("Uncollected profiling events: %zu.\n", buffer_->size());
  }
}

void TraceWriter::flush(const std::string& filename) {
  drain();
  std::unique_ptr<std::vector<Event>> events;
  {
    std::lock_guard<std::mutex> const guard(lock_);
    if (!buffer_ || buffer_->empty()) {
      return;
    }
    events = std::move(buffer_);
  }
  debug_message("Trace duration: %lf us, dumping %zu events to %s in separate thread.\n",
                Tracer::timestamp(), events->size(), filename.c_str());
  try {
    auto job = std::make_unique<DumpJob>();
    job->filename = filename;
    auto* result = job.get();
    // Register before launch: vector growth must not destroy a joinable thread.
    dumps_.push_back(std::move(job));
    try {
      result->thread = std::thread([result, events = std::move(events)]() mutable {
        const auto begin = std::chrono::high_resolution_clock::now();
        try {
          std::ofstream file;
          file.exceptions(std::ios::failbit | std::ios::badbit);
          file.open(result->filename, std::ofstream::out | std::ofstream::binary);
          result->operation = "writing";
          file << "[";
          bool first = true;
          for (auto& event : *events) {
            if (!first) {
              file << ",";
            }
            first = false;
            file << "\n{"
                 << R"("pid":)" << event.process_id << ","
                 << R"("tid":)" << event.thread_id << ","
                 << R"("ts":)" << event.timestamp << ","
                 << R"("dur":)" << event.duration << ","
                 << R"("ph":")" << event.phase << "\","
                 << R"("cat":")" << event.category_name() << "\","
                 << R"("name":)" << json(event.name);
            if (event.phase[0] == 'X') {
              file << "," << R"("dur":)" << event.duration;
            }
            if (event.args && !event.args->empty()) {
              file << "," << R"("args":)" << *event.args;
            }
            file << "}";
          }
          file << "\n]";
          result->operation = "closing";
          file.close();
          result->success = true;
        } catch (const std::exception& error) {
          std::snprintf(result->error, sizeof(result->error), "%s", error.what());
        } catch (...) {
          std::snprintf(result->error, sizeof(result->error), "Unknown exception");
        }
        result->milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::high_resolution_clock::now() - begin).count();
        events.reset();
        result->done.store(true, std::memory_order_release);
        backend_wakeup_event_loop();
      });
    } catch (...) {
      dumps_.pop_back();
      throw;
    }
  } catch (const std::exception& error) {
    debug_message("Error starting trace dump to %s: %s.\n", filename.c_str(), error.what());
  }
}

std::atomic<bool> Tracer::is_enabled{false};
std::string Tracer::filename;

#ifdef _WIN32
LARGE_INTEGER Tracer::basetime;
#else
std::chrono::high_resolution_clock::time_point Tracer::basetime;
#endif

void Tracer::start(const char* file) {
  collect();
  {
    std::lock_guard<std::mutex> lock(timestamp_mutex);
#ifdef _WIN32
    QueryPerformanceCounter(&basetime);
#else
    basetime = std::chrono::high_resolution_clock::now();
#endif
  }
  filename = file;
  is_enabled.store(true, std::memory_order_relaxed);
}

double Tracer::timestamp() {
  std::lock_guard<std::mutex> lock(timestamp_mutex);
#ifdef _WIN32
  static LARGE_INTEGER frequency{};
  if (frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
  }
  LARGE_INTEGER ending_time;
  QueryPerformanceCounter(&ending_time);
  uint64_t elapsed = ending_time.QuadPart - basetime.QuadPart;
  elapsed *= 1000000;
  return elapsed / frequency.QuadPart;
#else
  return std::chrono::duration<double, std::micro>(
             std::chrono::high_resolution_clock::now() - basetime).count();
#endif
}

void Tracer::drain() {
  instance().drain();
}

void Tracer::log(Event&& e) {
  if (Tracer::enabled()) {
    instance().log(std::move(e));
  }
}

void Tracer::logSimpleEvent(const std::string_view& name, const EventCategory& category) {
  if (Tracer::enabled()) {
    log({name, category, "i"});
  }
}
void Tracer::begin(const std::string_view& name, const EventCategory& category, json&& args) {
  if (Tracer::enabled()) {
    log({name, category, "B", std::move(args)});
  }
}
void Tracer::begin(const std::string_view& name, const EventCategory& category) {
  if (Tracer::enabled()) {
    log({name, category, "B"});
  }
}
void Tracer::end(const std::string_view& name, const EventCategory& category) {
  if (Tracer::enabled()) {
    log({name, category, "E"});
  }
}

void Tracer::setThreadName(const std::string_view& name) {
  if (Tracer::enabled()) {
    Event e("thread_name", EventCategory::DEFAULT, "M",
            json{
                {"name", name},
            });
    e.timestamp = 0;
    log(std::move(e));
  }
}

void Tracer::counter(const std::string_view& name, long n) {
  if (Tracer::enabled()) {
    counter(name, {{name, n}});
  }
}

void Tracer::counter(const std::string_view& name, std::optional<json>&& args) {
  if (Tracer::enabled()) {
    log({name, EventCategory::DEFAULT, "C", std::move(args)});
  }
}

void Tracer::collect() {
  // It's possible that we are over limit and collection was disabled.
  if (!filename.empty()) {
    stop();
    instance().flush(filename);
    filename.clear();
  } else {
    drain();
  }
}

TraceWriter& Tracer::instance() {
  static TraceWriter trace_writer;
  return trace_writer;
}

ScopedTracerInner::ScopedTracerInner(const std::string& name, const EventCategory category,
                                     std::optional<std::function<json()>> lazy_arg,
                                     double time_limit_usec)
    : time_limit_usec(time_limit_usec),
      event(std::make_unique<Event>(
          name, category, "X", lazy_arg ? std::make_optional<json>((*lazy_arg)()) : std::nullopt)) {
}

ScopedTracerInner::~ScopedTracerInner() {
  if (!this->event) return;

  this->event->duration = Tracer::timestamp() - this->event->timestamp;
  if (this->event->duration >= time_limit_usec) {
    Tracer::log(std::move(*this->event));
  }
}
