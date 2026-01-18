# Repository Library (Data Access Layer)

add_library(Repository STATIC
  "src/UserRepository.cpp"
  "src/WalletRepository.cpp"
  "src/TransactionRepository.cpp"
  "src/TokenRepository.cpp"
  "src/Logger.cpp"
  "src/SessionRepository.cpp"
  "include/Repository/UserRepository.h"
  "include/Repository/WalletRepository.h"
  "include/Repository/TransactionRepository.h"
  "include/Repository/TokenRepository.h"
  "include/Repository/Logger.h"
  "include/Repository/RepositoryTypes.h"
  "include/Repository/SessionRepository.h"
)
target_include_directories(Repository PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(Repository PRIVATE Crypto Database)

# Repository needs SQLite headers since it uses sqlite3_stmt directly
# Try to find SQLCipher using CMake package first
find_package(sqlcipher QUIET)
if(sqlcipher_FOUND)
  target_link_libraries(Repository PRIVATE sqlcipher::sqlcipher)
  target_compile_definitions(Repository PRIVATE SQLCIPHER_AVAILABLE)
else()
  # Fallback to manual linking
  find_path(SQLCIPHER_INCLUDE_DIR sqlcipher/sqlite3.h)
  find_library(SQLCIPHER_LIBRARY NAMES sqlcipher)

  if(SQLCIPHER_INCLUDE_DIR AND SQLCIPHER_LIBRARY)
    target_include_directories(Repository PRIVATE ${SQLCIPHER_INCLUDE_DIR})
    target_link_libraries(Repository PRIVATE ${SQLCIPHER_LIBRARY})

    # SQLCipher needs OpenSSL
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
      target_link_libraries(Repository PRIVATE OpenSSL::Crypto OpenSSL::SSL)
    endif()

    target_compile_definitions(Repository PRIVATE SQLCIPHER_AVAILABLE)
  else()
    # Fallback to regular SQLite3
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY NAMES sqlite3)

    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
      target_include_directories(Repository PRIVATE ${SQLITE3_INCLUDE_DIR})
      target_link_libraries(Repository PRIVATE ${SQLITE3_LIBRARY})
    endif()
  endif()
endif()

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Repository PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(Repository PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(Repository PRIVATE -fPIE -pie)
endif()
