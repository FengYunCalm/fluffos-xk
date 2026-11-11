/* buffer.c for MudOS 0.9.x by John Garnett, 1993/11/07 */

#include "base/std.h"

#include "vm/internal/base/machine.h"

buffer_t null_buf = {
    1, /* Ref count, which will ensure that it will
        * never be deallocated */
    0  /* size */
};

buffer_t* null_buffer() {
  null_buf.ref++;
  return &null_buf;
} /* null_buffer() */

void free_buffer(buffer_t* b) {
  b->ref--;
  /* don't try to free the null_buffer (ref count might overflow) */
  if ((b->ref > 0) || (b == &null_buf)) {
    return;
  }
  FREE((char*)b);
} /* free_buffer() */

buffer_t* allocate_buffer(LPC_INT size) {
  auto max_buffer_size = CONFIG_INT(__MAX_BUFFER_SIZE__);

  buffer_t* buf;

  if ((size < 0) || (size > max_buffer_size)) {
    error("Illegal buffer size.\n");
  }
  if (size == 0) {
    return null_buffer();
  }
  /* using calloc() so that memory will be zero'd out when allocated */
  buf = reinterpret_cast<buffer_t*>(
      DCALLOC(sizeof(buffer_t) + size - 1, 1, TAG_BUFFER, "allocate_buffer"));
  buf->size = size;
  buf->ref = 1;
  return buf;
}

int write_buffer(buffer_t* buf, LPC_INT start, const char* str, LPC_INT theLength) {
  const auto size = static_cast<LPC_INT>(buf->size);

  if (start < 0) {
    start += size;
    if (start < 0) {
      return 0;
    }
  }
  // Negative or over-long lengths would otherwise convert to a huge size_t in
  // memcpy. Every comparison stays in LPC_INT, so a start or length past 32
  // bits is rejected instead of wrapping back into range (#1247 BUFFER-1).
  if (theLength < 0 || start > size || theLength > size - start) {
    return 0;
  }
  memcpy(buf->item + start, str, theLength);
  return 1;
} /* write_buffer() */

char* read_buffer(buffer_t* b, LPC_INT start, LPC_INT len, int* rlen) {
  const auto size = static_cast<LPC_INT>(b->size);

  if (len < 0) {
    return nullptr;
  }
  if (start < 0) {
    start += size;
    if (start < 0) {
      return nullptr;
    }
  }
  if (len == 0) {
    len = size;
  }
  if (start >= size) {
    return nullptr;
  }
  if (len > size - start) {
    len = size - start;
  }

  LPC_INT n = 0;
  for (const char* src = reinterpret_cast<const char*>(b->item) + start; *src && n < len;
       src++, n++) {
    ;
  }
  char* str = new_string(n, "read_buffer: str");
  memcpy(str, b->item + start, n);
  str[*rlen = static_cast<int>(n)] = '\0';

  return str;
} /* read_buffer() */
