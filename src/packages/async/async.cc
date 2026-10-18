#include "base/package_api.h"

#include "packages/async/async.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <set>  // #1247 ASYNC-1
#include <string>
#include <thread>
#include <vector>

#if HAVE_DIRENT_H
#include <dirent.h>
#else
#define dirent direct
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#include <sys/param.h>  // for MAXPATHLEN
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <vm/internal/base/function.h>
#include <vm/internal/base/interpret.h>
#include <vm/internal/base/program.h>
#include <vm/internal/base/promise.h>
#include <vm/internal/base/object.h>

#include "vm/context.h"
#include "vm/owner.h"

#ifdef F_ASYNC_DB_EXEC
#include "packages/db/db.h"
#endif

#include "packages/core/file.h"  // check_valid_path, FIXME

namespace {

enum atypes { AREAD, AWRITE, AGETDIR, ADBEXEC, ADONE };

enum astates { BUSY, DONE };

struct Request;
void free_async_request(Request* req);
void free_async_request_cleanup(void* context) {
  free_async_request(static_cast<Request*>(context));
}
std::mutex reqs_lock;
std::set<struct Request*> live_requests;

struct Request {
  Request()
      : flags(0),
        ret(0),
        handle(0),
        db_generation(0),
        fun(nullptr),
        bound_args(nullptr),
        promise(nullptr),
        owner(nullptr),
        failed(false),
        charged_bytes(0),
        admitted(false),
        next(nullptr),
        type(AREAD),
        status(BUSY) {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    live_requests.insert(this);
  }

  std::string path;
  std::string owner_id;
  uint64_t owner_epoch{0};
  int flags;
  int ret;
  int handle;
  uint64_t db_generation;
  std::string data;
  function_to_call_t* fun;
  /* Bound args trailing the callback (async_db_exec's spec allows a trailing
     `...`), heap-copied off the VM stack -- mirrors call_out()'s cop->vs
     (call_out.cc), since this callback fires long after the registering stack
     frame is gone. fun->args/fun->narg point into this array; null when there
     are no bound args. */
  array_t* bound_args;
  promise_t* promise;
  object_t* owner;
  bool failed;
  size_t charged_bytes;
  bool admitted;
  struct Request* next;
  enum atypes type;
  int status;
  VMOwnerCallbackCleanupRecord cleanup_record;
};

struct Work {
  struct Request* data;
  void* (*func)(struct Request*);
};

std::deque<struct Work*> reqs;

// #1247 ASYNC-2..4: works a worker thread is CURRENTLY processing: popped from
// reqs but not yet moved to finished_reqs. Guarded by reqs_lock so
// async_mark_request() (main thread, via check_memory) can account for their
// callback funptr during that window -- otherwise a DEBUGMALLOC
// sweep that lands mid-processing false-flags the ref.
std::set<struct Work*> current_works;
std::condition_variable reqs_cv;
std::vector<std::thread> workers;
size_t idle_worker_count = 0;
size_t live_worker_count = 0;
bool stop_workers = false;
size_t pending_request_bytes = 0;

std::deque<struct Request*> finished_reqs;
std::mutex finished_reqs_lock;

// complete_all_asyncio() is a synchronous main-thread barrier used by tests
// and shutdown. While it drains callbacks, dispatch them through the main
// queue instead of an owner executor: the barrier cannot wait for an owner
// task by blocking the same main thread that must admit its follow-up work.
std::atomic<bool> completion_barrier_active{false};

class CompletionBarrierScope {
 public:
  CompletionBarrierScope()
      : previous_(completion_barrier_active.exchange(true, std::memory_order_acq_rel)) {}
  ~CompletionBarrierScope() {
    completion_barrier_active.store(previous_, std::memory_order_release);
  }

 private:
  bool previous_;
};

class ControlledLpcScope {
 public:
  ControlledLpcScope() : previous_(vm_context().owner.controlled_lpc_active) {
    vm_context().owner.controlled_lpc_active = true;
  }
  ~ControlledLpcScope() {
    vm_context().owner.controlled_lpc_active = previous_;
  }

 private:
  bool previous_;
};

object_t* callback_owner(function_to_call_t* fun) {
  if (!fun) {
    return nullptr;
  }
  return fun->ob ? fun->ob : fun->f.fp ? fun->f.fp->hdr.owner : nullptr;
}

object_t* request_owner(Request* req) {
  return req->fun ? callback_owner(req->fun) : req->owner;
}

void bind_request_owner(Request* req) {
  auto* owner = request_owner(req);
  req->owner_id = vm_owner_id(owner);
  req->owner_epoch = vm_owner_epoch(owner);
  req->cleanup_record.prepare(req->owner_id.c_str(), req->owner_epoch, "async_callback",
                              "<pending>", free_async_request_cleanup, req);
}

svalue_t* safe_call_async_callback(Request* req, int narg, const char* operation) {
  auto* owner = callback_owner(req->fun);
  if (owner && (owner->flags & O_DESTRUCTED)) {
    vm_owner_record_task_trace(req->owner_id.c_str(), "async_callback", operation, req->owner_epoch,
                               "destructed");
    pop_n_elems(narg);
    return nullptr;
  }
  if (owner && (req->owner_id != vm_owner_id(owner) || req->owner_epoch != vm_owner_epoch(owner))) {
    vm_owner_record_task_trace(vm_owner_id(owner), "async_callback", operation,
                               vm_owner_epoch(owner), "stale");
    pop_n_elems(narg);
    return nullptr;
  }
  VMOwnerScope owner_scope(vm_context(), owner ? vm_owner_id(owner) : req->owner_id.c_str(),
                           owner ? vm_owner_epoch(owner) : req->owner_epoch);
  vm_owner_record_task_trace(owner ? vm_owner_id(owner) : req->owner_id.c_str(), "async_callback",
                             operation, owner ? vm_owner_epoch(owner) : req->owner_epoch,
                             "dispatched");
  ControlledLpcScope controlled_lpc;
  return safe_call_efun_callback(req->fun, narg);
}

const char* async_operation_name(enum atypes type) {
  switch (type) {
    case AREAD:
      return "async_read";
    case AWRITE:
      return "async_write";
#ifdef F_ASYNC_GETDIR
    case AGETDIR:
      return "async_getdir";
#endif
#ifdef F_ASYNC_DB_EXEC
    case ADBEXEC:
      return "async_db_exec";
#endif
    case ADONE:
    default:
      return "async_done";
  }
}

void free_async_request(Request* req) {
  if (!req) {
    return;
  }
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    if (req->admitted) {
      DEBUG_CHECK(req->charged_bytes > pending_request_bytes,
                  "async request byte accounting underflow");
      pending_request_bytes -= req->charged_bytes;
      req->charged_bytes = 0;
      req->admitted = false;
    }
    live_requests.erase(req);
  }
  if (req->fun) {
    free_funp(req->fun->f.fp);
    delete req->fun;
  }
  if (req->bound_args) {
    free_array(req->bound_args);
  }
  if (req->promise) {
    free_promise(req->promise);
  }
  if (req->owner) {
    free_object(&req->owner, "async request owner");
  }
  delete req;
}

void cleanup_async_request(Request* req, const char* operation, bool main_required) {
  if (!req) {
    return;
  }
  if (!main_required || vm_context_is_main_thread()) {
    free_async_request(req);
    return;
  }
  req->cleanup_record.task_key = operation ? operation : "";
  (void)vm_owner_enqueue_executor_callback_cleanup(&req->cleanup_record);
}

void publish_finished(Request* req) {
  {
    std::lock_guard<std::mutex> const lock(finished_reqs_lock);
    finished_reqs.push_back(req);
  }
  backend_wakeup_event_loop();
}

void reject_request(Request* req, const char* reason) {
#ifdef F_ASYNC_GETDIR
  req->ret = req->type == AGETDIR ? 0 : -1;
#else
  req->ret = -1;
#endif
  req->failed = true;
  req->path = reason ? reason : "async request rejected";
  req->status = DONE;
  publish_finished(req);
}

bool admit_request(Request* req, size_t payload_bytes, size_t path_bytes) {
  constexpr size_t kMax = std::numeric_limits<size_t>::max();
  if (payload_bytes > kMax - path_bytes) {
    reject_request(req, "async request byte budget exceeded");
    return false;
  }
  auto const charge = payload_bytes + path_bytes;
  bool accepted = false;
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    auto const max_requests = CONFIG_INT(__RC_MAX_ASYNC_REQUESTS__);
    auto const max_bytes = CONFIG_INT(__RC_MAX_ASYNC_PENDING_BYTES__);
    accepted = !stop_workers &&
               live_requests.size() <= static_cast<size_t>(max_requests > 0 ? max_requests : 1) &&
               charge <= static_cast<size_t>(max_bytes > 0 ? max_bytes : 1) &&
               pending_request_bytes <= static_cast<size_t>(max_bytes > 0 ? max_bytes : 1) - charge;
    if (accepted) {
      req->charged_bytes = charge;
      req->admitted = true;
      pending_request_bytes += charge;
    }
  }
  if (!accepted) {
    reject_request(req, "async request queue or byte budget exceeded");
  }
  return accepted;
}

bool grow_request_data(Request* req, size_t new_size) {
  auto const old_size = req->data.size();
  if (new_size <= old_size) {
    return true;
  }
  auto const additional = new_size - old_size;
  bool reserved = false;
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    auto const max_bytes = CONFIG_INT(__RC_MAX_ASYNC_PENDING_BYTES__);
    auto const limit = static_cast<size_t>(max_bytes > 0 ? max_bytes : 1);
    reserved = req->admitted && additional <= limit && pending_request_bytes <= limit - additional;
    if (reserved) {
      req->charged_bytes += additional;
      pending_request_bytes += additional;
    }
  }
  if (!reserved) {
    return false;
  }
  try {
    req->data.resize(new_size);
  } catch (...) {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    req->charged_bytes -= additional;
    pending_request_bytes -= additional;
    return false;
  }
  return true;
}

void mark_worker_failure(Request* req) {
#ifdef F_ASYNC_GETDIR
  req->ret = req->type == AGETDIR ? 0 : -1;
#else
  req->ret = -1;
#endif
  req->failed = true;
  req->status = DONE;
}

void thread_func() {
  Tracer::setThreadName("Package Async thread");
  ScopedTracer const tracer("Async thread loop");

  while (true) {
    struct Work* w = nullptr;
    {
      std::unique_lock<std::mutex> lock(reqs_lock);
      ++idle_worker_count;
      reqs_cv.wait(lock, [] {
        return stop_workers || !reqs.empty();
      });
      --idle_worker_count;
      if (reqs.empty()) {
        if (stop_workers) {
          --live_worker_count;
          return;
        }
        continue;
      }
      w = reqs.front();
      reqs.pop_front();
      current_works.insert(w);  // #1247 ASYNC-3: in-flight: keep it accountable
    }

    try {
      ScopedTracer const work_tracer("Async thread work", EventCategory::DEFAULT, [=] {
        return json{{"type", w->data->type}};
      });
      w->func(w->data);
    } catch (...) {
      // A backend exception must still produce the operation's normal failure
      // result and release the request through the main-thread delivery path.
      mark_worker_failure(w->data);
    }

    if (w->data->status == DONE) {
      {
        std::lock_guard<std::mutex> const lock(reqs_lock);
        std::lock_guard<std::mutex> const flock(finished_reqs_lock);
        current_works.erase(w);  // #1247 ASYNC-4
        finished_reqs.push_back(w->data);
        delete w;
      }
    } else {
      std::lock_guard<std::mutex> const lock(reqs_lock);
      current_works.erase(w);  // #1247 ASYNC-4
      reqs.push_back(w);
      reqs_cv.notify_one();
    }

    // P7-2: worker completion wakes the main loop through the backend
    // self-pipe; it never creates a libevent event off the main thread.
    backend_wakeup_event_loop();
  }
}

bool enqueue_work(void* (*func)(struct Request*), struct Request* data) {
  std::unique_ptr<Work> work;
  try {
    work = std::make_unique<Work>();
    work->func = func;
    work->data = data;
  } catch (...) {
    reject_request(data, "async request allocation failed");
    return false;
  }

  bool rejected = false;
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    if (!data->admitted || stop_workers) {
      rejected = true;
    } else {
      reqs.push_back(work.release());
      if (idle_worker_count == 0 &&
          live_worker_count < static_cast<size_t>(CONFIG_INT(__RC_MAX_ASYNC_WORKERS__) > 0
                                                      ? CONFIG_INT(__RC_MAX_ASYNC_WORKERS__)
                                                      : 1)) {
        try {
          workers.emplace_back(thread_func);
          ++live_worker_count;
        } catch (...) {
          auto* failed_work = reqs.back();
          reqs.pop_back();
          delete failed_work;
          rejected = true;
        }
      }
    }
  }
  if (rejected) {
    reject_request(data, "async worker pool unavailable");
    return false;
  }
  reqs_cv.notify_one();
  return true;
}

void* gzreadthread(struct Request* req) {
  gzFile file = gzopen(req->path.c_str(), "rb");
  if (!file) {
    req->ret = -1;
    req->status = DONE;
    return nullptr;
  }
  req->ret = gzread(file, (void*)(req->data.data()), req->data.size());
  req->status = DONE;
  gzclose(file);
  return nullptr;
}

int aio_gzread(struct Request* req) {
  req->status = BUSY;
  enqueue_work(gzreadthread, req);
  return 0;
}

void* gzwritethread(struct Request* req) {
  int const fd = open(req->path.c_str(),
                      req->flags & 1 ? O_CREAT | O_WRONLY | O_TRUNC : O_CREAT | O_WRONLY | O_APPEND,
                      S_IRWXU | S_IRWXG);
  if (fd < 0) {
    req->ret = -1;
    req->status = DONE;
    return nullptr;
  }
  gzFile file = gzdopen(fd, "wb");
  if (!file) {
    close(fd);
    req->ret = -1;
    req->status = DONE;
    return nullptr;
  }
  req->ret = gzwrite(file, (void*)(req->data.data()), req->data.size());
  req->status = DONE;
  gzclose(file);
  return nullptr;
}

int aio_gzwrite(struct Request* req) {
  req->status = BUSY;
  enqueue_work(gzwritethread, req);
  return 0;
}

void* writethread(struct Request* req) {
  int const fd = open(req->path.c_str(),
                      req->flags & 1 ? O_CREAT | O_WRONLY | O_TRUNC : O_CREAT | O_WRONLY | O_APPEND,
                      S_IRWXU | S_IRWXG);

  req->ret = fd < 0 ? -1 : write(fd, req->data.data(), req->data.size());

  req->status = DONE;
  if (fd >= 0) {
    close(fd);
  }
  return nullptr;
}

int aio_write(struct Request* req) {
  req->status = BUSY;
  enqueue_work(writethread, req);
  return 0;
}

void* readthread(struct Request* req) {
  int const fd = open(req->path.c_str(), O_RDONLY);
  if (fd < 0) {
    req->ret = -1;
    req->status = DONE;
    return nullptr;
  }
  // Bound the read by the buffer's ACTUAL size, not max_size() (~2^62) --
  // the caller sizes req->data before dispatching (mirror gzreadthread just
  // above, which correctly uses .size()). A short read shrinks the buffer;
  // a negative return leaves it untouched.
  auto size = read(fd, (void*)(req->data.data()), req->data.size());
  close(fd);
  if (size >= 0) {
    req->data.resize(size);
  }
  req->ret = size;
  req->status = DONE;
  return nullptr;
}

int aio_read(struct Request* req) {
  req->status = BUSY;
  enqueue_work(readthread, req);
  return 0;
}

}  // namespace

#ifdef F_ASYNC_DB_EXEC
void* dbexecthread(struct Request* req) {
  ScopedTracer const work_tracer("db_exec", EventCategory::DEFAULT, [=] {
    return json{req->data};
  });

  db_lock_mutex();
  DEFER {
    db_unlock_mutex();
  };
  if (!db_generation_matches(req->handle, req->db_generation)) {
    req->path = "stale database connection";
    req->ret = -1;
    req->status = DONE;
    return nullptr;
  }

  // see add_db_exec
  db_t* db = find_db_conn(req->handle);
  int ret = -1;
  if (db && db->type->execute) {
    if (db->type->cleanup) {
      db->type->cleanup(&(db->c));
    }

    ret = db->type->execute(&(db->c), req->data.c_str());
    if (ret == -1) {
      if (db->type->error) {
        char* tmp = db->type->error(&(db->c));
        req->path = std::string(tmp);
        FREE_MSTR(tmp);
      } else {
        req->path = "Unknown error";
      }
    }
  } else {
    req->path = std::string("No database exec function!");
  }

  req->ret = ret;
  req->status = DONE;
  return nullptr;
}

int aio_db_exec(struct Request* req) {
  req->status = BUSY;
  enqueue_work(dbexecthread, req);
  return 0;
}
#endif

#ifdef F_ASYNC_GETDIR
void* getdirthread(struct Request* req) {
  ScopedTracer const work_tracer("getdir", EventCategory::DEFAULT, [=] {
    return json{req->path};
  });

  DIR* dirp = nullptr;
  if ((dirp = opendir(req->path.c_str())) == nullptr) {
    req->failed = true;
    req->ret = 0;
    req->status = DONE;
    return nullptr;
  }
  /*
   * Count files. The request owns only the entries it has reserved, and the
   * configured array limit prevents an unbounded directory from consuming the
   * process-wide async byte budget.
   */
  int const max_array_size = CONFIG_INT(__MAX_ARRAY_SIZE__);
  int i = 0;
  for (auto* de = readdir(dirp); de; de = readdir(dirp)) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;
    if (i >= max_array_size) {
      break;
    }
    // #1247 ASYNC-5: the buffer is used as an array of `struct dirent`, indexed
    // by i, so it must grow by sizeof(dirent) per entry -- growing by
    // sizeof(dirent*) (a pointer, ~8 bytes) undersized it and memcpy of the
    // ~280-byte dirent overflowed the heap once a directory held enough
    // entries.
    auto const required = static_cast<size_t>(i + 1) * sizeof(struct dirent);
    if (!grow_request_data(req, required)) {
      req->failed = true;
      req->ret = 0;
      closedir(dirp);
      req->status = DONE;
      return nullptr;
    }
    memcpy(&((dirent*)(req->data.data()))[i], de, sizeof(*de));
    i++;
  }

  closedir(dirp);

  req->ret = i;
  req->status = DONE;
  return nullptr;
}

int aio_getdir(struct Request* req) {
  req->status = BUSY;
  enqueue_work(getdirthread, req);
  return 0;
}

#endif

int add_read(const char* fname, function_to_call_t* fun) {
  const auto read_file_max_size = CONFIG_INT(__MAX_READ_FILE_SIZE__);

  if (fname) {
    auto* req = new Request();
    req->fun = fun;
    req->type = AREAD;
    if (!admit_request(req, static_cast<size_t>(read_file_max_size > 0 ? read_file_max_size : 1),
                       std::strlen(fname))) {
      bind_request_owner(req);
      return 0;
    }
    // printf("fname: %s\n", fname);
    try {
      req->data.resize(read_file_max_size > 0 ? read_file_max_size : 1);
      req->path = std::string(fname);
    } catch (...) {
      reject_request(req, "async request allocation failed");
      bind_request_owner(req);
      return 0;
    }
    bind_request_owner(req);
    return aio_gzread(req);
  }
  // #1247 ASYNC-6: denied path: no request will ever be created to free the
  // callback the efun already ref-bumped and handed us, so release it before
  // unwinding.
  free_funp(fun->f.fp);
  delete fun;
  error("permission denied\n");

  return 1;
}

#ifdef F_ASYNC_GETDIR
int add_getdir(const char* fname, function_to_call_t* fun) {
  if (fname) {
    // printf("fname: %s\n", fname);
    auto* req = new Request();
    req->fun = fun;
    req->type = AGETDIR;
    if (!admit_request(req, 0, std::strlen(fname))) {
      bind_request_owner(req);
      return 0;
    }
    try {
      req->path = fname;
    } catch (...) {
      reject_request(req, "async request allocation failed");
      bind_request_owner(req);
      return 0;
    }
    bind_request_owner(req);
    return aio_getdir(req);
  }
  // #1247 ASYNC-7: denied path: free the callback the efun already ref-bumped
  // and handed us.
  free_funp(fun->f.fp);
  delete fun;
  error("permission denied\n");

  return 1;
}
#endif

int add_write(const char* fname, const char* buf, int size, char flags, function_to_call_t* fun) {
  if (!fname) {
    // #1247 ASYNC-8: denied path: free the callback the efun already
    // ref-bumped and handed us.
    free_funp(fun->f.fp);
    delete fun;
    error("permission denied\n");
  }

  auto* req = new Request();
  req->fun = fun;
  req->type = AWRITE;
  req->flags = flags;
  if (!admit_request(req, static_cast<size_t>(size > 0 ? size : 0), std::strlen(fname))) {
    bind_request_owner(req);
    return 0;
  }
  try {
    req->data = std::string(buf, size);
    req->path = std::string(fname);
  } catch (...) {
    reject_request(req, "async request allocation failed");
    bind_request_owner(req);
    return 0;
  }
  bind_request_owner(req);
  if (flags & 2) {
    return aio_gzwrite(req);
  }
  return aio_write(req);
}

#ifdef F_ASYNC_DB_EXEC
int add_db_exec(int handle, uint64_t generation, const char* sql, function_to_call_t* fun,
                array_t* bound_args) {
  auto* req = new Request();
  req->fun = fun;
  req->bound_args = bound_args;
  req->type = ADBEXEC;
  req->handle = handle;
  req->db_generation = generation;
  if (!admit_request(req, sql ? std::strlen(sql) : 0, 0)) {
    bind_request_owner(req);
    return 0;
  }
  try {
    req->data = sql ? sql : "";
  } catch (...) {
    reject_request(req, "async request allocation failed");
    bind_request_owner(req);
    return 0;
  }
  bind_request_owner(req);
  return aio_db_exec(req);
}
#endif

Request* make_promise_request(int value_type) {
  auto* req = new Request();
  req->promise = promise_alloc();
  req->promise->value_type = static_cast<unsigned short>(value_type);
  req->owner = current_object;
  if (req->owner) {
    add_ref(req->owner, "async promise request owner");
  }
  bind_request_owner(req);
  return req;
}

void settle_promise_request(Request* req, svalue_t* value, bool rejected) {
  (void)promise_settle(req->promise, value, rejected);
  /* promise_settle() borrows value and assigns its own reference. The request
   * handlers construct owned temporary strings/arrays, so release that
   * temporary on both the normal and duplicate-settlement paths. */
  free_svalue(value, "async promise settlement");
}

void handle_read(struct Request* req) {
  if (req->promise) {
    svalue_t result{};
    if (req->ret < 0) {
      result.type = T_NUMBER;
      result.u.number = req->ret;
      settle_promise_request(req, &result, true);
      return;
    }
    result.type = T_STRING;
    result.subtype = STRING_MALLOC;
    char* file = new_string(req->ret, "read_file_async_promise: str");
    memcpy(file, req->data.data(), req->ret);
    file[req->ret] = 0;
    result.u.string = file;
    settle_promise_request(req, &result, false);
    return;
  }
  int const val = req->ret;
  if (val < 0) {
    push_number(val);
    set_eval(max_eval_cost);
    safe_call_async_callback(req, 1, "async_read");
    return;
  }
  char* file = new_string(val, "read_file_async: str");
  memcpy(file, (char*)(req->data.data()), val);
  file[val] = 0;
  push_malloced_string(file);
  set_eval(max_eval_cost);
  safe_call_async_callback(req, 1, "async_read");
}

#ifdef F_ASYNC_GETDIR
void handle_getdir(struct Request* req) {
  auto max_array_size = CONFIG_INT(__MAX_ARRAY_SIZE__);

  if (req->failed && req->promise) {
    svalue_t result{};
    result.type = T_NUMBER;
    result.u.number = -1;
    settle_promise_request(req, &result, true);
    return;
  }

  int ret_size = req->ret;
  if (ret_size > max_array_size) {
    ret_size = max_array_size;
  }
  array_t* ret = allocate_empty_array(ret_size);
  if (ret_size > 0) {
    for (int i = 0; i < ret_size; i++) {
      auto de = ((struct dirent*)req->data.data())[i];
      svalue_t* vp = &(ret->item[i]);
      vp->type = T_STRING;
      vp->subtype = STRING_MALLOC;
      vp->u.string = string_copy(de.d_name, "encode_stat");
    }

    qsort((void*)ret->item, ret_size, sizeof ret->item[0],
          [](const void* p1, const void* p2) -> int {
            auto* x = (svalue_t*)p1;
            auto* y = (svalue_t*)p2;

            return strcmp(x->u.string, y->u.string);
          });
  }

  if (req->promise) {
    svalue_t result{};
    result.type = T_ARRAY;
    result.u.arr = ret;
    settle_promise_request(req, &result, false);
    return;
  }

  push_refed_array(ret);
  set_eval(max_eval_cost);
  safe_call_async_callback(req, 1, "async_getdir");
}
#endif

void handle_write(struct Request* req) {
  int const val = req->ret;
  if (req->promise) {
    svalue_t result = const0u;
    if (val < 0) {
      result.type = T_NUMBER;
      result.u.number = val;
      settle_promise_request(req, &result, true);
    } else {
      settle_promise_request(req, &result, false);
    }
    return;
  }
  if (val < 0) {
    push_number(val);
    set_eval(max_eval_cost);
    safe_call_async_callback(req, 1, "async_write");
    return;
  }
  push_undefined();
  set_eval(max_eval_cost);
  safe_call_async_callback(req, 1, "async_write");
}

void handle_db_exec(struct Request* req) {
  int const val = req->ret;
  if (req->promise) {
    svalue_t result{};
    if (val == -1) {
      result.type = T_STRING;
      result.subtype = STRING_MALLOC;
      result.u.string = string_copy(req->path.c_str(), "async_db_exec_promise: error");
      settle_promise_request(req, &result, true);
    } else {
      result.type = T_NUMBER;
      result.u.number = val;
      settle_promise_request(req, &result, false);
    }
    return;
  }
  if (val == -1) {
    copy_and_push_string(req->path.c_str());
  } else {
    push_number(val);
  }
  set_eval(max_eval_cost);
  safe_call_async_callback(req, 1, "async_db_exec");
}

void handle_finished_request(Request* req, enum atypes type) {
  req->type = ADONE;
  switch (type) {
    case AREAD:
      handle_read(req);
      break;
    case AWRITE:
      handle_write(req);
      break;
#ifdef F_ASYNC_GETDIR
    case AGETDIR:
      handle_getdir(req);
      break;
#endif
#ifdef F_ASYNC_DB_EXEC
    case ADBEXEC:
      handle_db_exec(req);
      break;
#endif
    case ADONE:
      // must have had an error while handling it before.
      break;
    default:
      fatal("unknown async type\n");
  }
}

void dispatch_finished_request(Request* req, enum atypes type) {
  auto* owner = request_owner(req);
  auto operation = std::string(async_operation_name(type));
  if (!owner || (owner->flags & O_DESTRUCTED)) {
    handle_finished_request(req, type);
    free_async_request(req);
    return;
  }

  auto executor_available = vm_owner_executor_available();
  auto use_executor =
      executor_available && !completion_barrier_active.load(std::memory_order_acquire);
  // Promise settlement queues VM microtasks and may resume an LPC coroutine;
  // keep that transition on the main/owner admission path rather than
  // mutating promise references from an executor worker.
  if (use_executor && req->promise == nullptr) {
    auto task_id = vm_owner_enqueue_executor_task(
        owner, "async_callback", operation.c_str(),
        [req, type, operation] {
          handle_finished_request(req, type);
          cleanup_async_request(req, operation.c_str(), true);
        },
        [req, operation] {
          cleanup_async_request(req, operation.c_str(), true);
        });
    if (task_id != 0) {
      return;
    }
  }

  auto task_id = vm_owner_enqueue_main_task(
      owner, "async_callback", operation.c_str(),
      [req, type] {
        handle_finished_request(req, type);
        free_async_request(req);
      },
      [req] {
        free_async_request(req);
      },
      executor_available ? VM_OWNER_MAIN_TASK_EXPLICIT_FALLBACK
                         : VM_OWNER_MAIN_TASK_OFF_MODE_FALLBACK);
  if (task_id == 0) {
    handle_finished_request(req, type);
    free_async_request(req);
  }
}

void check_reqs() {
  ScopedTracer const tracer("Async callback");

  std::deque<struct Request*> ready;
  {
    std::lock_guard<std::mutex> const lock(finished_reqs_lock);
    ready.swap(finished_reqs);
  }

  while (!ready.empty()) {
    auto* req = ready.front();
    ready.pop_front();
    auto const type = req->type;
    dispatch_finished_request(req, type);
  }
  vm_owner_drain_main_tasks(1024);
}

void complete_all_asyncio() {
  CompletionBarrierScope const barrier_scope;
  auto diagnostic_at = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  // A callback may submit another request (for example, an async read from an
  // async write callback), so one wait-and-dispatch pass is not sufficient.
  // Also account for work already popped by a worker: reqs can be empty while
  // current_works is still about to publish a finished request.
  while (true) {
    {
      std::lock_guard<std::mutex> const lock(reqs_lock);
      if (!reqs.empty() || !current_works.empty()) {
        // A pure yield is not a scheduling guarantee on Windows: the main
        // thread can repeatedly resume while the worker that owns the queued
        // request has not run yet. Sleep briefly so completion barriers cannot
        // starve their workers on that platform.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
    }

    check_reqs();

    bool pending = false;
    {
      std::lock_guard<std::mutex> const lock(reqs_lock);
      pending = !reqs.empty() || !current_works.empty();
    }
    if (!pending) {
      std::lock_guard<std::mutex> const lock(finished_reqs_lock);
      pending = !finished_reqs.empty();
    }
    {
      std::lock_guard<std::mutex> const lock(reqs_lock);
      pending = pending || !live_requests.empty() || pending_request_bytes != 0;
    }
    if (!pending && vm_owner_main_queue_total_depth() == 0) {
      break;
    }
    auto const now = std::chrono::steady_clock::now();
    if (now >= diagnostic_at) {
      size_t queued = 0;
      size_t current = 0;
      size_t live = 0;
      size_t bytes = 0;
      std::vector<std::string> paths;
      {
        std::lock_guard<std::mutex> const lock(reqs_lock);
        queued = reqs.size();
        current = current_works.size();
        live = live_requests.size();
        bytes = pending_request_bytes;
        paths.reserve(live_requests.size());
        for (auto const* req : live_requests) {
          paths.push_back(req->path);
        }
      }
      size_t finished = 0;
      {
        std::lock_guard<std::mutex> const lock(finished_reqs_lock);
        finished = finished_reqs.size();
      }
      std::fprintf(stderr, "async completion barrier pending: reqs=%zu current=%zu finished=%zu live=%zu bytes=%zu main=%lld\\n",
                   queued, current, finished, live, bytes,
                   static_cast<long long>(vm_owner_main_queue_total_depth()));
      for (auto const& path : paths) {
        std::fprintf(stderr, "async request path=%s\\n", path.c_str());
      }
      diagnostic_at = now + std::chrono::seconds(10);
    }
    // Keep the synchronous barrier cooperative on platforms where yield()
    // may immediately return without giving a worker a time slice.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void stop_async_workers() {
  // Stop only after every producer and owner callback has had a chance to
  // finish its request chain. The final drain also delivers any completion
  // published in the race between the previous drain and the stop flag.
  complete_all_asyncio();
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    stop_workers = true;
  }
  reqs_cv.notify_all();

  for (auto& worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    DEBUG_CHECK(live_worker_count != 0, "async worker count did not drain");
    workers.clear();
  }

  check_reqs();
  complete_all_asyncio();

  bool dirty = false;
  {
    std::lock_guard<std::mutex> const lock(reqs_lock);
    dirty = !reqs.empty() || !current_works.empty() || !live_requests.empty() ||
            pending_request_bytes != 0 || live_worker_count != 0;
  }
  {
    std::lock_guard<std::mutex> const lock(finished_reqs_lock);
    dirty = dirty || !finished_reqs.empty();
  }
  DEBUG_CHECK(dirty, "async shutdown left requests or workers behind");
}

int find_test_lfun_index(object_t* owner, const char* method) {
  if (!owner || !owner->prog || !method) {
    return -1;
  }
  for (int i = 0; i < owner->prog->num_functions_defined; i++) {
    auto* entry = find_func_entry(owner->prog, i);
    if (entry && entry->funcname && std::strcmp(entry->funcname, method) == 0) {
      return i;
    }
  }
  return -1;
}

function_to_call_t* make_test_lfun_callback(object_t* owner, int function_index) {
  VMExecutionState execution;
  execution.current_object = owner;
  execution.current_prog = owner ? owner->prog : nullptr;
  VMExecutionScope execution_scope(vm_context(), execution);
  svalue_t no_args{};
  no_args.type = T_NUMBER;
  auto* fun = new function_to_call_t;
  fun->ob = nullptr;
  fun->f.fp = make_lfun_funp(function_index, &no_args);
  fun->narg = 0;
  fun->args = nullptr;
  return fun;
}

bool vm_async_test_support_dispatch_read_callback(object_t* owner, const char* method,
                                                  const char* payload) {
  auto function_index = find_test_lfun_index(owner, method);
  if (function_index < 0) {
    return false;
  }
  auto* req = new Request();
  req->fun = make_test_lfun_callback(owner, function_index);
  req->type = AREAD;
  req->status = DONE;
  req->data = payload ? payload : "";
  req->ret = static_cast<int>(req->data.size());
  bind_request_owner(req);
  dispatch_finished_request(req, AREAD);
  return true;
}

#ifdef F_ASYNC_READ

void f_async_read() {
  std::unique_ptr<function_to_call_t> cb(new function_to_call_t);
  process_efun_callback(1, cb.get(), F_ASYNC_READ);
  // A `mixed` argument bypasses the spec's compile-time `function` check;
  // process_efun_callback() then sets cb->ob (string-callback form) rather than
  // cb->f.fp, and the ref++ below would type-confuse cb->f.str (a char*) as a
  // funptr_t*. The string form has no reference-counted lifetime that could
  // survive until this callback fires.
  if (cb->ob != nullptr) {
    error("async_read: callback must be a function pointer, not a string.\n");
  }
  cb->f.fp->hdr.ref++;
  pop_stack();

  add_read(check_valid_path(sp->u.string, current_object, "read_file", 0), cb.release());
  pop_stack();
}
#endif

#ifdef F_ASYNC_READ_PROMISE
void f_async_read_promise() {
  auto* req = make_promise_request(TYPE_STRING);
  req->type = AREAD;
  auto* path = check_valid_path(sp->u.string, current_object, "read_file", 0);
  bool admitted = false;
  if (path) {
    auto const size = static_cast<size_t>(
        CONFIG_INT(__MAX_READ_FILE_SIZE__) > 0 ? CONFIG_INT(__MAX_READ_FILE_SIZE__) : 1);
    admitted = admit_request(req, size, std::strlen(path));
    if (admitted) {
      try {
        req->path = path;
        req->data.resize(size);
      } catch (...) {
        reject_request(req, "async request allocation failed");
        admitted = false;
      }
    }
  }

  /* Keep one reference for the returned stack value and one for the request.
   * Start the request only after replacing the input argument so an immediate
   * fallback completion cannot free the value before it is pushed. */
  req->promise->ref++;
  pop_stack();
  push_refed_promise(req->promise);
  if (admitted) {
    aio_gzread(req);
  } else if (!path) {
    req->ret = -1;
    req->status = DONE;
    dispatch_finished_request(req, AREAD);
  }
}
#endif

#ifdef F_ASYNC_WRITE
void f_async_write() {
  std::unique_ptr<function_to_call_t> cb(new function_to_call_t);
  process_efun_callback(3, cb.get(), F_ASYNC_WRITE);
  if (cb->ob != nullptr) {
    error("async_write: callback must be a function pointer, not a string.\n");
  }
  cb->f.fp->hdr.ref++;
  pop_stack();

  add_write(check_valid_path((sp - 2)->u.string, current_object, "write_file", 1),
            (sp - 1)->u.string, SVALUE_STRLEN((sp - 1)), sp->u.number, cb.release());
  pop_3_elems();
}
#endif

#ifdef F_ASYNC_WRITE_PROMISE
void f_async_write_promise() {
  auto* req = make_promise_request(TYPE_NUMBER);
  req->type = AWRITE;
  auto* path = check_valid_path((sp - 2)->u.string, current_object, "write_file", 1);
  auto const size = SVALUE_STRLEN(sp - 1);
  auto const* input = (sp - 1)->u.string;
  req->flags = static_cast<char>(sp->u.number);
  bool admitted = false;
  if (path) {
    admitted = admit_request(req, size, std::strlen(path));
    if (admitted) {
      try {
        req->path = path;
        req->data = std::string(input, size);
      } catch (...) {
        reject_request(req, "async request allocation failed");
        admitted = false;
      }
    }
  }

  req->promise->ref++;
  pop_3_elems();
  push_refed_promise(req->promise);
  if (admitted) {
    if (req->flags & 2) {
      aio_gzwrite(req);
    } else {
      aio_write(req);
    }
  } else if (!path) {
    req->ret = -1;
    req->status = DONE;
    dispatch_finished_request(req, AWRITE);
  }
}
#endif

#ifdef F_ASYNC_GETDIR
void f_async_getdir() {
  std::unique_ptr<function_to_call_t> cb(new function_to_call_t);
  process_efun_callback(1, cb.get(), F_ASYNC_GETDIR);
  if (cb->ob != nullptr) {
    error("async_getdir: callback must be a function pointer, not a string.\n");
  }
  cb->f.fp->hdr.ref++;
  pop_stack();

  add_getdir(check_valid_path(sp->u.string, current_object, "get_dir", 0), cb.release());
  pop_stack();
}
#endif

#ifdef F_ASYNC_GETDIR_PROMISE
void f_async_getdir_promise() {
  auto* req = make_promise_request(TYPE_STRING | TYPE_MOD_ARRAY);
  req->type = AGETDIR;
  auto* path = check_valid_path(sp->u.string, current_object, "get_dir", 0);
  bool admitted = false;
  if (path) {
    admitted = admit_request(req, 0, std::strlen(path));
    if (admitted) {
      try {
        req->path = path;
      } catch (...) {
        reject_request(req, "async request allocation failed");
        admitted = false;
      }
    }
  }
  req->promise->ref++;
  pop_stack();
  push_refed_promise(req->promise);
  if (admitted) {
    aio_getdir(req);
  } else if (!path) {
    req->failed = true;
    req->status = DONE;
    dispatch_finished_request(req, AGETDIR);
  }
}
#endif

#ifdef F_ASYNC_DB_EXEC
void f_async_db_exec() {
  std::unique_ptr<function_to_call_t> cb(new function_to_call_t);
  process_efun_callback(2, cb.get(), F_ASYNC_DB_EXEC);
  if (cb->ob != nullptr) {
    error("async_db_exec: callback must be a function pointer, not a string.\n");
  }
  cb->f.fp->hdr.ref++;

  // Trailing bound args are captured by process_efun_callback() as a raw
  // pointer into the live VM stack, which is only safe for its synchronous
  // consumers (pcre, array map/filter/sort). This callback runs after a DB
  // round trip on a worker thread, so the slots have to be moved off the stack
  // into an array (the bitwise copy IS the new owning reference, matching
  // call_out()'s cop->vs).
  array_t* bound_args = nullptr;
  if (cb->narg > 0) {
    bound_args = allocate_empty_array(cb->narg);
    memcpy(bound_args->item, cb->args, sizeof(svalue_t) * cb->narg);
    cb->args = bound_args->item;
  }
  bool bound_args_handed_off = false;
  DEFER {
    if (bound_args && !bound_args_handed_off) {
      free_array(bound_args);
    }
  };

  // The bound args' stack slots have been transferred into bound_args (no ref
  // bump above), so step over them without freeing -- same "args have been
  // transfered; don't free them" idiom as int_call_out().
  sp -= cb->narg;
  pop_stack();

  array_t* info;
  info = allocate_empty_array(1);
  info->item[0].type = T_STRING;
  info->item[0].subtype = STRING_MALLOC;
  info->item[0].u.string = string_copy(sp->u.string, "f_db_exec");
  valid_database("exec", info);

  uint64_t generation = 0;
#ifdef PACKAGE_ASYNC
  db_lock_mutex();
#endif
  if (!db_capture_generation((sp - 1)->u.number, &generation)) {
#ifdef PACKAGE_ASYNC
    db_unlock_mutex();
#endif
    error("Attempt to exec on an invalid database handle\n");
  }
#ifdef PACKAGE_ASYNC
  db_unlock_mutex();
#endif

  add_db_exec((sp - 1)->u.number, generation, sp->u.string, cb.release(), bound_args);
  bound_args_handed_off = true;
  pop_2_elems();
}
#endif

#ifdef F_ASYNC_DB_EXEC_PROMISE
void f_async_db_exec_promise() {
  array_t* info = allocate_empty_array(1);
  info->item[0].type = T_STRING;
  info->item[0].subtype = STRING_MALLOC;
  info->item[0].u.string = string_copy(sp->u.string, "f_db_exec_promise");
  valid_database("exec", info);

#ifdef PACKAGE_ASYNC
  db_lock_mutex();
#endif
  uint64_t generation = 0;
  if (!db_capture_generation((sp - 1)->u.number, &generation)) {
#ifdef PACKAGE_ASYNC
    db_unlock_mutex();
#endif
    error("Attempt to exec on an invalid database handle\n");
  }
#ifdef PACKAGE_ASYNC
  db_unlock_mutex();
#endif

  auto* req = make_promise_request(TYPE_NUMBER);
  req->type = ADBEXEC;
  req->handle = static_cast<int>((sp - 1)->u.number);
  req->db_generation = generation;
  auto const* sql = sp->u.string;
  bool admitted = admit_request(req, std::strlen(sql), 0);
  if (admitted) {
    try {
      req->data = sql;
    } catch (...) {
      reject_request(req, "async request allocation failed");
      admitted = false;
    }
  }
  req->promise->ref++;
  pop_2_elems();
  push_refed_promise(req->promise);
  if (admitted) {
    aio_db_exec(req);
  }
}
#endif

void async_mark_request() {
#ifdef DEBUGMALLOC_EXTENSIONS
  auto mark_request = [](Request* req) {
    if (req->fun != nullptr) {
      req->fun->f.fp->hdr.extra_ref++;
    }
    if (req->bound_args != nullptr) {
      req->bound_args->extra_ref++;
    }
    if (req->promise != nullptr) {
      mark_promise(req->promise);
    }
    if (req->owner != nullptr) {
      req->owner->extra_ref++;
    }
  };

  // Keep every live request visible to the debug allocator, including the
  // short interval after a worker moved it into an owner-admitted callback.
  std::lock_guard<std::mutex> const lock(reqs_lock);
  for (auto* req : live_requests) {
    mark_request(req);
  }
#endif
}
