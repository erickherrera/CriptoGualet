# This file is included by the root CMakeLists.txt which handles:
# - Clang compiler detection and configuration
# - vcpkg integration  
# - Qt6 setup with cross-platform paths
# - C++ standard and build options

# ---- Windows CryptoAPI (instead of OpenSSL) ----

# ---- Crypto library (cryptographic functions) ----
add_library(Crypto STATIC
  "./Crypto.cpp"
  "../include/Crypto.h"
)
target_include_directories(Crypto PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/."
)

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  # Clang on non-Windows platforms
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Crypto PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  # Clang-CL on Windows (no stack protector or PIE support)
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # MSVC flags
  target_compile_options(Crypto PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  # GCC or other compilers
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(Crypto PRIVATE -fPIE -pie)
endif()

# ---- QR Code library ----
add_library(QRGenerator STATIC
  "./QRGenerator.cpp"
  "../include/QRGenerator.h"
)
target_include_directories(QRGenerator PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/."
)

# Link libqrencode if available
if(LIBQRENCODE_FOUND)
  target_include_directories(QRGenerator PRIVATE ${QRENCODE_INCLUDE_DIR})
  target_link_libraries(QRGenerator PRIVATE ${QRENCODE_LIBRARIES})
  target_compile_definitions(QRGenerator PRIVATE QRENCODE_AVAILABLE)
  message(STATUS "QRGenerator will be built with libqrencode support")
else()
  message(STATUS "QRGenerator will be built with fallback pattern only")
endif()

# ---- Auth library (portable, builds everywhere) ----
add_library(Auth STATIC
  "./Auth.cpp"
  "../include/Auth.h"
)
target_include_directories(Auth PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/."
)
target_link_libraries(Auth PRIVATE Crypto)

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  # Clang on non-Windows platforms
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Auth PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  # Clang-CL on Windows (no stack protector or PIE support)
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # MSVC flags
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
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
    target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
    target_link_options(CriptoGualet PRIVATE -fPIE -pie)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
    target_compile_options(CriptoGualet PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
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
    "QtSeedDisplayDialog.cpp"
    "SharedSymbols.cpp"
    "../include/QtLoginUI.h"
    "../include/QtWalletUI.h"
    "../include/QtThemeManager.h"
    "../include/QtSeedDisplayDialog.h"
    "../include/CriptoGualetQt.h"
  )

  target_link_libraries(CriptoGualetQt PRIVATE
    Auth
    QRGenerator
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
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
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

    # Copy qrencode DLL if available (use debug versions for debug builds)
    if(LIBQRENCODE_FOUND)
      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/debug/bin/qrencoded.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/bin/qrencode.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying qrencode DLL to output directory")

      # Also copy dependencies that qrencode.dll needs (debug versions for debug builds)
      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/debug/bin/libpng16d.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/bin/libpng16.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying libpng16 DLL to output directory")

      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/debug/bin/zlibd1.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/bin/zlib1.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying zlib DLL to output directory")
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
