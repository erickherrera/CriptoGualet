cmake_minimum_required(VERSION 3.16)

project(CriptoGualet LANGUAGES CXX)

# Enable vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_GUI_QT "Build the Qt GUI wallet app" ON)

# Find Qt6 if GUI is enabled
if(BUILD_GUI_QT)
  find_package(Qt6 QUIET COMPONENTS Core Widgets Gui)
  if(Qt6_FOUND)
    qt6_standard_project_setup()
    message(STATUS "Qt6 found - Qt GUI will be built")
  else()
    message(WARNING "Qt6 not found - Qt GUI will not be built. Install Qt6 to enable Qt GUI.")
    set(BUILD_GUI_QT OFF)
  endif()
endif()

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Root CMakeLists.txt - delegates to src/
add_subdirectory(src)