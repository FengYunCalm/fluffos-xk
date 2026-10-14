if (PACKAGE_DB)
    add_library(package_db STATIC
            "db.cc"
            "db.h")

    set(_db_enabled_ids)
    foreach (_db_variable PACKAGE_DB_MYSQL PACKAGE_DB_SQLITE PACKAGE_DB_POSTGRESQL)
      set(_db_value "${${_db_variable}}")
      string(TOUPPER "${_db_value}" _db_upper_value)
      if ("${_db_upper_value}" STREQUAL "" OR
          "${_db_upper_value}" STREQUAL "0" OR
          "${_db_upper_value}" STREQUAL "OFF" OR
          "${_db_upper_value}" STREQUAL "NO" OR
          "${_db_upper_value}" STREQUAL "FALSE" OR
          "${_db_upper_value}" STREQUAL "N" OR
          "${_db_upper_value}" STREQUAL "IGNORE" OR
          "${_db_upper_value}" STREQUAL "NOTFOUND" OR
          "${_db_upper_value}" MATCHES "-NOTFOUND$")
        continue()
      endif()

      if (NOT "${_db_value}" MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
                "${_db_variable} must be a positive decimal integer between 1 and 2147483647; "
                "use an empty string, 0, or OFF to disable it (got '${_db_value}').")
      endif()
      string(LENGTH "${_db_value}" _db_value_length)
      if (_db_value_length GREATER 10 OR
          (_db_value_length EQUAL 10 AND "${_db_value}" GREATER 2147483647))
        message(FATAL_ERROR
                "${_db_variable} must be no greater than 2147483647 (got '${_db_value}').")
      endif()

      string(REPLACE "PACKAGE_DB_" "" _db_backend "${_db_variable}")
      string(TOLOWER "${_db_backend}" _db_backend_lower)
      set(_db_${_db_backend_lower}_enabled TRUE)
      list(APPEND _db_enabled_ids "${_db_value}")
    endforeach()

    list(LENGTH _db_enabled_ids _db_id_count)
    list(REMOVE_DUPLICATES _db_enabled_ids)
    list(LENGTH _db_enabled_ids _db_unique_id_count)
    if (_db_id_count EQUAL 0)
      message(FATAL_ERROR "PACKAGE_DB is enabled but no database backend is enabled.")
    elseif (_db_id_count GREATER _db_unique_id_count)
      message(FATAL_ERROR "Enabled database backends must use unique type IDs.")
    endif()

    if (NOT "${PACKAGE_DB_DEFAULT_DB}" MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR
              "PACKAGE_DB_DEFAULT_DB must be a positive decimal integer between 1 and 2147483647; "
              "got '${PACKAGE_DB_DEFAULT_DB}'.")
    endif()
    string(LENGTH "${PACKAGE_DB_DEFAULT_DB}" _db_default_length)
    if (_db_default_length GREATER 10 OR
        (_db_default_length EQUAL 10 AND "${PACKAGE_DB_DEFAULT_DB}" GREATER 2147483647))
      message(FATAL_ERROR
              "PACKAGE_DB_DEFAULT_DB must be no greater than 2147483647 "
              "(got '${PACKAGE_DB_DEFAULT_DB}').")
    endif()
    list(FIND _db_enabled_ids "${PACKAGE_DB_DEFAULT_DB}" _db_default_index)
    if (_db_default_index EQUAL -1)
      message(FATAL_ERROR
              "PACKAGE_DB_DEFAULT_DB must match an enabled database backend type ID "
              "(got '${PACKAGE_DB_DEFAULT_DB}').")
    endif()

    target_compile_definitions(package_db PUBLIC "DEFAULT_DB=${PACKAGE_DB_DEFAULT_DB}")

    if (_db_mysql_enabled)
        message(STATUS "PACKAGE_DB USE_MYSQL: ${PACKAGE_DB_MYSQL}")
        target_compile_options(package_db PUBLIC "${MYSQL_CXXFLAGS}")
        target_compile_definitions(package_db PUBLIC "USE_MYSQL=${PACKAGE_DB_MYSQL}")

        if (STATIC)
          set(MYSQLCLIENT_STATIC_LINKING TRUE)
        endif()

        find_package(MySQL REQUIRED)
        target_include_directories(package_db SYSTEM PUBLIC ${MYSQL_INCLUDE_DIR})

        target_link_libraries(package_db PRIVATE ${MYSQL_LINK_FLAGS})
        target_link_libraries(package_db PRIVATE -L${MYSQL_LIB_DIR} ${MYSQL_LIBRARIES})
    endif()

    if (_db_sqlite_enabled)
      find_package(SQLite3 REQUIRED)
      message(STATUS "PACKAGE_DB USE_SQLITE3: ${PACKAGE_DB_SQLITE}, Found version ${SQLite3_VERSION}")

      target_compile_definitions(package_db PUBLIC "USE_SQLITE3=${PACKAGE_DB_SQLITE}")
      target_include_directories(package_db SYSTEM PUBLIC ${SQLite3_INCLUDE_DIRS})
      target_link_libraries(package_db PRIVATE ${SQLite3_LIBRARIES})
    endif()

    if (_db_postgresql_enabled)
      find_package(PostgreSQL REQUIRED)
      message(STATUS "PACKAGE_DB USE_POSTGRES: ${PACKAGE_DB_POSTGRESQL}, Found version ${PostgreSQL_VERSION_STRING}")

      target_compile_definitions(package_db PUBLIC "USE_POSTGRES=${PACKAGE_DB_POSTGRESQL}")
      target_include_directories(package_db SYSTEM PUBLIC ${PostgreSQL_INCLUDE_DIRS})
      target_link_libraries(package_db PRIVATE -L${PostgreSQL_LIBRARY_DIRS} ${PostgreSQL_LIBRARIES})
    endif()

endif()
