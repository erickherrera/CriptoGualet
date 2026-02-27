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

# Try to find SQLCipher
# First, check for system-installed SQLCipher on Linux
if(NOT WIN32)
  find_path(SQLCIPHER_INCLUDE_DIR sqlcipher/sqlite3.h PATHS /usr/include)
  find_library(SQLCIPHER_LIBRARY NAMES sqlcipher PATHS /usr/lib/x86_64-linux-gnu)
  
  if(SQLCIPHER_INCLUDE_DIR AND SQLCIPHER_LIBRARY)
    set(sqlcipher_FOUND TRUE)
    set(sqlcipher_SYSTEM TRUE)
    message(STATUS "Database library found system SQLCipher: ${SQLCIPHER_LIBRARY}")
  endif()
endif()

# If not found system-wide, try vcpkg
if(NOT sqlcipher_FOUND)
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
        message(STATUS "Database library found SQLCipher via pkg-config")
      endif()
    endif()
  endif()
endif()

if(sqlcipher_FOUND)
  if(sqlcipher_SYSTEM)
    target_include_directories(Database PUBLIC ${SQLCIPHER_INCLUDE_DIR})
    target_link_libraries(Database PUBLIC ${SQLCIPHER_LIBRARY})
  else()
    target_link_libraries(Database PUBLIC sqlcipher::sqlcipher)
  endif()
  target_compile_definitions(Database PUBLIC SQLCIPHER_AVAILABLE)
else()
  message(FATAL_ERROR "SQLCipher is required but was not found. Install with: sudo apt-get install libsqlcipher-dev (Linux) or vcpkg install sqlcipher --allow-unsupported")
endif()
