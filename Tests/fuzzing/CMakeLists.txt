# Comprehensive Fuzzing Suite for CriptoGualet
# Requires libFuzzer (included with Clang) or standalone libFuzzer library
# Cross-platform: Windows, Linux, macOS

option(ENABLE_FUZZING "Enable fuzzing targets" OFF)
option(FUZZING_BUILD "Build for fuzzing (disables some safety checks)" OFF)

if(ENABLE_FUZZING)
    # Check for libFuzzer support
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Fuzzing enabled with Clang/libFuzzer")
        # Note: Sanitizer flags are already applied globally in Tests/CMakeLists.txt
    else()
        message(FATAL_ERROR "Fuzzing requires Clang compiler with libFuzzer support. "
                            "Please use Clang or disable fuzzing with -DENABLE_FUZZING=OFF")
    endif()

    # Platform-specific libraries
    if(WIN32)
        set(FUZZING_PLATFORM_LIBS ws2_32 wsock32 bcrypt crypt32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(FUZZING_PLATFORM_LIBS pthread dl)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")  # macOS
        set(FUZZING_PLATFORM_LIBS pthread)
    else()
        set(FUZZING_PLATFORM_LIBS "")
    endif()

    # Fuzzing support libraries
    set(FUZZING_LIBS
        Crypto
        Database
        Repository
        SharedSymbols
        BlockCypher
    )

    if(TARGET WalletAPI)
        list(APPEND FUZZING_LIBS WalletAPI)
    endif()

    # Helper function to create fuzzer targets
    function(add_fuzzer_target target_name source_file)
        add_executable(${target_name} ${source_file})
        
        target_include_directories(${target_name} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/harnesses"
            "${CMAKE_SOURCE_DIR}/backend/core/include"
            "${CMAKE_SOURCE_DIR}/backend/utils/include"
            "${CMAKE_SOURCE_DIR}/backend/blockchain/include"
            "${CMAKE_SOURCE_DIR}/backend/database/include"
            "${CMAKE_SOURCE_DIR}/backend/repository/include"
        )

        # Target-specific compile options (sanitizer flags are already applied globally)
        target_compile_options(${target_name} PRIVATE
            -O1
            -g
        )

        # MSVC-specific: Set runtime library per-target
        # Note: Fuzzer libraries are built with MT_StaticRelease, so we must match
        if(MSVC)
            set_property(TARGET ${target_name} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded"
            )
            # Iterator debug level for compatibility
            target_compile_definitions(${target_name} PRIVATE
                _ITERATOR_DEBUG_LEVEL=0
            )
        endif()

        target_link_libraries(${target_name} PRIVATE 
            ${FUZZING_LIBS}
            ${FUZZING_PLATFORM_LIBS}
        )

        # On Windows with Visual Studio, link sanitizer runtime libraries explicitly
        if(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio" AND DEFINED FUZZER_SANITIZER_LIBS)
            target_link_libraries(${target_name} PRIVATE 
                ${FUZZER_SANITIZER_LIBS}
            )
        endif()

        # Set properties for fuzzing
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzzers"
        )

        # Add to CTest with short timeout (fuzzing tests run indefinitely)
        # These are meant to be run manually or in CI with time limits
        add_test(NAME ${target_name}_smoke 
                 COMMAND ${target_name} -max_total_time=30 -max_len=4096
                 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzzers")
        
        set_tests_properties(${target_name}_smoke PROPERTIES
            TIMEOUT 60
            LABELS "fuzzing"
        )
    endfunction()

    # Address Parsing Fuzzer
    add_fuzzer_target(fuzz_address_parsing "harnesses/fuzz_address_parsing.cpp")

    # Transaction Parsing Fuzzer
    add_fuzzer_target(fuzz_transaction_parsing "harnesses/fuzz_transaction_parsing.cpp")

    # Cryptographic Operations Fuzzer
    add_fuzzer_target(fuzz_crypto_operations "harnesses/fuzz_crypto_operations.cpp")

    # Database Input Fuzzer
    add_fuzzer_target(fuzz_database_input "harnesses/fuzz_database_input.cpp")

    # JSON/API Response Fuzzer
    add_fuzzer_target(fuzz_json_parsing "harnesses/fuzz_json_parsing.cpp")

    # Comprehensive Wallet Fuzzer (end-to-end)
    add_fuzzer_target(fuzz_wallet_operations "harnesses/fuzz_wallet_operations.cpp")

    # Copy corpus directories to build
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/corpus" 
         DESTINATION "${CMAKE_BINARY_DIR}/fuzzers")

    # Custom target to run all fuzzers with time limits
    add_custom_target(run_fuzzers_5min
        COMMENT "Running all fuzzers for 5 minutes each"
    )

    add_custom_target(run_fuzzers_1hour
        COMMENT "Running all fuzzers for 1 hour each"
    )

    foreach(fuzzer 
            fuzz_address_parsing 
            fuzz_transaction_parsing 
            fuzz_crypto_operations 
            fuzz_database_input 
            fuzz_json_parsing
            fuzz_wallet_operations)
        
        add_custom_command(TARGET run_fuzzers_5min POST_BUILD
            COMMAND ${fuzzer} -max_total_time=300 corpus/${fuzzer}
            COMMENT "Running ${fuzzer} for 5 minutes"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzzers"
        )

        add_custom_command(TARGET run_fuzzers_1hour POST_BUILD
            COMMAND ${fuzzer} -max_total_time=3600 corpus/${fuzzer}
            COMMENT "Running ${fuzzer} for 1 hour"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzzers"
        )
    endforeach()

    # Corpus minimization target
    add_custom_target(minimize_corpus
        COMMENT "Minimizing fuzzing corpus"
    )

    foreach(fuzzer 
            fuzz_address_parsing 
            fuzz_transaction_parsing 
            fuzz_crypto_operations 
            fuzz_database_input 
            fuzz_json_parsing
            fuzz_wallet_operations)
        
        add_custom_command(TARGET minimize_corpus POST_BUILD
            COMMAND ${fuzzer} -merge=1 corpus/${fuzzer}_minimized corpus/${fuzzer}
            COMMENT "Minimizing corpus for ${fuzzer}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/fuzzers"
        )
    endforeach()

    message(STATUS "Fuzzing targets configured:")
    message(STATUS "  - fuzz_address_parsing")
    message(STATUS "  - fuzz_transaction_parsing")
    message(STATUS "  - fuzz_crypto_operations")
    message(STATUS "  - fuzz_database_input")
    message(STATUS "  - fuzz_json_parsing")
    message(STATUS "  - fuzz_wallet_operations")
    message(STATUS "  - run_fuzzers_5min (run all fuzzers for 5 minutes)")
    message(STATUS "  - run_fuzzers_1hour (run all fuzzers for 1 hour)")
    message(STATUS "  - minimize_corpus (merge and minimize corpus)")
    message(STATUS "  - Platform: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  - Compiler: ${CMAKE_CXX_COMPILER_ID}")

endif()
