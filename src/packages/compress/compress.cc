/* Compression efun stuff.
 * Started Wed Mar 21 01:52:25 PST 2001
 * by David Bennett (ddt@discworld.imaginary.com)
 */

#include "base/package_api.h"

#include "packages/core/file.h"

#include <memory>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>

#include <zlib.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

constexpr char kGzExtension[] = ".gz";
constexpr size_t kGzExtensionLength = sizeof(kGzExtension) - 1;

enum { COMPRESS_BUF_SIZE = 8096 };

namespace {

using FileStream = std::unique_ptr<FILE, decltype(&fclose)>;
using GzipStream = std::unique_ptr<gzFile_s, decltype(&gzclose)>;

int open_binary_file(const char* path, int flags) {
#ifdef O_BINARY
  flags |= O_BINARY;
#endif
  return open(path, flags, 0666);
}

int open_output_file(const char* path, int input_fd) {
  const int output_fd = open_binary_file(path, O_WRONLY | O_CREAT);
  if (output_fd < 0) {
    return -1;
  }
  const auto fail = [output_fd]() {
    const int saved_errno = errno;
    close(output_fd);
    errno = saved_errno;
    return -1;
  };

  // Compare open handles before truncation; path comparisons miss aliases and races.
#ifdef _WIN32
  struct _stat64 input_stat{}, output_stat{};
  if (_fstat64(input_fd, &input_stat) != 0 || _fstat64(output_fd, &output_stat) != 0) {
    return fail();
  }
  const bool input_regular = (input_stat.st_mode & _S_IFMT) == _S_IFREG;
  const bool output_regular = (output_stat.st_mode & _S_IFMT) == _S_IFREG;
  if (input_regular && output_regular) {
    BY_HANDLE_FILE_INFORMATION input_info{}, output_info{};
    if (!GetFileInformationByHandle(reinterpret_cast<HANDLE>(_get_osfhandle(input_fd)),
                                    &input_info) ||
        !GetFileInformationByHandle(reinterpret_cast<HANDLE>(_get_osfhandle(output_fd)),
                                    &output_info)) {
      errno = EIO;
      return fail();
    }
    if (input_info.dwVolumeSerialNumber == output_info.dwVolumeSerialNumber &&
        input_info.nFileIndexHigh == output_info.nFileIndexHigh &&
        input_info.nFileIndexLow == output_info.nFileIndexLow) {
      errno = EINVAL;
      return fail();
    }
  }
  if (output_regular && _chsize_s(output_fd, 0) != 0) {
    return fail();
  }
#else
  struct stat input_stat{}, output_stat{};
  if (fstat(input_fd, &input_stat) != 0 || fstat(output_fd, &output_stat) != 0) {
    return fail();
  }
  if (input_stat.st_dev == output_stat.st_dev && input_stat.st_ino == output_stat.st_ino) {
    errno = EINVAL;
    return fail();
  }
  if (S_ISREG(output_stat.st_mode) && ftruncate(output_fd, 0) != 0) {
    return fail();
  }
#endif
  return output_fd;
}

bool compress_file_contents(const char* input, const char* output) {
  const int input_fd = open_binary_file(input, O_RDONLY);
  if (input_fd < 0) {
    return false;
  }
  FileStream source(fdopen(input_fd, "rb"), &fclose);
  if (!source) {
    close(input_fd);
    return false;
  }
  const int output_fd = open_output_file(output, input_fd);
  if (output_fd < 0) {
    return false;
  }
  GzipStream destination(gzdopen(output_fd, "wb"), &gzclose);
  if (!destination) {
    close(output_fd);
    return false;
  }

  char buffer[4096];
  bool wrote_all = true;
  size_t count;
  while ((count = fread(buffer, 1, sizeof(buffer), source.get())) != 0) {
    if (gzwrite(destination.get(), buffer, static_cast<unsigned int>(count)) !=
        static_cast<int>(count)) {
      wrote_all = false;
      break;
    }
  }
  const bool read_all = !ferror(source.get());
  const int source_close = fclose(source.release());
  const int destination_close = gzclose(destination.release());
  return read_all && wrote_all && source_close == 0 && destination_close == Z_OK &&
         unlink(input) == 0;
}

bool uncompress_file_contents(const char* input, const char* output) {
  const int input_fd = open_binary_file(input, O_RDONLY);
  if (input_fd < 0) {
    return false;
  }
  GzipStream source(gzdopen(input_fd, "rb"), &gzclose);
  if (!source) {
    close(input_fd);
    return false;
  }
  const int output_fd = open_output_file(output, input_fd);
  if (output_fd < 0) {
    return false;
  }
  FileStream destination(fdopen(output_fd, "wb"), &fclose);
  if (!destination) {
    close(output_fd);
    return false;
  }

  char buffer[4096];
  bool wrote_all = true;
  int count;
  while ((count = gzread(source.get(), buffer, sizeof(buffer))) > 0) {
    if (fwrite(buffer, 1, count, destination.get()) != static_cast<size_t>(count)) {
      wrote_all = false;
      break;
    }
  }
  const bool output_ok = !ferror(destination.get());
  const int source_close = gzclose(source.release());
  const int destination_close = fclose(destination.release());
  return count == 0 && wrote_all && output_ok && source_close == Z_OK && destination_close == 0 &&
         unlink(input) == 0;
}

}  // namespace

#ifdef F_COMPRESS_FILE
void f_compress_file() {
  int const num_arg = st_num_arg;
  const char *input_file;
  const char *real_input_file;
  const char *real_output_file;

  // Not a string?  Error!
  if ((sp - num_arg + 1)->type != T_STRING) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  input_file = (sp - num_arg + 1)->u.string;
  std::string input_path(input_file);
  std::string output_file;
  if (num_arg == 2) {
    if (((sp - num_arg + 2)->type != T_STRING)) {
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = (sp - num_arg + 2)->u.string;
  } else {
    if (input_path.size() >= kGzExtensionLength &&
        input_path.compare(input_path.size() - kGzExtensionLength, kGzExtensionLength,
                           kGzExtension) == 0) {
      // Already compressed...
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = input_path + kGzExtension;
  }

  real_output_file = check_valid_path(output_file.c_str(), current_object, "compress_file", 1);
  if (!real_output_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }
  std::string output_path(real_output_file);

  real_input_file = check_valid_path(input_path.c_str(), current_object, "compress_file", 0);
  if (!real_input_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  const std::string source_path(real_input_file);
  const bool succeeded = compress_file_contents(source_path.c_str(), output_path.c_str());
  pop_n_elems(num_arg);
  push_number(succeeded);
}
#endif

#ifdef F_UNCOMPRESS_FILE
void f_uncompress_file() {
  int const num_arg = st_num_arg;
  const char *input_file;
  const char *real_input_file;
  const char *real_output_file;

  // Not a string?  Error!
  if ((sp - num_arg + 1)->type != T_STRING) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  input_file = (sp - num_arg + 1)->u.string;
  std::string input_path(input_file);
  std::string output_file;
  if (num_arg == 2) {
    if (((sp - num_arg + 2)->type != T_STRING)) {
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = (sp - num_arg + 2)->u.string;
  } else {
    const bool has_gz_extension =
        input_path.size() >= kGzExtensionLength &&
        input_path.compare(input_path.size() - kGzExtensionLength, kGzExtensionLength,
                           kGzExtension) == 0;
    if (!has_gz_extension) {
      // Not compressed...
      pop_n_elems(num_arg);
      push_number(0);
      return;
    }
    output_file = input_path.substr(0, input_path.size() - kGzExtensionLength);
  }

  real_output_file = check_valid_path(output_file.c_str(), current_object, "compress_file", 1);
  if (!real_output_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }
  std::string output_path(real_output_file);

  real_input_file = check_valid_path(input_path.c_str(), current_object, "compress_file", 0);
  if (!real_input_file) {
    pop_n_elems(num_arg);
    push_number(0);
    return;
  }

  const std::string source_path(real_input_file);
  const bool succeeded = uncompress_file_contents(source_path.c_str(), output_path.c_str());
  pop_n_elems(num_arg);
  push_number(succeeded);
}
#endif

#ifdef F_COMPRESS
void f_compress() {
  unsigned char *buffer;
  unsigned char *input;
  int size;
  buffer_t *real_buffer;
  uLongf new_size;

  if (sp->type == T_STRING) {
    size = SVALUE_STRLEN(sp);
    input = (unsigned char *)sp->u.string;
  } else if (sp->type == T_BUFFER) {
    size = sp->u.buf->size;
    input = sp->u.buf->item;
  } else {
    pop_n_elems(st_num_arg);
    push_undefined();
    return;
  }

  new_size = compressBound(size);
  // Make it a little larger as specified in the docs.
  buffer = reinterpret_cast<unsigned char *>(DMALLOC(new_size, TAG_TEMPORARY, "compress"));
  compress(buffer, &new_size, input, size);

  // Shrink it down.
  pop_n_elems(st_num_arg);
  real_buffer = allocate_buffer(new_size);
  write_buffer(real_buffer, 0, reinterpret_cast<char *>(buffer), new_size);
  FREE(buffer);
  push_refed_buffer(real_buffer);
}
#endif

#ifdef F_UNCOMPRESS
static void *zlib_alloc(void * /*opaque*/, unsigned int items, unsigned int size) {
  return DCALLOC(items, size, TAG_TEMPORARY, "zlib_alloc");
}

static void zlib_free(void * /*opaque*/, void *address) { FREE(address); }

void f_uncompress() {
  z_stream *compressed;
  unsigned char compress_buf[COMPRESS_BUF_SIZE];
  unsigned char *output_data = nullptr;
  int len;
  int pos;
  buffer_t *buffer;
  int ret;

  if (sp->type == T_BUFFER) {
    buffer = sp->u.buf;
  } else {
    pop_n_elems(st_num_arg);
    push_undefined();
    return;
  }

  compressed =
      reinterpret_cast<z_stream *>(DMALLOC(sizeof(z_stream), TAG_INTERACTIVE, "start_compression"));
  compressed->next_in = buffer->item;
  compressed->avail_in = buffer->size;
  compressed->next_out = compress_buf;
  compressed->avail_out = COMPRESS_BUF_SIZE;
  compressed->zalloc = zlib_alloc;
  compressed->zfree = zlib_free;
  compressed->opaque = nullptr;

  if (inflateInit(compressed) != Z_OK) {
    FREE(compressed);
    pop_n_elems(st_num_arg);
    error("inflateInit failed");
  }

  len = 0;
  output_data = nullptr;
  bool too_large = false;
  do {
    ret = inflate(compressed, 0);
    if (ret == Z_OK || ret == Z_STREAM_END) {
      pos = len;
      len += COMPRESS_BUF_SIZE - compressed->avail_out;
      if (len > CONFIG_INT(__MAX_BUFFER_SIZE__)) {
        // Decompressed output has grown past the configured limit (e.g. a
        // zip bomb) -- stop here, before `len` can overflow the plain int
        // used for the DMALLOC/DREALLOC/memcpy sizes below.
        too_large = true;
        break;
      }
      if (!output_data) {
        output_data = reinterpret_cast<unsigned char *>(DMALLOC(len, TAG_TEMPORARY, "uncompress"));
      } else {
        output_data = reinterpret_cast<unsigned char *>(
            DREALLOC(output_data, len, TAG_TEMPORARY, "uncompress"));
      }
      memcpy(output_data + pos, compress_buf, len - pos);
      compressed->next_out = compress_buf;
      compressed->avail_out = COMPRESS_BUF_SIZE;
    }
  } while (ret == Z_OK);

  inflateEnd(compressed);

  pop_n_elems(st_num_arg);

  if (too_large) {
    if (output_data) {
      FREE(output_data);
    }
    FREE(compressed);
    error("uncompress: decompressed data exceeds maximum buffer size\n");
  }

  if (ret == Z_STREAM_END) {
    buffer = allocate_buffer(len);
    write_buffer(buffer, 0, reinterpret_cast<char *>(output_data), len);
    FREE(output_data);
    push_refed_buffer(buffer);
    FREE(compressed);
  } else {
    // #1247 COMPRESS-1: release the partial output on the error path --
    // otherwise every failed inflate leaks the buffer.
    if (output_data) {
      FREE(output_data);
    }
    FREE(compressed);
    error("inflate: no ZSTREAM_END\n");
  }
}
#endif
