cmake_minimum_required(VERSION 3.20)

# Declare the project EARLY so VS presets/toolsets apply cleanly
project(CriptoGualet LANGUAGES CXX)

# Make MSVC runtime selection controllable & stable across configs
if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
# Set MSVC runtime library based on build type
# For multi-config generators (Visual Studio), CMake handles this per-config automatically
# For single-config generators, set it based on CMAKE_BUILD_TYPE
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_CONFIGURATION_TYPES)
  # Single-config generator - set based on build type
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "")
  endif()
endif()

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
# Presets may already set CMAKE_TOOLCHAIN_FILE, so only set if not already defined
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Path to vcpkg toolchain file")
endif()

# Set C++ standard (matches CMakePresets.json)
# Allow override from presets, but default to C++20
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Add MSVC-specific flags for Qt compatibility
if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  add_compile_options(/Zc:__cplusplus)
endif()

# Cross-platform compiler flags for Clang (only when using Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Minimal warning set - only show critical issues
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything")

  # Enable ONLY critical error-like warnings
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type")  # Missing return statements (critical bug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=uninitialized")  # Use of uninitialized variables
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=delete-non-virtual-dtor")  # Memory safety
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-result")  # Ignoring important return values
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security")  # Format string vulnerabilities
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnull-dereference")  # Null pointer dereferences
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdangling")  # Dangling pointers/references

  # Suppress intentional GNU extensions
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")  # Logger macros use GNU extension

  # Treat warnings as informational (don't fail build)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

  if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nonportable-system-include-path")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Note: ASan with MSVC/Clang-CL may require matching runtime; disabled for compatibility
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
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

  find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Svg Network)
  if(Qt6_FOUND)
    qt6_standard_project_setup()
    message(STATUS "Qt6 found at ${Qt6_DIR}")
    message(STATUS "Qt GUI will be built")
  else()
    message(WARNING "Qt6 not found - Qt GUI will not be built. Install Qt6 to enable Qt GUI.")
    set(BUILD_GUI_QT OFF)
  endif()
endif()

# Add cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find QR code library - try vcpkg first, then custom finder
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBQRENCODE QUIET libqrencode)
endif()

if(NOT LIBQRENCODE_FOUND)
    find_package(LibQREncode QUIET)
    if(LibQREncode_FOUND)
        set(LIBQRENCODE_FOUND TRUE)
        set(QRENCODE_INCLUDE_DIR ${LIBQRENCODE_INCLUDE_DIRS})
        set(QRENCODE_LIBRARIES ${LIBQRENCODE_LIBRARIES})
    endif()
endif()

# Fallback to manual search
if(NOT LIBQRENCODE_FOUND)
    # Check vcpkg installed directory first
    find_path(QRENCODE_INCLUDE_DIR NAMES qrencode.h
        PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
        NO_DEFAULT_PATH
    )

    find_library(QRENCODE_LIBRARY_RELEASE NAMES qrencode
        PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
        NO_DEFAULT_PATH
    )

    find_library(QRENCODE_LIBRARY_DEBUG NAMES qrencoded
        PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib
        NO_DEFAULT_PATH
    )

    # If not found in vcpkg, try system paths
    if(NOT QRENCODE_INCLUDE_DIR)
        find_path(QRENCODE_INCLUDE_DIR NAMES qrencode.h)
    endif()

    if(NOT QRENCODE_LIBRARY_RELEASE)
        find_library(QRENCODE_LIBRARY_RELEASE qrencode)
    endif()

    if(NOT QRENCODE_LIBRARY_DEBUG)
        find_library(QRENCODE_LIBRARY_DEBUG qrencoded)
    endif()

    if(QRENCODE_LIBRARY_DEBUG)
        set(QRENCODE_LIBRARIES optimized ${QRENCODE_LIBRARY_RELEASE} debug ${QRENCODE_LIBRARY_DEBUG})
    else()
        set(QRENCODE_LIBRARIES ${QRENCODE_LIBRARY_RELEASE})
    endif()

    if(QRENCODE_INCLUDE_DIR AND QRENCODE_LIBRARIES)
        set(LIBQRENCODE_FOUND TRUE)
        message(STATUS "libqrencode found (manual detection)")
        message(STATUS "  Include: ${QRENCODE_INCLUDE_DIR}")
        message(STATUS "  Library: ${QRENCODE_LIBRARIES}")
    endif()
endif()

if(LIBQRENCODE_FOUND)
    message(STATUS "libqrencode found - QR code functionality enabled")
else()
    message(WARNING "libqrencode not found - QR code functionality will use fallback")
endif()

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Check for BlockCypher dependencies
# Add vcpkg installed directory to prefix path if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
  if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
  else()
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-linux")
  endif()
endif()

find_package(cpr CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)
if(cpr_FOUND AND nlohmann_json_FOUND)
  set(BLOCKCYPHER_AVAILABLE TRUE)
  message(STATUS "BlockCypher API dependencies found")
else()
  set(BLOCKCYPHER_AVAILABLE FALSE)
  message(STATUS "BlockCypher API dependencies not found - run 'vcpkg install' to install cpr and nlohmann-json")
endif()

# Add backend components
add_subdirectory(backend/utils)
add_subdirectory(backend/blockchain)
add_subdirectory(backend/database)
add_subdirectory(backend/repository)
add_subdirectory(backend/core)

# Add frontend components
add_subdirectory(frontend/qt)

# Add test suite
add_subdirectory(Tests)

# Set startup project for Visual Studio (must be after add_subdirectory)
if(BUILD_GUI_QT AND Qt6_FOUND)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualetQt)
endif()

# Test executables are now defined in Tests/CMakeLists.txt
