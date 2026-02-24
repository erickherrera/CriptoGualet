# Database Library (Encrypted SQLCipher-based storage)

add_library(Database STATIC
  "src/DatabaseManager.cpp"
  "include/Database/DatabaseManager.h"
)
target_include_directories(Database PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Link Windows bcrypt library for cryptographic functions
if(WIN32)
  target_link_libraries(Database PRIVATE bcrypt)
else()
  find_package(unofficial-sodium CONFIG REQUIRED)
  target_link_libraries(Database PRIVATE unofficial-sodium::sodium)
endif()

# Try to find SQLCipher using CMake package first
find_package(sqlcipher QUIET)
if(NOT sqlcipher_FOUND AND NOT WIN32)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(SQLCIPHER QUIET sqlcipher)
    if(SQLCIPHER_FOUND)
      set(sqlcipher_FOUND TRUE)
      add_library(sqlcipher::sqlcipher INTERFACE IMPORTED)
      target_link_libraries(sqlcipher::sqlcipher INTERFACE ${SQLCIPHER_LIBRARIES})
      target_include_directories(sqlcipher::sqlcipher INTERFACE ${SQLCIPHER_INCLUDE_DIRS})
      target_link_options(sqlcipher::sqlcipher INTERFACE ${SQLCIPHER_LDFLAGS})
    endif()
  endif()
endif()

if(sqlcipher_FOUND)
  target_link_libraries(Database PRIVATE sqlcipher::sqlcipher)
  target_compile_definitions(Database PRIVATE SQLCIPHER_AVAILABLE)
  message(STATUS "Database library found SQLCipher via CMake package")
else()
  # Fallback to manual linking
  find_path(SQLCIPHER_INCLUDE_DIR sqlcipher/sqlite3.h)
  find_library(SQLCIPHER_LIBRARY NAMES sqlcipher)

  if(SQLCIPHER_INCLUDE_DIR AND SQLCIPHER_LIBRARY)
    target_include_directories(Database PRIVATE ${SQLCIPHER_INCLUDE_DIR})
    target_link_libraries(Database PRIVATE ${SQLCIPHER_LIBRARY})

    # SQLCipher needs OpenSSL - add it explicitly
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
      target_link_libraries(Database PRIVATE OpenSSL::Crypto OpenSSL::SSL)
      message(STATUS "Linked OpenSSL for SQLCipher support")
    endif()

    target_compile_definitions(Database PRIVATE SQLCIPHER_AVAILABLE)
    message(STATUS "Database library found SQLCipher via vcpkg: ${SQLCIPHER_LIBRARY}")
  else()
    # Fallback to regular SQLite3
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY NAMES sqlite3)

    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
      target_include_directories(Database PRIVATE ${SQLITE3_INCLUDE_DIR})
      target_link_libraries(Database PRIVATE ${SQLITE3_LIBRARY})
      message(STATUS "Database library found SQLite3 (fallback): ${SQLITE3_LIBRARY}")
      message(WARNING "Using SQLite3 without encryption - install SQLCipher for production use")
    else()
      message(WARNING "Neither SQLCipher nor SQLite3 found - Database library will not be functional")
      message(STATUS "Run 'vcpkg install sqlcipher sqlite3' to install database dependencies")
    endif()
  endif()
endif()
