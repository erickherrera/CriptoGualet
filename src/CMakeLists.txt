cmake_minimum_required(VERSION 3.16)

project(CriptoGualet LANGUAGES CXX)

# Enable vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

# ---- Options ----
option(BUILD_GUI_WIN32 "Build the Win32 GUI wallet app (Windows only)" OFF)
option(BUILD_GUI_QT "Build the Qt GUI wallet app" ON)

# ---- Common C++ standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Qt6 Setup ----
if(BUILD_GUI_QT)
  find_package(Qt6 QUIET COMPONENTS Core Widgets)
  if(Qt6_FOUND)
    qt6_standard_project_setup()
    message(STATUS "Qt6 found - Qt GUI will be built")
  else()
    message(WARNING "Qt6 not found - Qt GUI will not be built. Install Qt6 to enable Qt GUI.")
    set(BUILD_GUI_QT OFF)
  endif()
endif()

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

# Reasonable warnings per-compiler with security flags
if(MSVC)
  target_compile_options(Auth PRIVATE /W4 /permissive- /utf-8 /EHsc /GS /DYNAMICBASE /NXCOMPAT)
  target_link_options(Auth PRIVATE /DYNAMICBASE /NXCOMPAT)
else()
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

  # MSVC-specific flags for the GUI with security
  if(MSVC)
    target_compile_options(CriptoGualet PRIVATE /W4 /permissive- /utf-8 /EHsc /GS /DYNAMICBASE /NXCOMPAT)
    target_link_options(CriptoGualet PRIVATE /DYNAMICBASE /NXCOMPAT)
    set_property(TARGET CriptoGualet PROPERTY VS_GLOBAL_CharacterSet Unicode)
  else()
    target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
    target_link_options(CriptoGualet PRIVATE -fPIE -pie)
  endif()

  # Link Windows libs and CryptoAPI
  target_link_libraries(CriptoGualet PRIVATE Auth comctl32 crypt32 bcrypt)
  
  # Set as startup project for Visual Studio
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualet)

else()
  message(STATUS "Non-Windows platform detected or BUILD_GUI_WIN32=OFF: Win32 GUI target will not be built.")
endif()

# ---- Qt GUI executable ----
if(BUILD_GUI_QT AND Qt6_FOUND)
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
    "../.gitignore"
  )

  target_link_libraries(CriptoGualetQt PRIVATE 
    Auth 
    Qt6::Core 
    Qt6::Widgets
    comctl32 
    crypt32 
    bcrypt
  )

  # MSVC-specific flags for Qt with security
  if(MSVC)
    target_compile_options(CriptoGualetQt PRIVATE /W4 /permissive- /utf-8 /EHsc /GS /Zc:__cplusplus)
    target_link_options(CriptoGualetQt PRIVATE /DYNAMICBASE /NXCOMPAT)
  else()
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  endif()

  # Set as startup project for Visual Studio if Qt GUI is built
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CriptoGualetQt)
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

# (Optional) organize IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Export compile commands (handy for editors; harmless everywhere)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
