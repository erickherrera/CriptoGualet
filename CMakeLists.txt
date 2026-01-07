cmake_minimum_required(VERSION 3.20)

# Suppress common CMake warnings and set policies
# CMP0091: Make MSVC runtime selection controllable & stable across configs
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# CMP0144: Suppress warning about CMAKE_MSVC_RUNTIME_LIBRARY
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# CMP0092: Use MSVC_RUNTIME_LIBRARY for runtime library selection
if(POLICY CMP0092)
  cmake_policy(SET CMP0092 NEW)
endif()

# CMP0138: Suppress warnings about VS_STARTUP_PROJECT
if(POLICY CMP0138)
  cmake_policy(SET CMP0138 NEW)
endif()

# Windows SDK configuration - must be before project() call
# When using clang-cl with Ninja, CMake needs to find Windows SDK tools (rc.exe) during compiler test
if(WIN32)
  # Find Windows SDK installation
  set(WINDOWS_SDK_PATHS
    "C:/Program Files (x86)/Windows Kits/10"
    "C:/Program Files/Windows Kits/10"
  )
  
  # Find the latest Windows SDK version
  set(WINDOWS_SDK_VERSION "")
  set(WINDOWS_SDK_ROOT "")
  foreach(SDK_PATH ${WINDOWS_SDK_PATHS})
    if(EXISTS "${SDK_PATH}")
      file(GLOB SDK_VERSIONS "${SDK_PATH}/Include/*")
      if(SDK_VERSIONS)
        list(SORT SDK_VERSIONS)
        list(REVERSE SDK_VERSIONS)
        list(GET SDK_VERSIONS 0 LATEST_SDK)
        get_filename_component(WINDOWS_SDK_VERSION "${LATEST_SDK}" NAME)
        set(WINDOWS_SDK_ROOT "${SDK_PATH}")
        break()
      endif()
    endif()
  endforeach()
  
  if(WINDOWS_SDK_ROOT AND WINDOWS_SDK_VERSION)
    # Find rc.exe (Resource Compiler) - must be set before project() call
    find_program(RC_EXECUTABLE
      NAMES rc.exe
      PATHS
        "${WINDOWS_SDK_ROOT}/bin/${WINDOWS_SDK_VERSION}/x64"
        "${WINDOWS_SDK_ROOT}/bin/x64"
        "${WINDOWS_SDK_ROOT}/bin/${WINDOWS_SDK_VERSION}/x86"
        "${WINDOWS_SDK_ROOT}/bin/x86"
      NO_DEFAULT_PATH
    )
    
    if(RC_EXECUTABLE)
      set(CMAKE_RC_COMPILER "${RC_EXECUTABLE}" CACHE FILEPATH "Windows Resource Compiler")
    else()
      # Try to find it in PATH as fallback
      find_program(RC_EXECUTABLE NAMES rc.exe)
      if(RC_EXECUTABLE)
        set(CMAKE_RC_COMPILER "${RC_EXECUTABLE}" CACHE FILEPATH "Windows Resource Compiler")
      endif()
    endif()
  endif()
endif()

# Declare the project EARLY so VS presets/toolsets apply cleanly
project(CriptoGualet LANGUAGES CXX)

# Set MSVC runtime library based on build type
# With CMP0091 policy set, CMake handles runtime library selection properly
# For multi-config generators (Visual Studio), we set a default that works for Debug
# Individual targets can override if needed for Release builds
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio) - default to Debug runtime for safety
    # Release builds can override via target properties if needed
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library")
    set_property(CACHE CMAKE_MSVC_RUNTIME_LIBRARY PROPERTY STRINGS 
      "MultiThreaded" "MultiThreadedDLL" "MultiThreadedDebug" "MultiThreadedDebugDLL")
  else()
    # Single-config generator - set based on build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library")
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "MSVC runtime library")
    endif()
  endif()
endif()

# Helper function to set MSVC runtime library per-configuration on targets
# Usage: set_msvc_runtime_library(target_name)
function(set_msvc_runtime_library target)
  if(MSVC AND CMAKE_CONFIGURATION_TYPES)
    # For multi-config generators, set per-configuration using generator expressions
    set_property(TARGET ${target} PROPERTY
      MSVC_RUNTIME_LIBRARY
      "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
endfunction()

# Put built EXEs/DLLs in a predictable place: <build>/bin
# (launch.vs.json uses this)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# -------------------------------
# Compiler information
# -------------------------------
# Note: For Visual Studio generators, the compiler is determined by the generator/toolset,
# not by CMAKE_C_COMPILER/CMAKE_CXX_COMPILER variables.
# The CMakePresets.json specifies the appropriate generator and toolset.
message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")
message(STATUS "C++ Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")

# Windows SDK information logging (detection already done before project() call)
if(WIN32 AND CMAKE_RC_COMPILER)
  message(STATUS "Windows Resource Compiler: ${CMAKE_RC_COMPILER}")
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
