# Test Utilities Library
add_library(TestUtils STATIC
  "src/TestUtils.cpp"
)
target_include_directories(TestUtils PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(TestUtils PRIVATE Auth Crypto Database Repository SharedSymbols)
if(TARGET WalletAPI)
  target_link_libraries(TestUtils PRIVATE WalletAPI)
endif()

set(TEST_SUPPORT_LIBS
  Auth
  Crypto
  Database
  Repository
  SharedSymbols
  SecureCredentialStore
  QRGenerator
)
if(TARGET WalletAPI)
  list(APPEND TEST_SUPPORT_LIBS WalletAPI)
endif()

function(configure_criptogualet_test target_name)
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(${target_name} PRIVATE TestUtils ${TEST_SUPPORT_LIBS})

  if(WIN32)
    target_link_libraries(${target_name} PRIVATE ws2_32 wsock32 bcrypt crypt32)
  endif()

  target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
    $<$<COMPILE_LANGUAGE:CXX>:/Wall>
  )
endfunction()

function(add_criptogualet_test target_name source_file)
  add_executable(${target_name} ${source_file})
  configure_criptogualet_test(${target_name})
  add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

function(add_criptogualet_tool target_name source_file)
  add_executable(${target_name} ${source_file})
  configure_criptogualet_test(${target_name})
endfunction()

# Session Test Executables
add_criptogualet_test(test_session_consolidated "test_session_consolidated.cpp")

# Find SQLite3 for repository tests
find_package(SQLite3)

# Consolidated repository tests
add_criptogualet_test(test_repository_consolidated "test_repository_consolidated.cpp")
if(SQLITE3_FOUND)
  target_link_libraries(test_repository_consolidated PRIVATE SQLite::SQLite3)
endif()

# Additional test executables (launch manually / set as startup)
add_criptogualet_tool(test_password_verification "test_password_verification.cpp")
add_criptogualet_tool(test_wallet_chains "test_wallet_chains.cpp")
add_criptogualet_tool(test_secure_seed "test_secure_seed.cpp")
add_criptogualet_tool(test_bip39 "test_bip39.cpp")
add_criptogualet_tool(test_security_enhancements "test_security_enhancements.cpp")
add_criptogualet_tool(test_database "test_database.cpp")
add_criptogualet_tool(test_2fa "test_2fa.cpp")

if(TARGET BlockCypher)
  add_criptogualet_tool(test_blockcypher_api "test_blockcypher_api.cpp")
  target_link_libraries(test_blockcypher_api PRIVATE BlockCypher)
endif()

# Custom target to run all session tests
add_custom_target(run_all_session_tests
    COMMAND test_session_consolidated
    COMMENT "Run all session management tests"
    DEPENDS test_session_consolidated
)

# -----------------------------------------------------------------------------
# Installation - Bundle test executables for QtTestConsole diagnostics
# -----------------------------------------------------------------------------
# These executables are needed by the "System Diagnostics & Security Verification"
# feature in the Qt GUI (QtTestConsole.cpp)

if(WIN32)
  set(TEST_EXE_SUFFIX ".exe")
endif()

# Install test executables to bin directory
set(TEST_EXECUTABLES
  test_password_verification
  test_wallet_chains
  test_secure_seed
  test_bip39
  test_security_enhancements
  test_2fa
  test_database
  test_session_consolidated
  test_repository_consolidated
)

if(TARGET BlockCypher)
  list(APPEND TEST_EXECUTABLES test_blockcypher_api)
endif()

foreach(TEST_TARGET ${TEST_EXECUTABLES})
  if(TARGET ${TEST_TARGET})
    install(TARGETS ${TEST_TARGET}
            RUNTIME DESTINATION bin
            COMPONENT Runtime)
  endif()
endforeach()
