# Find the PCRE2 8-bit library used by the PCRE package.
#
# PCRE_INCLUDE_DIR - directory containing pcre2.h
# PCRE_LIBRARY    - the PCRE2 8-bit library
# PCRE_FOUND      - true only when the minimum version and API probe pass

# A stale PCRE1 cache must not satisfy this module.  Keep valid user-provided
# PCRE2 paths, but discard the old variable values before find_* can reuse them.
if(DEFINED PCRE_INCLUDE_DIR AND
   NOT EXISTS "${PCRE_INCLUDE_DIR}/pcre2.h")
  unset(PCRE_INCLUDE_DIR)
  unset(PCRE_INCLUDE_DIR CACHE)
endif()
if(DEFINED PCRE_LIBRARY AND
   NOT "${PCRE_LIBRARY}" MATCHES "pcre2-8")
  unset(PCRE_LIBRARY)
  unset(PCRE_LIBRARY CACHE)
endif()

find_path(PCRE_INCLUDE_DIR NAMES pcre2.h)
find_library(PCRE_LIBRARY NAMES pcre2-8)

set(PCRE_VERSION "")
set(PCRE2_VERSION_OK FALSE)
set(PCRE2_API_PROBE FALSE)

if(PCRE_INCLUDE_DIR AND EXISTS "${PCRE_INCLUDE_DIR}/pcre2.h")
  file(STRINGS "${PCRE_INCLUDE_DIR}/pcre2.h" _pcre2_major_line
       REGEX "^[ \\t]*#define[ \\t]+PCRE2_MAJOR[ \\t]+[0-9]+")
  file(STRINGS "${PCRE_INCLUDE_DIR}/pcre2.h" _pcre2_minor_line
       REGEX "^[ \\t]*#define[ \\t]+PCRE2_MINOR[ \\t]+[0-9]+")
  string(REGEX REPLACE ".*PCRE2_MAJOR[ \\t]+([0-9]+).*" "\\1" _pcre2_major
         "${_pcre2_major_line}")
  string(REGEX REPLACE ".*PCRE2_MINOR[ \\t]+([0-9]+).*" "\\1" _pcre2_minor
         "${_pcre2_minor_line}")
  if(_pcre2_major AND _pcre2_minor)
    set(PCRE_VERSION "${_pcre2_major}.${_pcre2_minor}")
    if(_pcre2_major GREATER 10 OR
       (_pcre2_major EQUAL 10 AND _pcre2_minor GREATER_EQUAL 42))
      set(PCRE2_VERSION_OK TRUE)
    endif()
  endif()
endif()

if(PCRE_INCLUDE_DIR AND PCRE_LIBRARY AND PCRE2_VERSION_OK)
  include(CheckCSourceCompiles)
  unset(PCRE2_API_PROBE)
  unset(PCRE2_API_PROBE CACHE)
  set(_pcre2_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
  set(_pcre2_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
  set(_pcre2_saved_required_definitions "${CMAKE_REQUIRED_DEFINITIONS}")
  set(CMAKE_REQUIRED_INCLUDES "${PCRE_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${PCRE_LIBRARY}")
  set(CMAKE_REQUIRED_DEFINITIONS "-DPCRE2_CODE_UNIT_WIDTH=8")
  check_c_source_compiles("#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
int main(void) {
  int errorcode = 0;
  PCRE2_SIZE erroroffset = 0;
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)\"a\", 1, 0, &errorcode,
                                   &erroroffset, 0);
  if (code == 0) return 1;
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(code, 0);
  if (match_data == 0) return 2;
  (void)pcre2_match(code, (PCRE2_SPTR)\"a\", 1, 0, 0, match_data, 0);
  pcre2_match_data_free(match_data);
  pcre2_code_free(code);
  return 0;
}" PCRE2_API_PROBE)
  set(CMAKE_REQUIRED_INCLUDES "${_pcre2_saved_required_includes}")
  set(CMAKE_REQUIRED_LIBRARIES "${_pcre2_saved_required_libraries}")
  set(CMAKE_REQUIRED_DEFINITIONS "${_pcre2_saved_required_definitions}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE
  REQUIRED_VARS PCRE_LIBRARY PCRE_INCLUDE_DIR PCRE2_VERSION_OK PCRE2_API_PROBE
  VERSION_VAR PCRE_VERSION)

mark_as_advanced(PCRE_INCLUDE_DIR PCRE_LIBRARY)
