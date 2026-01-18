# Test Utilities Library
add_library(TestUtils STATIC
  "src/TestUtils.cpp"
)
target_include_directories(TestUtils PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(TestUtils PRIVATE Auth Crypto WalletAPI Database Repository SharedSymbols)

# Helper macro for session tests
macro(add_session_test target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE TestUtils)
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  if(WIN32)
    target_link_libraries(${target_name} PRIVATE ws2_32 wsock32)
  endif()

  # Set compiler flags for better error reporting
  target_compile_options(${target_name} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:/W4>
      $<$<COMPILE_LANGUAGE:CXX>:/Wall>
  )

  # Add tests to CTest
  add_test(NAME ${target_name} COMMAND ${target_name})
endmacro()

# Session Test Executables
add_session_test(test_session_manager "test_session_manager.cpp")
add_session_test(test_session_repository "test_session_repository.cpp")
add_session_test(test_session_integration "test_session_integration.cpp")
add_session_test(test_session_security "test_session_security.cpp")
add_session_test(test_user_session "test_user_session.cpp")

# Custom target to run all session tests
add_custom_target(run_all_session_tests
    COMMAND test_session_manager && test_session_repository && test_session_integration && test_session_security && test_user_session
    COMMENT "Run all session management tests"
    DEPENDS test_session_manager test_session_repository test_session_integration test_session_security test_user_session
)