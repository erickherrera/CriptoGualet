cmake_minimum_required(VERSION 3.10)

project(CriptoGualet LANGUAGES CXX)

# ---- Options ----
option(BUILD_GUI_WIN32 "Build the Win32 GUI wallet app (Windows only)" ON)

# ---- Common C++ standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Auth library (portable, builds everywhere) ----
add_library(Auth STATIC
  "./Auth/Auth.cpp"
  "./Auth/Auth.h"
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
    "CriptoGualet.h"
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

  # Link Windows libs
  target_link_libraries(CriptoGualet PRIVATE Auth comctl32)

else()
  message(STATUS "Non-Windows platform detected or BUILD_GUI_WIN32=OFF: Win32 GUI target will not be built.")
endif()

# (Optional) organize IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Export compile commands (handy for editors; harmless everywhere)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
