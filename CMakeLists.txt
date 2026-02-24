# Find MSVC compiler if not configured
if(WIN32 AND NOT CMAKE_CXX_COMPILER)
    find_program(VSWHERE_EXE "vswhere.exe"
        PATHS "C:/Program Files (x86)/Microsoft Visual Studio/Installer"
    )

    if(VSWHERE_EXE)
        # Find latest VS installation
        execute_process(
            COMMAND ${VSWHERE_EXE} -latest -property installationPath
            OUTPUT_VARIABLE VS_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(VS_PATH AND EXISTS "${VS_PATH}/VC/Tools/MSVC")
            # Find the latest MSVC toolset version
            file(GLOB MSVC_VERSIONS "${VS_PATH}/VC/Tools/MSVC/*")
            list(SORT MSVC_VERSIONS)
            list(REVERSE MSVC_VERSIONS)
            list(GET MSVC_VERSIONS 0 LATEST_MSVC_PATH)
            
            get_filename_component(LATEST_MSVC_VERSION "${LATEST_MSVC_PATH}" NAME)

            set(MSVC_COMPILER_PATH "${LATEST_MSVC_PATH}/bin/Hostx64/x64/cl.exe")
            if(EXISTS "${MSVC_COMPILER_PATH}")
                set(CMAKE_CXX_COMPILER "${MSVC_COMPILER_PATH}" CACHE STRING "MSVC C++ Compiler")
                set(CMAKE_C_COMPILER "${MSVC_COMPILER_PATH}" CACHE STRING "MSVC C Compiler")
                message(STATUS "Found MSVC compiler: ${MSVC_COMPILER_PATH}")
            endif()
        endif()
    endif()
endif()

# If clang-cl is requested but path is missing, locate it via VS installation
if(WIN32 AND (CMAKE_CXX_COMPILER MATCHES "clang-cl" OR CMAKE_C_COMPILER MATCHES "clang-cl"))
    if((CMAKE_CXX_COMPILER AND NOT EXISTS "${CMAKE_CXX_COMPILER}") OR (CMAKE_C_COMPILER AND NOT EXISTS "${CMAKE_C_COMPILER}"))
        find_program(VSWHERE_EXE "vswhere.exe"
            PATHS "C:/Program Files (x86)/Microsoft Visual Studio/Installer"
        )
        if(VSWHERE_EXE)
            execute_process(
                COMMAND ${VSWHERE_EXE} -latest -property installationPath
                OUTPUT_VARIABLE VS_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            set(CLANGCL_PATH "${VS_PATH}/VC/Tools/LLVM/x64/bin/clang-cl.exe")
            if(VS_PATH AND EXISTS "${CLANGCL_PATH}")
                set(CMAKE_CXX_COMPILER "${CLANGCL_PATH}" CACHE STRING "ClangCL C++ Compiler" FORCE)
                set(CMAKE_C_COMPILER "${CLANGCL_PATH}" CACHE STRING "ClangCL C Compiler" FORCE)
                message(STATUS "Found clang-cl compiler: ${CLANGCL_PATH}")
            endif()
        endif()
    endif()
endif()

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
# CMake needs to find Windows SDK tools (rc.exe) during compiler test
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
# For multi-config generators (Visual Studio), we use a generator expression 
# to ensure the correct runtime is used for both Debug and Release configurations.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio) - use generator expression for consistency
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library")
    set_property(CACHE CMAKE_MSVC_RUNTIME_LIBRARY PROPERTY STRINGS 
      "MultiThreaded" "MultiThreadedDLL" "MultiThreadedDebug" "MultiThreadedDebugDLL"
      "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    # Single-config generator - set based on build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "MSVC runtime library")
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "MSVC runtime library")
    endif()
  endif()
endif()

# Ensure MSVC runtime consistency for ClangCL + Qt compatibility
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator - set consistent runtime using generator expressions
    # This is CRITICAL for preventing _ITERATOR_DEBUG_LEVEL mismatches in Release builds
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library for ClangCL" FORCE)
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
  # Force disable ASAN for this project to prevent flag conflicts
  add_compile_definitions(CRYPTO_NO_ASAN)
endif()

# Cross-platform compiler flags for Clang (only when using Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Determine if we're using ClangCL or pure Clang
  if(CMAKE_CXX_COMPILER MATCHES "clang-cl")
    set(CLANG_MODE "CLANGCL")
    message(STATUS "ClangCL mode detected - using MSVC compatibility")
  else()
    set(CLANG_MODE "PURE_CLANG")
    message(STATUS "Pure Clang mode detected")
  endif()
  
  # Enhanced warning set for crypto wallet security (disable C++98 warnings for ClangCL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=uninitialized")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=delete-non-virtual-dtor")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-result")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnull-dereference")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdangling")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")  # Security important
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
  # Disable C++98 compatibility warnings that cause linking issues
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic")

  # ClangCL-specific optimizations
  if(CLANG_MODE STREQUAL "CLANGCL")
    # Core MSVC ABI compatibility for Qt integration
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-compatibility-version=19.39")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_ENABLE_EXTENDED_ALIGNED_STORAGE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nonportable-system-include-path")
    # Suppress intentional GNU extensions for logger macros
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
  endif()

  # Enhanced debug configuration with sanitizers (disabled by default for compatibility)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Configurable sanitizers for security testing
    option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)
    option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for UB detection" OFF)
    
    if(ENABLE_ASAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
      # ASan-specific optimizations
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      message(STATUS "AddressSanitizer enabled")
    endif()
    
    if(ENABLE_UBSAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
      message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
    
    if(ENABLE_UBSAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
      message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # LTO for better performance
    option(ENABLE_LTO "Enable Link-Time Optimization for better performance" OFF)
    if(ENABLE_LTO)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -flto")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
      message(STATUS "Link-Time Optimization enabled")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    endif()
  endif()
endif()

# Build options
option(ENABLE_UNITY_BUILD "Enable CMake unity build for faster compiles" OFF)
option(ENABLE_MSVC_MP "Enable MSVC /MP parallel compilation" OFF)

if(ENABLE_UNITY_BUILD)
  set(CMAKE_UNITY_BUILD ON)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)
endif()

if(MSVC AND ENABLE_MSVC_MP)
  add_compile_options(/MP)
endif()

option(BUILD_GUI_QT "Build the Qt GUI wallet app" ON)
set(CRYPTOGUWALET_VS_STARTUP_TARGET "" CACHE STRING "Default Visual Studio startup target (leave empty to select in IDE)")

# Find Qt6 if GUI is enabled
if(BUILD_GUI_QT)
  if(WIN32)
    # Add common Qt roots; exact ABI will be resolved by find_package
    list(APPEND CMAKE_PREFIX_PATH
      "C:/Program Files/Qt"
      "C:/Qt"
      "C:/Program Files (x86)/Qt"
    )
    if(DEFINED QT_DIR AND EXISTS "${QT_DIR}")
      list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}")
      message(STATUS "Using QT_DIR: ${QT_DIR}")
    endif()
    # Enhanced Qt path resolution for different compilers
    file(GLOB QT_VERSIONS "C:/Program Files/Qt/6.*")
    foreach(QT_VERSION ${QT_VERSIONS})
      if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        list(APPEND CMAKE_PREFIX_PATH
          "${QT_VERSION}/msvc2022_64"
          "${QT_VERSION}/msvc2019_64")
      elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(CLANG_MODE STREQUAL "CLANGCL")
          # ClangCL should use MSVC-built Qt for ABI compatibility
          list(APPEND CMAKE_PREFIX_PATH
            "${QT_VERSION}/msvc2022_64"
            "${QT_VERSION}/msvc2019_64"
            "${QT_VERSION}/clang_64")
        else()
          # Pure Clang can use native Qt builds
          list(APPEND CMAKE_PREFIX_PATH
            "${QT_VERSION}/clang_64"
            "${QT_VERSION}/llvm-mingw_64"
            "${QT_VERSION}/msvc2022_64")  # Fallback
        endif()
      else()
        list(APPEND CMAKE_PREFIX_PATH
          "${QT_VERSION}/mingw_64"
          "${QT_VERSION}/llvm-mingw_64"
          "${QT_VERSION}/clang_64")
      endif()
    endforeach()
  elseif(UNIX AND NOT APPLE)
    # Linux Qt6 detection
    list(APPEND CMAKE_PREFIX_PATH
      "/usr/local/qt6"
      "/usr/local/Qt6"
      "/opt/qt6"
      "/opt/Qt6"
      "$ENV{HOME}/Qt6"
      "$ENV{HOME}/Qt/6.x.x/gcc_64"
      "$ENV{HOME}/Qt/6.9.1/gcc_64"
      "$ENV{HOME}/Qt/6.8.0/gcc_64"
      "/usr/lib/x86_64-linux-gnu/qt6"
      "/usr/lib/x86_64-linux-gnu/cmake/Qt6"
    )
    # Also check vcpkg-installed Qt6
    if(DEFINED VCPKG_ROOT)
      list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-linux/tools/Qt6")
    endif()
    if(DEFINED QT_DIR AND EXISTS "${QT_DIR}")
      list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}")
      message(STATUS "Using QT_DIR: ${QT_DIR}")
    endif()
  endif()

  find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Svg Network Concurrent)
  if(Qt6_FOUND)
    qt6_standard_project_setup()
    message(STATUS "Qt6 found at ${Qt6_DIR}")
    message(STATUS "Qt GUI will be built with ${CMAKE_CXX_COMPILER_ID}")
    
    # Verify Qt compatibility
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      if(CLANG_MODE STREQUAL "CLANGCL")
        message(STATUS "ClangCL+Qt6: Using MSVC-built Qt for ABI compatibility")
      else()
        message(STATUS "Pure Clang+Qt6: Using native Qt build if available")
      endif()
    endif()
  else()
    message(WARNING "Qt6 not found - Qt GUI will not be built")
    if(WIN32)
      message(STATUS "Install Qt6 MSVC 2022 64-bit for best ClangCL compatibility")
    elseif(UNIX AND NOT APPLE)
      message(STATUS "Install Qt6 for Linux: apt install qt6-base-dev (or use vcpkg)")
    endif()
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
        set(LIBQRENCODE_FOUND TRUE CACHE BOOL "libqrencode found" FORCE)
        set(QRENCODE_INCLUDE_DIR ${LIBQRENCODE_INCLUDE_DIRS} CACHE PATH "libqrencode include dir" FORCE)
        set(QRENCODE_LIBRARIES ${LIBQRENCODE_LIBRARIES} CACHE STRING "libqrencode libraries" FORCE)
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
        set(LIBQRENCODE_FOUND TRUE CACHE BOOL "libqrencode found" FORCE)
        set(QRENCODE_INCLUDE_DIR ${QRENCODE_INCLUDE_DIR} CACHE PATH "libqrencode include dir" FORCE)
        set(QRENCODE_LIBRARIES ${QRENCODE_LIBRARIES} CACHE STRING "libqrencode libraries" FORCE)
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
  set(BLOCKCYPHER_AVAILABLE TRUE CACHE BOOL "BlockCypher API dependencies found" FORCE)
  message(STATUS "BlockCypher API dependencies found")
else()
  set(BLOCKCYPHER_AVAILABLE FALSE CACHE BOOL "BlockCypher API dependencies found" FORCE)
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
enable_testing()
add_subdirectory(Tests)

# Set Visual Studio startup target if requested (must be after add_subdirectory)
if(CRYPTOGUWALET_VS_STARTUP_TARGET)
  if(TARGET ${CRYPTOGUWALET_VS_STARTUP_TARGET})
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CRYPTOGUWALET_VS_STARTUP_TARGET})
    message(STATUS "Visual Studio startup target: ${CRYPTOGUWALET_VS_STARTUP_TARGET}")
  else()
    message(WARNING "Requested VS startup target '${CRYPTOGUWALET_VS_STARTUP_TARGET}' does not exist")
  endif()
endif()

# Test executables are now defined in Tests/CMakeLists.txt

# -----------------------------------------------------------------------------
# RPATH Configuration for Relocatable Binaries
# -----------------------------------------------------------------------------
# These settings ensure binaries can find their libraries at runtime after installation,
# regardless of where CPack stages them. This is critical for the packaging phase where
# CMake relinks binaries to use INSTALL_RPATH instead of BUILD_RPATH.

# Use RPATH for the build tree (allows running from build dir during development)
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Don't use the install RPATH during build (use build tree paths)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Set the RPATH for installed binaries to find libraries relative to executable
# $ORIGIN (Linux) or @executable_path (macOS) resolves to the directory containing the executable
if(UNIX AND NOT APPLE)
  set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib")
elseif(APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../Frameworks")
  set(CMAKE_MACOSX_RPATH ON)
endif()

# Add the automatically determined parts of the RPATH (libraries in build tree)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# -----------------------------------------------------------------------------
# CPack Packaging Configuration
# -----------------------------------------------------------------------------

# Common CPack settings (cross-platform)
set(CPACK_PACKAGE_NAME "CriptoGualet")
set(CPACK_PACKAGE_VENDOR "CriptoGualet Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Secure Non-Custodial Cryptocurrency Wallet")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CriptoGualet")
# License file for the installer
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
endif()

# Component-based installation
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "CriptoGualet Application")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "The CriptoGualet cryptocurrency wallet application and all required dependencies")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)

if(WIN32)
  set(CPACK_GENERATOR "NSIS;ZIP")
  
  # Package icon (must be .ico format, 150x57 pixels for NSIS header)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo_criptogualet.ico")
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons\\\\logo_criptogualet.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo_criptogualet.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo_criptogualet.ico")
  endif()
  
  # NSIS Installer Configuration
  set(CPACK_NSIS_DISPLAY_NAME "CriptoGualet Wallet")
  set(CPACK_NSIS_PACKAGE_NAME "CriptoGualet")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\CriptoGualetQt.exe")
  set(CPACK_NSIS_HELP_LINK "https://github.com/erickherrera/CriptoGualet")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/erickherrera/CriptoGualet")
  set(CPACK_NSIS_CONTACT "support@criptogualet.com")
  set(CPACK_NSIS_MODIFY_PATH OFF)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  
  # Request admin privileges for installation (required for Program Files)
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
  
  # Create Start Menu shortcuts
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\CriptoGualet.lnk' '$INSTDIR\\\\bin\\\\CriptoGualetQt.exe' '' '$INSTDIR\\\\bin\\\\CriptoGualetQt.exe' 0"
  )
  
  # Create Desktop shortcut
  set(CPACK_NSIS_CREATE_ICONS_EXTRA "${CPACK_NSIS_CREATE_ICONS_EXTRA}\n  CreateShortCut '$DESKTOP\\\\CriptoGualet.lnk' '$INSTDIR\\\\bin\\\\CriptoGualetQt.exe' '' '$INSTDIR\\\\bin\\\\CriptoGualetQt.exe' 0")
  
  # Delete shortcuts on uninstall
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\CriptoGualet.lnk'
     Delete '$DESKTOP\\\\CriptoGualet.lnk'"
  )

  # Install the icon file explicitly so it can be referenced if needed
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo_criptogualet.ico"
          DESTINATION bin/resources/icons
          COMPONENT Runtime)
  
  # Add/Remove Programs registry entries
  set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    WriteRegStr HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CriptoGualet' 'DisplayIcon' '$INSTDIR\\\\bin\\\\CriptoGualetQt.exe'
    WriteRegStr HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CriptoGualet' 'Publisher' 'CriptoGualet Team'
    WriteRegStr HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CriptoGualet' 'URLInfoAbout' 'https://github.com/erickherrera/CriptoGualet'
    WriteRegStr HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CriptoGualet' 'DisplayVersion' '${CPACK_PACKAGE_VERSION}'
  ")
  
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    DeleteRegKey HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CriptoGualet'
  ")
  
  # -----------------------------------------------------------------------------
  # Visual C++ Redistributable Handling
  # -----------------------------------------------------------------------------
  # The VC++ Runtime is required for any MSVC-compiled application.
  # We have two options:
  # 1. Bundle the VC++ Redistributable installer and run it during installation
  # 2. Use the merge modules to include the runtime directly
  # 
  # Option 1 is more reliable as it handles updates and shared components properly.
  # The installer will check if the runtime is already installed.
  
  # Find the VC++ Redistributable installer
  # Visual Studio 2022 (v143 toolset) uses vc_redist.x64.exe
  set(VCREDIST_PATHS
    "$ENV{VCToolsRedistDir}/vc_redist.x64.exe"
    "$ENV{VCINSTALLDIR}/Redist/MSVC/v143/vc_redist.x64.exe"
    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.38.33135/vc_redist.x64.exe"
    "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Redist/MSVC/14.38.33135/vc_redist.x64.exe"
    "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Redist/MSVC/14.38.33135/vc_redist.x64.exe"
  )
  
  # Search for VC++ Redistributable in common paths
  foreach(VCREDIST_PATH ${VCREDIST_PATHS})
    if(EXISTS "${VCREDIST_PATH}")
      set(VCREDIST_EXECUTABLE "${VCREDIST_PATH}")
      message(STATUS "Found VC++ Redistributable: ${VCREDIST_EXECUTABLE}")
      break()
    endif()
  endforeach()
  
  # If found, configure NSIS to install VC++ Runtime
  if(VCREDIST_EXECUTABLE)
    # Install the VC++ Redistributable to a temp location in the package
    install(FILES "${VCREDIST_EXECUTABLE}"
            DESTINATION "redist"
            COMPONENT Runtime)
    
    # Run the VC++ Redistributable installer silently during installation
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}
      ; Install Visual C++ Redistributable
      DetailPrint 'Installing Visual C++ Redistributable...'
      nsExec::ExecToLog '$INSTDIR\\\\redist\\\\vc_redist.x64.exe /install /quiet /norestart'
      Pop $0
      DetailPrint 'VC++ Redistributable installation returned: $0'
    ")
  else()
    message(WARNING "VC++ Redistributable not found. Users may need to install it manually.")
    message(STATUS "Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe")
  endif()

elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop;ZIP")
  set(CPACK_DMG_VOLUME_NAME "CriptoGualet")
  set(CPACK_BUNDLE_NAME "CriptoGualet")
  
elseif(UNIX)
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
  
  # Debian package settings
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CriptoGualet Team")
  set(CPACK_DEBIAN_PACKAGE_SECTION "finance")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, libqt6svg6, libqt6network6, libssl3")
  
  # RPM package settings
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/Finance")
  set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtsvg, openssl")
endif()

include(CPack)
