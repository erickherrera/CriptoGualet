# This file is included by the root CMakeLists.txt which handles:
# - Clang compiler detection and configuration
# - vcpkg integration  
# - Qt6 setup with cross-platform paths
# - C++ standard and build options

# ---- Windows CryptoAPI (instead of OpenSSL) ----

# ---- Auth library (portable, builds everywhere) ----
add_library(Auth STATIC
  "./Auth.cpp"
  "../include/Auth.h"
)
target_include_directories(Auth PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/."
  "${CMAKE_CURRENT_SOURCE_DIR}/./Auth"
)

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Clang-specific flags for security and warnings
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Auth PRIVATE -fPIE -pie)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # MSVC or Clang-CL flags
  target_compile_options(Auth PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  # GCC or other compilers
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(Auth PRIVATE -fPIE -pie)
endif()

# ---- Win32 GUI executable (Windows only) ----
if(WIN32 AND BUILD_GUI_WIN32)
  add_executable(CriptoGualet WIN32
    "CriptoGualet.cpp"
    "LoginUI.cpp"
    "WalletUI.cpp"
    "../Include/CriptoGualet.h"
    "../Include/LoginUI.h"
    "../Include/WalletUI.h"
  )

  # Define UNICODE on the GUI target
  target_compile_definitions(CriptoGualet PRIVATE UNICODE _UNICODE)

  # Cross-platform compiler flags with security
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
    target_link_options(CriptoGualet PRIVATE -fPIE -pie)
  elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(CriptoGualet PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
    set_property(TARGET CriptoGualet PROPERTY VS_GLOBAL_CharacterSet Unicode)
  else()
    target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
    target_link_options(CriptoGualet PRIVATE -fPIE -pie)
  endif()

  # Link Windows libs and CryptoAPI
  target_link_libraries(CriptoGualet PRIVATE Auth comctl32 crypt32 bcrypt)
  
  # Set as startup project only if Qt GUI is not being built
  if(NOT (BUILD_GUI_QT AND Qt6_FOUND))
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualet)
    set(STARTUP_PROJECT_SET TRUE)
    message(STATUS "Win32 GUI target will be the startup project")
  endif()

else()
  message(STATUS "Non-Windows platform detected or BUILD_GUI_WIN32=OFF: Win32 GUI target will not be built.")
endif()

# ---- Qt GUI executable ----
if(BUILD_GUI_QT AND Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  
  add_executable(CriptoGualetQt
    "CriptoGualetQt.cpp"
    "QtLoginUI.cpp"
    "QtWalletUI.cpp"
    "QtThemeManager.cpp"
    "SharedSymbols.cpp"
    "../include/QtLoginUI.h"
    "../include/QtWalletUI.h"
    "../include/QtThemeManager.h"
    "../include/CriptoGualetQt.h"
  )

  target_link_libraries(CriptoGualetQt PRIVATE 
    Auth 
    Qt6::Core 
    Qt6::Widgets
    Qt6::Gui
    comctl32 
    crypt32 
    bcrypt
  )
  
  target_include_directories(CriptoGualetQt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
  )

  # Cross-platform compiler flags with security for Qt target
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(CriptoGualetQt PRIVATE /W4 /permissive- /utf-8 /EHsc /GS /Zc:__cplusplus)
  else()
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  endif()

  # Set as startup project for Visual Studio if Qt GUI is built
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualetQt)
  set(STARTUP_PROJECT_SET TRUE)
  message(STATUS "Qt GUI target will be the startup project")

  # Deploy Qt6 DLLs automatically
  if(WIN32)
    find_program(QT_DEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(QT_DEPLOYQT_EXECUTABLE)
      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:CriptoGualetQt>
        COMMENT "Deploying Qt libraries")
    endif()
  endif()
elseif(BUILD_GUI_QT AND NOT Qt6_FOUND)
  message(STATUS "Qt6 not found - install Qt6 from https://qt.io to build the modern Qt GUI")
endif()

# Ensure at least one executable is built for startup project
if(NOT STARTUP_PROJECT_SET)
  message(WARNING "No executable targets built! Enable BUILD_GUI_WIN32 or BUILD_GUI_QT to build executables.")
  message(STATUS "Available targets: Auth (static library)")
endif()

# IDE folder organization is handled by root CMakeLists.txt
# Compile commands export is handled by root CMakeLists.txt
