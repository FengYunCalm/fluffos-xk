#include <gtest/gtest.h>

#include "base/package_api.h"
#include "test_mudlib.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <zlib.h>

#include "interactive.h"
#include "mainlib.h"
#include "packages/core/ed.h"
#include "packages/core/file.h"
#include "vm/internal/apply.h"
#include "vm/internal/base/scoped_current_object_as_master.h"
#include "vm/internal/simulate.h"
#include "vm/vm.h"

namespace {

constexpr char kTestFile[] = "u01_file_io_failure.txt";

enum class FaultPoint {
  kNone,
  kOpen,
  kWrite,
  kNewline,
  kStreamError,
  kClose,
  kGzipWrite,
  kGzipClose,
  kDeviceFull
};

// Linker wrapping is enabled only for this test executable. Other streams and
// threads always call the real implementation; no driver configuration enables it.
struct FaultState {
  FaultPoint point = FaultPoint::kNone;
  FILE* stream = nullptr;
  gzFile gzip = nullptr;
  int opens = 0;
  int closes = 0;
  int hits = 0;
};

thread_local FaultState fault;

size_t descriptor_count() {
  size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
    (void)entry;
    ++count;
  }
  return count;
}

}  // namespace

extern "C" {
FILE* __real_fopen(const char*, const char*);
int __real_fclose(FILE*);
size_t __real_fwrite(const void*, size_t, size_t, FILE*);
int __real_fputs(const char*, FILE*);
int __real_fputc(int, FILE*);
int __real_ferror(FILE*);
gzFile __real_gzopen(const char*, const char*);
int __real_gzwrite(gzFile, const void*, unsigned int);
int __real_gzclose(gzFile);

FILE* __wrap_fopen(const char* path, const char* mode) {
  if (std::strcmp(path, kTestFile) != 0) {
    return __real_fopen(path, mode);
  }
  if (fault.point == FaultPoint::kOpen) {
    ++fault.hits;
    errno = EACCES;
    return nullptr;
  }
  fault.stream = __real_fopen(path, mode);
  fault.opens += fault.stream != nullptr;
  return fault.stream;
}

int __wrap_fclose(FILE* stream) {
  if (stream != fault.stream) {
    return __real_fclose(stream);
  }
  fault.stream = nullptr;
  ++fault.closes;
  const int result = __real_fclose(stream);
  if (fault.point == FaultPoint::kClose) {
    ++fault.hits;
    errno = ENOSPC;
    return EOF;
  }
  return result;
}

size_t __wrap_fwrite(const void* data, size_t size, size_t count, FILE* stream) {
  if (stream == fault.stream && fault.point == FaultPoint::kWrite) {
    ++fault.hits;
    errno = ENOSPC;
    return count == 0 ? 0 : count - 1;
  }
  return __real_fwrite(data, size, count, stream);
}

int __wrap_fputs(const char* data, FILE* stream) {
  if (stream == fault.stream && fault.point == FaultPoint::kWrite) {
    ++fault.hits;
    errno = ENOSPC;
    return EOF;
  }
  return __real_fputs(data, stream);
}

int __wrap_fputc(int character, FILE* stream) {
  if (stream == fault.stream && fault.point == FaultPoint::kNewline) {
    ++fault.hits;
    errno = ENOSPC;
    return EOF;
  }
  return __real_fputc(character, stream);
}

int __wrap_ferror(FILE* stream) {
  if (stream == fault.stream && fault.point == FaultPoint::kStreamError) {
    ++fault.hits;
    errno = EIO;
    return 1;
  }
  return __real_ferror(stream);
}

gzFile __wrap_gzopen(const char* path, const char* mode) {
  if (std::strcmp(path, kTestFile) != 0) {
    return __real_gzopen(path, mode);
  }
  if (fault.point == FaultPoint::kOpen) {
    ++fault.hits;
    errno = EACCES;
    return nullptr;
  }
  fault.gzip = __real_gzopen(path, mode);
  fault.opens += fault.gzip != nullptr;
  return fault.gzip;
}

int __wrap_gzwrite(gzFile stream, const void* data, unsigned int length) {
  if (stream == fault.gzip && fault.point == FaultPoint::kGzipWrite) {
    ++fault.hits;
    errno = ENOSPC;
    return 0;
  }
  return __real_gzwrite(stream, data, length);
}

int __wrap_gzclose(gzFile stream) {
  if (stream != fault.gzip) {
    return __real_gzclose(stream);
  }
  fault.gzip = nullptr;
  ++fault.closes;
  const int result = __real_gzclose(stream);
  if (fault.point == FaultPoint::kGzipClose) {
    ++fault.hits;
    errno = ENOSPC;
    return Z_ERRNO;
  }
  return result;
}
}

namespace {

class FileIoTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    const auto mudlib = fluffos_test_mudlib::root();
    ASSERT_NE(std::getenv("FLUFFOS_TEST_MUDLIB"), nullptr) << "requires an isolated test mudlib";
    ASSERT_EQ(fluffos_test_mudlib::change_directory(mudlib), 0);
    init_main("etc/config.test");
    vm_start();
  }

 protected:
  void SetUp() override {
    clear_state();
    save_context(&context_);
    fault = {};
    descriptors_before_ = descriptor_count();
    ASSERT_FALSE(std::filesystem::exists(kTestFile));
    owns_path_ = true;
#ifdef OLD_ED
    ASSERT_EQ(master_ob->interactive, nullptr);
    editor_ip_.ob = master_ob;
    editor_ip_.iflags = NET_DEAD;  // The editor needs a user, not a live transport.
    master_ob->interactive = &editor_ip_;
    save_command_giver(master_ob);
    editor_attached_ = true;
#endif
  }

  void TearDown() override {
    EXPECT_EQ(fault.stream, nullptr);
    EXPECT_EQ(fault.gzip, nullptr);
    // Reclaim only this fixture's handles if a failing implementation leaked them.
    if (fault.stream) {
      __real_fclose(fault.stream);
    }
    if (fault.gzip) {
      __real_gzclose(fault.gzip);
    }
    fault = {};
    if (editor_active()) {
      run_editor_command("Q");
    }
#ifdef OLD_ED
    if (editor_attached_) {
      master_ob->interactive = nullptr;
      restore_command_giver();
    }
#endif
    if (owns_path_) {
      std::filesystem::remove(kTestFile);
    }
    EXPECT_EQ(descriptor_count(), descriptors_before_);
    restore_context(&context_);
    clear_state();
  }

  void check_write_failure(FaultPoint point, int flags) {
    ScopedCurrentObjectAsMaster current;
    fault.point = point;
    EXPECT_EQ(write_file(kTestFile, "payload\n", flags), 0);
    EXPECT_EQ(fault.hits, 1);
    EXPECT_EQ(fault.opens, 1);
    EXPECT_EQ(fault.closes, 1);
  }

  bool editor_active() {
#ifdef OLD_ED
    return editor_ip_.ed_buffer != nullptr;
#else
    return master_ob->flags & O_IN_EDIT;
#endif
  }

#ifndef OLD_ED
  std::string consume_ed_result(char* result) {
    const std::string text = result ? result : "";
    if (result) {
      FREE_MSTR(result);
    }
    return text;
  }
#endif

  std::string run_editor_command(const char* command) {
#ifdef OLD_ED
    std::string mutable_command = command;
    ed_cmd(mutable_command.data());
    return {};
#else
    return consume_ed_result(object_ed_cmd(master_ob, command));
#endif
  }

  void check_editor_failure(FaultPoint point, const char* command) {
    ScopedCurrentObjectAsMaster current;
    ASSERT_EQ(write_file(kTestFile, "", 1), 1);
#ifdef OLD_ED
    ed_start(kTestFile, callback_object_ ? "on_write" : nullptr, nullptr, 0, callback_object_, 0);
#else
    consume_ed_result(object_ed_start(master_ob, kTestFile, 0, 0));
#endif
    run_editor_command("a");
    run_editor_command("retained buffer line");
    run_editor_command(".");
    if (point == FaultPoint::kDeviceFull) {
      ASSERT_TRUE(std::filesystem::exists("/dev/full"));
      ASSERT_TRUE(std::filesystem::remove(kTestFile));
      std::filesystem::create_symlink("/dev/full", kTestFile);
    }
    ASSERT_EQ(fault.stream, nullptr);
    fault = {};
    fault.point = point;
    const auto result = run_editor_command(command);
#ifndef OLD_ED
    EXPECT_NE(result.find("error"), std::string::npos) << result;
#endif
    EXPECT_EQ(fault.hits, point == FaultPoint::kDeviceFull ? 0 : 1);
    EXPECT_EQ(fault.closes, 1);
#ifdef OLD_ED
    check_success_callbacks(0);
#endif
    ASSERT_TRUE(editor_active()) << "failed write must not end the session";
    run_editor_command("q");
    ASSERT_TRUE(editor_active()) << "failed write must retain the changed flag";
#ifdef OLD_ED
    EXPECT_STREQ(editor_ip_.ed_buffer->Line0.l_next->l_buff, "retained buffer line");
#else
    const auto contents = run_editor_command("1,$p");
    EXPECT_NE(contents.find("retained buffer line"), std::string::npos);
#endif
    fault.point = FaultPoint::kNone;
    if (point == FaultPoint::kDeviceFull) {
      ASSERT_TRUE(std::filesystem::remove(kTestFile));
    }
    run_editor_command("x");
    EXPECT_FALSE(editor_active());
#ifdef OLD_ED
    check_success_callbacks(1);
#endif
  }

#ifdef OLD_ED
  object_t* callback_object_ = nullptr;

  void check_success_callbacks(int expected) {
    if (callback_object_) {
      auto* result = safe_apply("successful_writes", callback_object_, 0, ORIGIN_DRIVER);
      ASSERT_NE(result, nullptr);
      ASSERT_EQ(result->type, T_NUMBER);
      EXPECT_EQ(result->u.number, expected);
      vm_apply_return_clear();
    }
  }
#endif

 private:
  size_t descriptors_before_ = 0;
  error_context_t context_{};
  bool owns_path_ = false;
#ifdef OLD_ED
  interactive_t editor_ip_{};
  bool editor_attached_ = false;
#endif
};

TEST_F(FileIoTest, WriteFileRejectsShortWrite) {
  check_write_failure(FaultPoint::kWrite, 1);
}

TEST_F(FileIoTest, WriteFileRejectsStreamError) {
  check_write_failure(FaultPoint::kStreamError, 1);
}

TEST_F(FileIoTest, WriteFileRejectsCloseFailure) {
  check_write_failure(FaultPoint::kClose, 1);
}

TEST_F(FileIoTest, GzipWriteRejectsWriteFailure) {
  check_write_failure(FaultPoint::kGzipWrite, 3);
}

TEST_F(FileIoTest, GzipWriteRejectsCloseFailure) {
  check_write_failure(FaultPoint::kGzipClose, 3);
}

TEST_F(FileIoTest, OpenFailureKeepsExistingExceptionContract) {
  ScopedCurrentObjectAsMaster current;
  for (const int flags : {1, 3}) {
    fault = {};
    fault.point = FaultPoint::kOpen;
    error_context_t context{};
    save_context(&context);
    bool threw = false;
    try {
      write_file(kTestFile, "payload", flags);
      pop_context(&context);
    } catch (...) {
      restore_context(&context);
      threw = true;
    }
    EXPECT_TRUE(threw);
    EXPECT_EQ(fault.hits, 1);
    EXPECT_EQ(fault.opens, 0);
    EXPECT_EQ(fault.closes, 0);
  }
}

TEST_F(FileIoTest, RealBufferedWriteFailure) {
  ScopedCurrentObjectAsMaster current;
  ASSERT_TRUE(std::filesystem::exists("/dev/full"));
  std::filesystem::create_symlink("/dev/full", kTestFile);
  EXPECT_EQ(write_file(kTestFile, "payload\n", 1), 0);
  EXPECT_EQ(fault.opens, 1);
  EXPECT_EQ(fault.closes, 1);
}

TEST_F(FileIoTest, RealGzipFlushFailure) {
  ScopedCurrentObjectAsMaster current;
  ASSERT_TRUE(std::filesystem::exists("/dev/full"));
  std::filesystem::create_symlink("/dev/full", kTestFile);
  EXPECT_EQ(write_file(kTestFile, "payload\n", 3), 0);
  EXPECT_EQ(fault.opens, 1);
  EXPECT_EQ(fault.closes, 1);
}

TEST_F(FileIoTest, EmptyWritesSucceed) {
  ScopedCurrentObjectAsMaster current;
  EXPECT_EQ(write_file(kTestFile, "", 1), 1);
  EXPECT_EQ(write_file(kTestFile, "", 3), 1);
  EXPECT_EQ(fault.opens, 2);
  EXPECT_EQ(fault.closes, 2);
}

TEST_F(FileIoTest, EditorRetainsChangesOnRealFlushFailure) {
  check_editor_failure(FaultPoint::kDeviceFull, "w");
}

TEST_F(FileIoTest, EditorDoesNotExitOnRealFlushFailure) {
  check_editor_failure(FaultPoint::kDeviceFull, "x");
}

TEST_F(FileIoTest, EditorRetainsChangesOnShortWrite) {
  check_editor_failure(FaultPoint::kWrite, "w");
}

TEST_F(FileIoTest, EditorRetainsChangesOnNewlineFailure) {
  check_editor_failure(FaultPoint::kNewline, "w");
}

TEST_F(FileIoTest, EditorRetainsChangesOnStreamError) {
  check_editor_failure(FaultPoint::kStreamError, "w");
}

TEST_F(FileIoTest, EditorRetainsChangesOnCloseFailure) {
  check_editor_failure(FaultPoint::kClose, "w");
}

#ifdef OLD_ED
TEST_F(FileIoTest, EditorNotifiesOnlyAfterSuccessfulClose) {
  ScopedCurrentObjectAsMaster current;
  callback_object_ = load_object("clone/file_io_editor", 0);
  ASSERT_NE(callback_object_, nullptr);
  const auto references = callback_object_->ref;
  check_editor_failure(FaultPoint::kClose, "w");
  EXPECT_EQ(callback_object_->ref, references);
}
#endif

TEST_F(FileIoTest, EditorDoesNotExitOnFailedSave) {
  check_editor_failure(FaultPoint::kClose, "x");
}

}  // namespace
