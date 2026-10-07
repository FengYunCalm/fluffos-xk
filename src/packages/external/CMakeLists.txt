if(${PACKAGE_EXTERNAL})
  add_library(package_external STATIC
          "external.cc"
          "external.h"
          "socketpair.cc"
          )
  if(NOT PACKAGE_SOCKETS)
    # The classic external callbacks still use the internal socket backend.
    target_sources(package_external PRIVATE "../sockets/socket_efuns.cc")
  endif()
endif()
