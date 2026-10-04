# Make Func
find_package(BISON)
if (BISON_FOUND)
  BISON_TARGET(MakeFunc make_func.y ${CMAKE_CURRENT_BINARY_DIR}/make_func.cc)
  set(MAKE_FUNC_SOURCE ${BISON_MakeFunc_OUTPUT_SOURCE})
else()
  message(STATUS "BISON: make_func uses pre-generated source.")
  set(MAKE_FUNC_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/make_func.autogen.cc")
endif()
add_executable(make_func ${MAKE_FUNC_SOURCE})

#build_applies
add_executable(build_applies "build_applies.cc")

add_executable(make_options_defs "make_options_defs.cc" "preprocessor.hpp")