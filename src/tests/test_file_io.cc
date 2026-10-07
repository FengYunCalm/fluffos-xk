#include <gtest/gtest.h>

#include "base/package_api.h"
#include "test_mudlib.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
constexpr char kInputFile[] = "u01_file_io_input.bin";

enum class FaultPoint {
  kNone,
  kOpen,
  kWrite,
  kNewline,
  kStreamError,
  kClose,
  kGzipWrite,
  kGzipClose,
  kDeviceFull,
  kRead,
  kGzipRead,
  kUnlink,
  kFileAdopt,
  kGzipAdopt,
  kMetadata,
  kTruncate
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
  int input_fd = -1;
  int output_fd = -1;
  size_t write_limit = 0;
  int oversized_writes = 0;
};

thread_local FaultState fault;

bool fixture_path(const char* path) {
  return std::strcmp(path, kTestFile) == 0 || std::strcmp(path, kInputFile) == 0;
}

bool reject_open(const char* path) {
  if (fault.point == FaultPoint::kOpen && std::strcmp(path, kTestFile) == 0) {
    ++fault.hits;
    errno = EACCES;
    return true;
  }
  return false;
}

bool tracked_fd(int fd) {
  return fd >= 0 && (fd == fault.input_fd || fd == fault.output_fd);
}

void transfer_fd(int fd) {
  if (fd == fault.input_fd) {
    fault.input_fd = -1;
  }
  if (fd == fault.output_fd) {
    fault.output_fd = -1;
  }
}

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
int __real_open(const char*, int, ...);
int __real_close(int);
int __real_unlink(const char*);
int __real_fstat(int, struct stat*);
int __real_ftruncate(int, off_t);
FILE* __real_fdopen(int, const char*);
gzFile __real_gzdopen(int, const char*);
size_t __real_fread(void*, size_t, size_t, FILE*);
int __real_gzread(gzFile, void*, unsigned int);
FILE* __real_fopen(const char*, const char*);
int __real_fclose(FILE*);
size_t __real_fwrite(const void*, size_t, size_t, FILE*);
int __real_fputs(const char*, FILE*);
int __real_fputc(int, FILE*);
int __real_ferror(FILE*);
gzFile __real_gzopen(const char*, const char*);
int __real_gzwrite(gzFile, const void*, unsigned int);
int __real_gzclose(gzFile);

int __wrap_open(const char* path, int flags, ...) {
  int mode = 0;
  bool needs_mode = flags & O_CREAT;
#ifdef O_TMPFILE
  needs_mode = needs_mode || (flags & O_TMPFILE) == O_TMPFILE;
#endif
  if (needs_mode) {
    va_list arguments;
    va_start(arguments, flags);
    mode = va_arg(arguments, int);
    va_end(arguments);
  }
  if (reject_open(path)) {
    return -1;
  }
  const int fd = __real_open(path, flags, mode);
  if (fd >= 0 && fixture_path(path)) {
    if (std::strcmp(path, kInputFile) == 0) {
      fault.input_fd = fd;
    } else {
      fault.output_fd = fd;
    }
    ++fault.opens;
  }
  return fd;
}

int __wrap_close(int fd) {
  if (tracked_fd(fd)) {
    transfer_fd(fd);
    ++fault.closes;
  }
  return __real_close(fd);
}

int __wrap_fstat(int fd, struct stat* info) {
  if (fd == fault.output_fd && fault.point == FaultPoint::kMetadata) {
    ++fault.hits;
    errno = EIO;
    return -1;
  }
  return __real_fstat(fd, info);
}

int __wrap_ftruncate(int fd, off_t length) {
  if (fd == fault.output_fd && fault.point == FaultPoint::kTruncate) {
    ++fault.hits;
    errno = EIO;
    return -1;
  }
  return __real_ftruncate(fd, length);
}

int __wrap_unlink(const char* path) {
  if (fixture_path(path) && fault.point == FaultPoint::kUnlink) {
    ++fault.hits;
    errno = EACCES;
    return -1;
  }
  return __real_unlink(path);
}

FILE* __wrap_fdopen(int fd, const char* mode) {
  const bool tracked = tracked_fd(fd);
  if (tracked && fault.point == FaultPoint::kFileAdopt) {
    ++fault.hits;
    errno = ENOMEM;
    return nullptr;
  }
  FILE* stream = __real_fdopen(fd, mode);
  if (tracked && stream) {
    transfer_fd(fd);
    fault.stream = stream;
  }
  return stream;
}

gzFile __wrap_gzdopen(int fd, const char* mode) {
  const bool tracked = tracked_fd(fd);
  if (tracked && fault.point == FaultPoint::kGzipAdopt) {
    ++fault.hits;
    errno = ENOMEM;
    return nullptr;
  }
  gzFile stream = __real_gzdopen(fd, mode);
  if (tracked && stream) {
    transfer_fd(fd);
    fault.gzip = stream;
  }
  return stream;
}

size_t __wrap_fread(void* data, size_t size, size_t count, FILE* stream) {
  if (stream == fault.stream && fault.point == FaultPoint::kRead) {
    ++fault.hits;
    errno = EIO;
    return 0;
  }
  return __real_fread(data, size, count, stream);
}

int __wrap_gzread(gzFile stream, void* data, unsigned int length) {
  if (stream == fault.gzip && fault.point == FaultPoint::kGzipRead) {
    ++fault.hits;
    errno = EIO;
    return -1;
  }
  return __real_gzread(stream, data, length);
}

FILE* __wrap_fopen(const char* path, const char* mode) {
  if (!fixture_path(path)) {
    return __real_fopen(path, mode);
  }
  if (reject_open(path)) {
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
  // Reject oversized requests before libc can read beyond a corrupt-input buffer.
  if (stream == fault.stream && fault.write_limit && size && count > fault.write_limit / size) {
    ++fault.oversized_writes;
    errno = EFBIG;
    return 0;
  }
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
  if (stream == fault.stream &&
      (fault.point == FaultPoint::kStreamError || fault.point == FaultPoint::kRead)) {
    ++fault.hits;
    errno = EIO;
    return 1;
  }
  return __real_ferror(stream);
}

gzFile __wrap_gzopen(const char* path, const char* mode) {
  if (!fixture_path(path)) {
    return __real_gzopen(path, mode);
  }
  if (reject_open(path)) {
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
    ASSERT_FALSE(std::filesystem::exists(kTestFile) || std::filesystem::is_symlink(kTestFile));
    owns_path_ = true;
    ASSERT_FALSE(std::filesystem::exists(kInputFile) || std::filesystem::is_symlink(kInputFile));
    owns_input_ = true;
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
    EXPECT_EQ(fault.input_fd, -1);
    EXPECT_EQ(fault.output_fd, -1);
    // Reclaim only this fixture's handles if a failing implementation leaked them.
    if (fault.stream) {
      __real_fclose(fault.stream);
    }
    if (fault.gzip) {
      __real_gzclose(fault.gzip);
    }
    if (fault.input_fd >= 0) {
      __real_close(fault.input_fd);
    }
    if (fault.output_fd >= 0) {
      __real_close(fault.output_fd);
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
    if (owns_input_) {
      std::filesystem::remove(kInputFile);
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

#ifdef PACKAGE_COMPRESS
  void prepare_compression(bool decompress) {
    ASSERT_EQ(write_file(kInputFile, "compression payload\n", decompress ? 3 : 1), 1);
    compression_object_ = load_object("clone/compress_file_io", 0);
    ASSERT_NE(compression_object_, nullptr);
  }

  std::string input_bytes() {
    std::ifstream input(kInputFile, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  }

  int run_compression(bool decompress, const char* output = kTestFile) {
    copy_and_push_string(kInputFile);
    copy_and_push_string(output);
    auto* result = safe_apply(decompress ? "uncompress_paths" : "compress_paths",
                              compression_object_, 2, ORIGIN_DRIVER);
    if (!result || result->type != T_NUMBER) {
      ADD_FAILURE() << "compression must return its numeric contract";
      vm_apply_return_clear();
      return -1;
    }
    const int value = result->u.number;
    vm_apply_return_clear();
    return value;
  }

  void check_preserved_input(const std::string& original) {
    EXPECT_EQ(fault.opens, fault.closes);
    EXPECT_EQ(fault.oversized_writes, 0);
    fault.point = FaultPoint::kNone;
    EXPECT_TRUE(std::filesystem::exists(kInputFile));
    EXPECT_EQ(input_bytes(), original);
  }

  void check_compression_failure(bool decompress, FaultPoint point) {
    ScopedCurrentObjectAsMaster current;
    ASSERT_NO_FATAL_FAILURE(prepare_compression(decompress));
    const auto original = input_bytes();
    ASSERT_FALSE(original.empty());
    fault = {};
    fault.point = point;
    fault.write_limit = 4096;
    EXPECT_EQ(run_compression(decompress), 0);
    EXPECT_GE(fault.hits, 1);
    check_preserved_input(original);
  }

  void check_compression_alias(bool decompress, const char* alias) {
    ScopedCurrentObjectAsMaster current;
    ASSERT_NO_FATAL_FAILURE(prepare_compression(decompress));
    const auto original = input_bytes();
    ASSERT_FALSE(original.empty());
    const char* output = kTestFile;
    if (std::strcmp(alias, "same") == 0) {
      output = kInputFile;
    } else if (std::strcmp(alias, "hardlink") == 0) {
      std::filesystem::create_hard_link(kInputFile, output);
    } else {
      std::filesystem::create_symlink(kInputFile, output);
    }
    fault = {};
    fault.write_limit = 4096;
    EXPECT_EQ(run_compression(decompress, output), 0);
    check_preserved_input(original);
  }

  void check_compression_device(bool decompress) {
    ScopedCurrentObjectAsMaster current;
    ASSERT_NO_FATAL_FAILURE(prepare_compression(decompress));
    const auto original = input_bytes();
    ASSERT_TRUE(std::filesystem::exists("/dev/full"));
    std::filesystem::create_symlink("/dev/full", kTestFile);
    fault = {};
    fault.write_limit = 4096;
    EXPECT_EQ(run_compression(decompress), 0);
    check_preserved_input(original);
  }

  void check_corrupt_gzip(bool truncated) {
    ScopedCurrentObjectAsMaster current;
    ASSERT_NO_FATAL_FAILURE(prepare_compression(true));
    auto original = input_bytes();
    ASSERT_GT(original.size(), 8U);
    if (truncated) {
      original.pop_back();
    } else {
      original[original.size() - 8] ^= 0x40;
    }
    std::ofstream input(kInputFile, std::ios::binary | std::ios::trunc);
    input.write(original.data(), original.size());
    input.close();
    ASSERT_TRUE(input.good());
    fault = {};
    fault.write_limit = 4096;
    EXPECT_EQ(run_compression(true), 0);
    check_preserved_input(original);
  }

  object_t* compression_object_ = nullptr;
#endif

 private:
  size_t descriptors_before_ = 0;
  error_context_t context_{};
  bool owns_path_ = false;
  bool owns_input_ = false;
#ifdef OLD_ED
  interactive_t editor_ip_{};
  bool editor_attached_ = false;
#endif
};

#ifdef PACKAGE_COMPRESS
TEST_F(FileIoTest, CompressMetadataFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kMetadata);
}

TEST_F(FileIoTest, UncompressMetadataFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kMetadata);
}

TEST_F(FileIoTest, CompressTruncateFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kTruncate);
}

TEST_F(FileIoTest, UncompressTruncateFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kTruncate);
}

TEST_F(FileIoTest, CompressRealFlushFailurePreservesInput) {
  check_compression_device(false);
}

TEST_F(FileIoTest, UncompressRealFlushFailurePreservesInput) {
  check_compression_device(true);
}

TEST_F(FileIoTest, CompressReadFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kRead);
}

TEST_F(FileIoTest, CompressStreamErrorPreservesInput) {
  check_compression_failure(false, FaultPoint::kStreamError);
}

TEST_F(FileIoTest, CompressWriteFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kGzipWrite);
}

TEST_F(FileIoTest, CompressInputCloseFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kClose);
}

TEST_F(FileIoTest, CompressOutputCloseFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kGzipClose);
}

TEST_F(FileIoTest, CompressOpenFailurePreservesInput) {
  check_compression_failure(false, FaultPoint::kOpen);
}

TEST_F(FileIoTest, CompressUnlinkFailureReturnsFailure) {
  check_compression_failure(false, FaultPoint::kUnlink);
}

TEST_F(FileIoTest, UncompressReadFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kGzipRead);
}

TEST_F(FileIoTest, UncompressStreamErrorPreservesInput) {
  check_compression_failure(true, FaultPoint::kStreamError);
}

TEST_F(FileIoTest, UncompressWriteFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kWrite);
}

TEST_F(FileIoTest, UncompressInputCloseFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kGzipClose);
}

TEST_F(FileIoTest, UncompressOutputCloseFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kClose);
}

TEST_F(FileIoTest, UncompressOpenFailurePreservesInput) {
  check_compression_failure(true, FaultPoint::kOpen);
}

TEST_F(FileIoTest, UncompressUnlinkFailureReturnsFailure) {
  check_compression_failure(true, FaultPoint::kUnlink);
}

TEST_F(FileIoTest, CompressRejectsSamePath) {
  check_compression_alias(false, "same");
}

TEST_F(FileIoTest, CompressRejectsHardlinkAlias) {
  check_compression_alias(false, "hardlink");
}

TEST_F(FileIoTest, CompressRejectsSymlinkAlias) {
  check_compression_alias(false, "symlink");
}

TEST_F(FileIoTest, UncompressRejectsSamePath) {
  check_compression_alias(true, "same");
}

TEST_F(FileIoTest, UncompressRejectsHardlinkAlias) {
  check_compression_alias(true, "hardlink");
}

TEST_F(FileIoTest, UncompressRejectsSymlinkAlias) {
  check_compression_alias(true, "symlink");
}

TEST_F(FileIoTest, UncompressRejectsBadCrc) {
  check_corrupt_gzip(false);
}

TEST_F(FileIoTest, UncompressRejectsTruncatedGzip) {
  check_corrupt_gzip(true);
}

TEST_F(FileIoTest, CompressFileAdoptionFailureClosesDescriptors) {
  check_compression_failure(false, FaultPoint::kFileAdopt);
}

TEST_F(FileIoTest, CompressGzipAdoptionFailureClosesDescriptors) {
  check_compression_failure(false, FaultPoint::kGzipAdopt);
}

TEST_F(FileIoTest, UncompressFileAdoptionFailureClosesDescriptors) {
  check_compression_failure(true, FaultPoint::kFileAdopt);
}

TEST_F(FileIoTest, UncompressGzipAdoptionFailureClosesDescriptors) {
  check_compression_failure(true, FaultPoint::kGzipAdopt);
}
#endif

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
