cmake_minimum_required(VERSION 3.20)

# Declare the project EARLY so VS presets/toolsets apply cleanly
project(CriptoGualet LANGUAGES CXX)

# Make MSVC runtime selection controllable & stable across configs
if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
# Debug uses /MDd by default (matches VS Debug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")

# Put built EXEs/DLLs in a predictable place: <build>/bin
# (launch.vs.json uses this)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# -------------------------------
# Compiler detection - try to find a suitable compiler
# -------------------------------
if (NOT DEFINED CMAKE_CXX_COMPILER)
  # Check standard installation locations
  find_program(CLANG_EXECUTABLE NAMES clang
    PATHS "C:/Program Files/LLVM/bin"
    NO_DEFAULT_PATH)
  find_program(CLANGXX_EXECUTABLE NAMES clang++
    PATHS "C:/Program Files/LLVM/bin"
    NO_DEFAULT_PATH)

  # If not found in standard location, try system PATH
  if(NOT CLANG_EXECUTABLE OR NOT CLANGXX_EXECUTABLE)
    find_program(CLANG_EXECUTABLE NAMES clang)
    find_program(CLANGXX_EXECUTABLE NAMES clang++)
  endif()

  # Try clang-cl for MSVC compatibility on Windows
  find_program(CLANGCL_EXECUTABLE NAMES clang-cl
    PATHS "C:/Program Files/LLVM/bin"
    NO_DEFAULT_PATH)
  if(NOT CLANGCL_EXECUTABLE)
    find_program(CLANGCL_EXECUTABLE NAMES clang-cl)
  endif()

  if(CLANGCL_EXECUTABLE AND WIN32)
    set(CMAKE_C_COMPILER ${CLANGCL_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${CLANGCL_EXECUTABLE})
    message(STATUS "Using Clang-CL: ${CLANGCL_EXECUTABLE}")
  elseif(CLANG_EXECUTABLE AND CLANGXX_EXECUTABLE)
    set(CMAKE_C_COMPILER ${CLANG_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${CLANGXX_EXECUTABLE})
    message(STATUS "Using Clang.")
    message(STATUS "Clang C compiler: ${CLANG_EXECUTABLE}")
    message(STATUS "Clang C++ compiler: ${CLANGXX_EXECUTABLE}")
  else()
    # Fall back to system default compiler detection
    find_program(MSVC_CL NAMES cl.exe)
    if(MSVC_CL AND WIN32)
      set(CMAKE_C_COMPILER ${MSVC_CL})
      set(CMAKE_CXX_COMPILER ${MSVC_CL})
      message(STATUS "Using MSVC compiler: ${MSVC_CL}")
    else()
      message(WARNING "No suitable compiler found. Please ensure a C++ compiler is installed.")
    endif()
  endif()
else()
  message(STATUS "Compiler already specified: ${CMAKE_CXX_COMPILER}")
endif()

# Enable vcpkg integration (matches your presets if VCPKG_ROOT is set)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

# Set C++ standard (Qt 6.9.1 requires C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add MSVC-specific flags for Qt compatibility
if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  add_compile_options(/Zc:__cplusplus)
endif()

# Cross-platform compiler flags for Clang (only when using Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Note: ASan with MSVC/Clang-CL may require matching runtime; ok outside VS.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  endif()

  if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-compatibility-version=19")
  endif()
endif()

# Build options
option(BUILD_GUI_QT "Build the Qt GUI wallet app" ON)

# Find Qt6 if GUI is enabled
if(BUILD_GUI_QT)
  if(WIN32)
    # Add common Qt roots; exact ABI will be resolved by find_package
    list(APPEND CMAKE_PREFIX_PATH
      "C:/Program Files/Qt"
      "C:/Qt"
      "C:/Program Files (x86)/Qt"
    )
    # Try to help CMake locate the right msvc/clang build of Qt
    file(GLOB QT_VERSIONS "C:/Program Files/Qt/6.*")
    foreach(QT_VERSION ${QT_VERSIONS})
      if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        list(APPEND CMAKE_PREFIX_PATH
          "${QT_VERSION}/msvc2022_64"
          "${QT_VERSION}/msvc2019_64")
      elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND CMAKE_PREFIX_PATH
          "${QT_VERSION}/msvc2022_64"
          "${QT_VERSION}/msvc2019_64"
          "${QT_VERSION}/clang_64"
          "${QT_VERSION}/llvm-mingw_64")
      else()
        list(APPEND CMAKE_PREFIX_PATH
          "${QT_VERSION}/mingw_64"
          "${QT_VERSION}/llvm-mingw_64"
          "${QT_VERSION}/clang_64")
      endif()
    endforeach()
  endif()

  find_package(Qt6 QUIET COMPONENTS Core Widgets Gui)
  if(Qt6_FOUND)
    qt6_standard_project_setup()
    message(STATUS "Qt6 found at ${Qt6_DIR}")
    message(STATUS "Qt GUI will be built")
  else()
    message(WARNING "Qt6 not found - Qt GUI will not be built. Install Qt6 to enable Qt GUI.")
    set(BUILD_GUI_QT OFF)
  endif()
endif()

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Delegate to src/
add_subdirectory(src)

# Set startup project for Visual Studio (must be after add_subdirectory)
if(BUILD_GUI_QT AND Qt6_FOUND)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualetQt)
endif()

# Add debug test executable
add_executable(debug_test debug_test.cpp src/SharedSymbols.cpp)
target_link_libraries(debug_test PRIVATE Auth comctl32 crypt32 bcrypt)
target_include_directories(debug_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add BIP39 test executable
add_executable(test_bip39 test_bip39.cpp src/SharedSymbols.cpp)
target_link_libraries(test_bip39 PRIVATE Auth comctl32 crypt32 bcrypt)
target_include_directories(test_bip39 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
