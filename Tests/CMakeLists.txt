# Test Executables

# ---- Debug Test Executable (Basic Auth) ----
add_executable(debug_test
  "test_basic_auth.cpp"
)
target_link_libraries(debug_test PRIVATE Auth SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(debug_test PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
)

# ---- BIP39 Test Executable ----
add_executable(test_bip39
  "test_bip39.cpp"
)
target_link_libraries(test_bip39 PRIVATE Auth SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_bip39 PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
)

# ---- Secure Seed Test Executable ----
add_executable(test_secure_seed
  "test_secure_seed.cpp"
)
target_link_libraries(test_secure_seed PRIVATE Auth QRGenerator SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_secure_seed PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
)

# Copy qrencode DLLs for test_secure_seed on Windows
if(WIN32 AND LIBQRENCODE_FOUND)
  add_custom_command(TARGET test_secure_seed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/qrencoded.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/qrencode.dll>"
    "$<TARGET_FILE_DIR:test_secure_seed>"
    COMMENT "Copying qrencode DLL for test_secure_seed")

  add_custom_command(TARGET test_secure_seed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/libpng16d.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libpng16.dll>"
    "$<TARGET_FILE_DIR:test_secure_seed>"
    COMMENT "Copying libpng16 DLL for test_secure_seed")

  add_custom_command(TARGET test_secure_seed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/zlibd1.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/zlib1.dll>"
    "$<TARGET_FILE_DIR:test_secure_seed>"
    COMMENT "Copying zlib DLL for test_secure_seed")
endif()

# ---- BlockCypher API Test Executable ----
if(BLOCKCYPHER_AVAILABLE)
  add_executable(test_blockcypher_api
    "test_blockcypher_api.cpp"
  )
  target_link_libraries(test_blockcypher_api PRIVATE WalletAPI BlockCypher SharedSymbols comctl32 crypt32 bcrypt)
  target_include_directories(test_blockcypher_api PRIVATE
    "${CMAKE_SOURCE_DIR}/backend/core/include"
    "${CMAKE_SOURCE_DIR}/backend/blockchain/include"
  )

  # Copy CPR and curl DLLs for test executable on Windows
  if(WIN32)
    add_custom_command(TARGET test_blockcypher_api POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/cpr.dll"
      "$<TARGET_FILE_DIR:test_blockcypher_api>"
      COMMENT "Copying CPR DLL for test_blockcypher_api")

    add_custom_command(TARGET test_blockcypher_api POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/libcurl-d.dll"
      "$<TARGET_FILE_DIR:test_blockcypher_api>"
      COMMENT "Copying libcurl DLL for test_blockcypher_api")

    add_custom_command(TARGET test_blockcypher_api POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/charset-1.dll"
      "$<TARGET_FILE_DIR:test_blockcypher_api>"
      COMMENT "Copying charset DLL for test_blockcypher_api")

    add_custom_command(TARGET test_blockcypher_api POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/iconv-2.dll"
      "$<TARGET_FILE_DIR:test_blockcypher_api>"
      COMMENT "Copying iconv DLL for test_blockcypher_api")
  endif()
endif()

# ---- Database Test Executable ----
add_executable(test_database
  "test_database.cpp"
)
target_link_libraries(test_database PRIVATE Database)
target_include_directories(test_database PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/database/include"
)

# Copy SQLCipher DLL for test execution
if(WIN32 AND sqlcipher_FOUND)
  add_custom_command(TARGET test_database POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
    "$<TARGET_FILE_DIR:test_database>"
    COMMENT "Copying SQLCipher DLL for test execution")
endif()

# Cross-platform compiler flags with security for test executable
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_database PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_database PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_database PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_database PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_database PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_database PRIVATE -fPIE -pie)
endif()

# ---- Security Enhancements Test Executable ----
add_executable(test_security_enhancements
  "test_security_enhancements.cpp"
)
target_link_libraries(test_security_enhancements PRIVATE Crypto)
target_include_directories(test_security_enhancements PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
)

# Cross-platform compiler flags with security for security test executable
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_security_enhancements PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_security_enhancements PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_security_enhancements PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_security_enhancements PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_security_enhancements PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_security_enhancements PRIVATE -fPIE -pie)
endif()

# ---- Repository Test Executable ----
add_executable(test_repository
  "test_repository.cpp"
)
target_link_libraries(test_repository PRIVATE Repository)
target_include_directories(test_repository PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/core/include"
)

# Cross-platform compiler flags with security for repository test executable
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_repository PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_repository PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_repository PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_repository PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_repository PRIVATE -fPIE -pie)
endif()

# ---- Auth + Database Integration Test Executable ----
add_executable(test_auth_database_integration
  "test_auth_database_integration.cpp"
)
target_link_libraries(test_auth_database_integration PRIVATE Auth Repository Database Crypto SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_auth_database_integration PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
)

# Copy SQLCipher DLL for test execution
if(WIN32 AND sqlcipher_FOUND)
  add_custom_command(TARGET test_auth_database_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
    "$<TARGET_FILE_DIR:test_auth_database_integration>"
    COMMENT "Copying SQLCipher DLL for test execution")
endif()

# Cross-platform compiler flags with security for auth integration test executable
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_auth_database_integration PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_auth_database_integration PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_auth_database_integration PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_auth_database_integration PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_auth_database_integration PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_auth_database_integration PRIVATE -fPIE -pie)
endif()

# ---- BIP32/BIP44 Transaction Test Executable ----
# Find secp256k1 library (same as backend/core)
find_package(unofficial-secp256k1 CONFIG REQUIRED)

add_executable(test_bip32_transaction
  "test_bip32_transaction.cpp"
)

target_link_libraries(test_bip32_transaction PRIVATE
  Crypto
  unofficial::secp256k1
  unofficial::secp256k1_precomputed
  comctl32
  crypt32
  bcrypt
)

target_include_directories(test_bip32_transaction PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
)

# Cross-platform compiler flags with security for BIP32 transaction test
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_bip32_transaction PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_bip32_transaction PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_bip32_transaction PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_bip32_transaction PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_bip32_transaction PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_bip32_transaction PRIVATE -fPIE -pie)
endif()

# ==============================================================================
# Repository Unit Tests (Phase 3)
# ==============================================================================

# ---- TestUtils Library ----
# Shared utilities for all repository tests
add_library(TestUtils STATIC
  "src/TestUtils.cpp"
)
target_include_directories(TestUtils PUBLIC
  "${CMAKE_SOURCE_DIR}/Tests/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/core/include"
)
target_link_libraries(TestUtils PUBLIC Repository Database Crypto)

# Helper macro for repository test configuration
macro(add_repository_test target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE TestUtils Repository Database Crypto)
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/Tests/include"
    "${CMAKE_SOURCE_DIR}/backend/repository/include"
    "${CMAKE_SOURCE_DIR}/backend/database/include"
    "${CMAKE_SOURCE_DIR}/backend/core/include"
  )

  # Copy SQLCipher DLL for test execution on Windows
  if(WIN32 AND sqlcipher_FOUND)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
      "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying SQLCipher DLL for ${target_name}")
  endif()

  # Cross-platform compiler flags
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
    target_link_options(${target_name} PRIVATE -fPIE -pie)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
  elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
    target_link_options(${target_name} PRIVATE -fPIE -pie)
  endif()
endmacro()

# ---- UserRepository Tests ----
add_repository_test(test_user_repository "test_user_repository.cpp")

# ---- WalletRepository Tests ----
add_repository_test(test_wallet_repository "test_wallet_repository.cpp")

# ---- TransactionRepository Tests ----
add_repository_test(test_transaction_repository "test_transaction_repository.cpp")

# ---- Repository Integration Tests ----
add_repository_test(test_repository_integration "test_repository_integration.cpp")
