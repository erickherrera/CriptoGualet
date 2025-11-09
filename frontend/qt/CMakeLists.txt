# Qt GUI Frontend

if(BUILD_GUI_QT AND Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  add_executable(CriptoGualetQt WIN32
    "CriptoGualetQt.cpp"
    "QtLoginUI.cpp"
    "QtWalletUI.cpp"
    "QtSettingsUI.cpp"
    "QtSidebar.cpp"
    "QtThemeManager.cpp"
    "QtSeedDisplayDialog.cpp"
    "QtExpandableWalletCard.cpp"
    "QtTopCryptosPage.cpp"
    "QtSendDialog.cpp"
    "include/QtLoginUI.h"
    "include/QtWalletUI.h"
    "include/QtSettingsUI.h"
    "include/QtSidebar.h"
    "include/QtThemeManager.h"
    "include/QtSeedDisplayDialog.h"
    "include/QtExpandableWalletCard.h"
    "include/QtTopCryptosPage.h"
    "include/QtSendDialog.h"
    "include/CriptoGualetQt.h"
    "../../resources/resources.qrc"
  )

  set(QT_LINK_LIBRARIES
    Auth
    Crypto
    Repository
    Database
    QRGenerator
    SharedSymbols
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
    comctl32
    crypt32
    bcrypt
  )

  # Add BlockCypher libraries if available
  if(BLOCKCYPHER_AVAILABLE)
    list(APPEND QT_LINK_LIBRARIES BlockCypher WalletAPI PriceService)
    target_compile_definitions(CriptoGualetQt PRIVATE BLOCKCYPHER_AVAILABLE)
  endif()

  target_link_libraries(CriptoGualetQt PRIVATE ${QT_LINK_LIBRARIES})

  target_include_directories(CriptoGualetQt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/backend/core/include"
    "${CMAKE_SOURCE_DIR}/backend/blockchain/include"
    "${CMAKE_SOURCE_DIR}/backend/database/include"
    "${CMAKE_SOURCE_DIR}/backend/repository/include"
    "${CMAKE_SOURCE_DIR}/backend/utils/include"
  )

  # Explicitly add Qt include directories for Clang compatibility
  target_include_directories(CriptoGualetQt SYSTEM PRIVATE
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Svg_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
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
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/debug/bin/qrencoded.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/qrencode.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying qrencode DLL to output directory")

      # Also copy dependencies that qrencode.dll needs (debug versions for debug builds)
      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/debug/bin/libpng16d.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/libpng16.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying libpng16 DLL to output directory")

      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/debug/bin/zlibd1.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/zlib1.dll>"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying zlib DLL to output directory")
    endif()

    # Copy SQLCipher DLL for database functionality (use debug version for debug builds)
    add_custom_command(TARGET CriptoGualetQt POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll"
      "$<TARGET_FILE_DIR:CriptoGualetQt>"
      COMMENT "Copying SQLCipher DLL to output directory")

    # Copy SQLite3 DLL (required by SQLCipher)
    add_custom_command(TARGET CriptoGualetQt POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/sqlite3.dll"
      "$<TARGET_FILE_DIR:CriptoGualetQt>"
      COMMENT "Copying SQLite3 DLL to output directory")

    # Copy OpenSSL Crypto DLL (required for cryptographic operations)
    add_custom_command(TARGET CriptoGualetQt POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/libcrypto-3-x64.dll"
      "$<TARGET_FILE_DIR:CriptoGualetQt>"
      COMMENT "Copying OpenSSL Crypto DLL to output directory")

    # Copy OpenSSL SSL DLL (required for network operations)
    add_custom_command(TARGET CriptoGualetQt POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/libssl-3-x64.dll"
      "$<TARGET_FILE_DIR:CriptoGualetQt>"
      COMMENT "Copying OpenSSL SSL DLL to output directory")
  endif()
elseif(BUILD_GUI_QT AND NOT Qt6_FOUND)
  message(STATUS "Qt6 not found - install Qt6 from https://qt.io to build the modern Qt GUI")
endif()
