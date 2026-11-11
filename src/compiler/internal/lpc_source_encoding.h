#ifndef LPC_SOURCE_ENCODING_H
#define LPC_SOURCE_ENCODING_H

#include "compiler/internal/LexStream.h"

#include <memory>
#include <string>

// Schema name published in status mappings for the source-encoding boundary.
inline constexpr const char* kLpcSourceEncodingSchemaV1 = "lpc_source_encoding_v1";
// Canonical encoding of VM strings. Legacy source, sessions and gateway payloads
// may opt into another encoding at their own boundary; the VM never sees it.
inline constexpr const char* kLpcInternalStringEncoding = "utf-8";

struct LpcSourceEncodingResult {
  std::string encoding{kLpcInternalStringEncoding};
  bool transcoded{false};
  int invalid_sequence_count{0};
  std::string data;
  std::string error;
};

// Returns the encoding declared by a leading `#pragma source_encoding("...")`,
// falling back to the internal encoding. Scans only the first few lines.
std::string lpc_source_encoding_from_source(const std::string& source);
LpcSourceEncodingResult lpc_source_decode_to_utf8(const std::string& source);
std::unique_ptr<LexStream> lpc_source_encoding_stream(std::unique_ptr<LexStream> source);

#endif /* LPC_SOURCE_ENCODING_H */
