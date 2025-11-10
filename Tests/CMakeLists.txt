# Test Executables

# ---- BIP39 Test Executable ----
add_executable(test_bip39
  "test_bip39.cpp"
)
target_link_libraries(test_bip39 PRIVATE Auth Crypto SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_bip39 PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
  "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include"
)

# ---- Ethereum Wallet Test Executable ----
add_executable(test_ethereum_wallet
  "test_ethereum_wallet.cpp"
)
target_link_libraries(test_ethereum_wallet PRIVATE Auth Crypto SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_ethereum_wallet PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
  "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include"
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
  target_link_libraries(test_blockcypher_api PRIVATE WalletAPI BlockCypher Crypto SharedSymbols comctl32 crypt32 bcrypt cpr::cpr)
  target_include_directories(test_blockcypher_api PRIVATE
    "${CMAKE_SOURCE_DIR}/backend/core/include"
    "${CMAKE_SOURCE_DIR}/backend/blockchain/include"
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include"
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

# Copy SQLCipher and dependency DLLs for test execution
if(WIN32)
  add_custom_command(TARGET test_database POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
    "$<TARGET_FILE_DIR:test_database>"
    COMMENT "Copying SQLCipher DLL for test execution")

  add_custom_command(TARGET test_database POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/sqlite3.dll"
    "$<TARGET_FILE_DIR:test_database>"
    COMMENT "Copying SQLite3 DLL for test execution")

  add_custom_command(TARGET test_database POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libcrypto-3-x64.dll"
    "$<TARGET_FILE_DIR:test_database>"
    COMMENT "Copying OpenSSL Crypto DLL for test execution")

  add_custom_command(TARGET test_database POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libssl-3-x64.dll"
    "$<TARGET_FILE_DIR:test_database>"
    COMMENT "Copying OpenSSL SSL DLL for test execution")
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

# Copy SQLCipher and dependency DLLs for test execution
if(WIN32)
  add_custom_command(TARGET test_auth_database_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
    "$<TARGET_FILE_DIR:test_auth_database_integration>"
    COMMENT "Copying SQLCipher DLL for test execution")

  add_custom_command(TARGET test_auth_database_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/sqlite3.dll"
    "$<TARGET_FILE_DIR:test_auth_database_integration>"
    COMMENT "Copying SQLite3 DLL for test execution")

  add_custom_command(TARGET test_auth_database_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libcrypto-3-x64.dll"
    "$<TARGET_FILE_DIR:test_auth_database_integration>"
    COMMENT "Copying OpenSSL Crypto DLL for test execution")

  add_custom_command(TARGET test_auth_database_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libssl-3-x64.dll"
    "$<TARGET_FILE_DIR:test_auth_database_integration>"
    COMMENT "Copying OpenSSL SSL DLL for test execution")
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

  # Copy SQLCipher and dependency DLLs for test execution on Windows
  if(WIN32)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
      "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying SQLCipher DLL for ${target_name}")

    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/sqlite3.dll"
      "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying SQLite3 DLL for ${target_name}")

    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libcrypto-3-x64.dll"
      "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying OpenSSL Crypto DLL for ${target_name}")

    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libssl-3-x64.dll"
      "$<TARGET_FILE_DIR:${target_name}>"
      COMMENT "Copying OpenSSL SSL DLL for ${target_name}")
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

# ---- Password Verification Test Tool ----
add_executable(test_password_verification
  "test_password_verification.cpp"
)
target_link_libraries(test_password_verification PRIVATE Auth Repository Database Crypto SharedSymbols comctl32 crypt32 bcrypt)
target_include_directories(test_password_verification PRIVATE
  "${CMAKE_SOURCE_DIR}/backend/core/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
)

# Copy SQLCipher and dependency DLLs for test execution
if(WIN32)
  add_custom_command(TARGET test_password_verification POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
    "$<TARGET_FILE_DIR:test_password_verification>"
    COMMENT "Copying SQLCipher DLL for test execution")

  add_custom_command(TARGET test_password_verification POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/sqlite3.dll"
    "$<TARGET_FILE_DIR:test_password_verification>"
    COMMENT "Copying SQLite3 DLL for test execution")

  add_custom_command(TARGET test_password_verification POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libcrypto-3-x64.dll"
    "$<TARGET_FILE_DIR:test_password_verification>"
    COMMENT "Copying OpenSSL Crypto DLL for test execution")

  add_custom_command(TARGET test_password_verification POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/libssl-3-x64.dll"
    "$<TARGET_FILE_DIR:test_password_verification>"
    COMMENT "Copying OpenSSL SSL DLL for test execution")
endif()

# Cross-platform compiler flags with security for password verification test
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(test_password_verification PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(test_password_verification PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(test_password_verification PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(test_password_verification PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(test_password_verification PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(test_password_verification PRIVATE -fPIE -pie)
endif()
