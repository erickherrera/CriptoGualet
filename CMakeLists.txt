cmake_minimum_required(VERSION 3.10)

project(CriptoGualet LANGUAGES CXX)

# ---- Executable ----
add_executable(CriptoGualet WIN32
    "CriptoGualet.cpp"
    "CriptoGualet.h"
)

# C++ standard
set_property(TARGET CriptoGualet PROPERTY CXX_STANDARD 20)
set_property(TARGET CriptoGualet PROPERTY CXX_STANDARD_REQUIRED ON)

# UTF-8 + Unicode on MSVC
if(MSVC)
  target_compile_options(CriptoGualet PRIVATE /utf-8)
  set_property(TARGET CriptoGualet PROPERTY VS_GLOBAL_CharacterSet Unicode)
endif()

# Prefer target-level defines (keeps scope tight)
target_compile_definitions(CriptoGualet PRIVATE UNICODE _UNICODE)

# Warnings by compiler (commonly accepted baseline)
if(MSVC)
  target_compile_options(CriptoGualet PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Windows libs
if(WIN32)
  target_link_libraries(CriptoGualet PRIVATE comctl32)
endif()

# (Optional) organize IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Clang tools integration
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Enable clang-tidy
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    set_target_properties(CriptoGualet PROPERTIES
      CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
    )
  endif()
endif()

# Export compile commands for clang tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)