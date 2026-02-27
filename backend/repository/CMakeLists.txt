# Repository Library (Data Access Layer)

add_library(Repository STATIC
  "src/UserRepository.cpp"
  "src/WalletRepository.cpp"
  "src/TransactionRepository.cpp"
  "src/TokenRepository.cpp"
  "src/Logger.cpp"
  "src/SessionRepository.cpp"
  "src/SettingsRepository.cpp"
  "include/Repository/UserRepository.h"
  "include/Repository/WalletRepository.h"
  "include/Repository/TransactionRepository.h"
  "include/Repository/TokenRepository.h"
  "include/Repository/Logger.h"
  "include/Repository/RepositoryTypes.h"
  "include/Repository/SessionRepository.h"
  "include/Repository/SettingsRepository.h"
)
target_include_directories(Repository PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(Repository PRIVATE Crypto Database)

# Repository uses sqlite3_stmt directly which requires SQLCipher headers
# SQLCipher is already linked via Database library - headers are inherited

# Verify SQLCipher is available (via Database dependency)
if(NOT SQLCIPHER_AVAILABLE)
  # Check for system SQLCipher as fallback
  if(NOT WIN32)
    find_path(SQLCIPHER_INCLUDE_DIR sqlcipher/sqlite3.h PATHS /usr/include)
    find_library(SQLCIPHER_LIBRARY NAMES sqlcipher PATHS /usr/lib/x86_64-linux-gnu)
    if(SQLCIPHER_INCLUDE_DIR AND SQLCIPHER_LIBRARY)
      target_include_directories(Repository PUBLIC ${SQLCIPHER_INCLUDE_DIR})
      target_link_libraries(Repository PUBLIC ${SQLCIPHER_LIBRARY})
      target_compile_definitions(Repository PUBLIC SQLCIPHER_AVAILABLE)
    endif()
  endif()
endif()

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIC -Wno-unused-parameter)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(Repository PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(Repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIC)
endif()
