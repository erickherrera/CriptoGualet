# Qt GUI Frontend

if(BUILD_GUI_QT AND Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  add_executable(CriptoGualetQt WIN32
    "../../src/CriptoGualetQt.cpp"
    "QtLoginUI.cpp"
    "QtWalletUI.cpp"
    "QtSettingsUI.cpp"
    "QtSidebar.cpp"
    "QtThemeManager.cpp"
    "QtSeedDisplayDialog.cpp"
    "QtSuccessDialog.cpp"
    "QtExpandableWalletCard.cpp"
    "QtTopCryptosPage.cpp"
    "QtSendDialog.cpp"
    "QtReceiveDialog.cpp"
    "QtPasswordConfirmDialog.cpp"
    "QtTokenImportDialog.cpp"
    "QtTokenCard.cpp"
    "QtTokenListWidget.cpp"
    "QtTestConsole.cpp"
    "LoadingOverlay.cpp"
    "include/QtLoginUI.h"
    "include/QtWalletUI.h"
    "include/QtSettingsUI.h"
    "include/QtSidebar.h"
    "include/QtThemeManager.h"
    "include/QtSeedDisplayDialog.h"
    "include/QtSuccessDialog.h"
    "include/QtExpandableWalletCard.h"
    "include/QtTopCryptosPage.h"
    "include/QtSendDialog.h"
    "include/QtReceiveDialog.h"
    "include/QtPasswordConfirmDialog.h"
    "include/QtTokenImportDialog.h"
    "include/QtTokenCard.h"
    "include/QtTokenListWidget.h"
    "include/QtTestConsole.h"
    "include/LoadingOverlay.h"
    "include/CriptoGualetQt.h"
    "../../resources/resources.qrc"
    "../../resources/app.rc"
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
    Qt6::Concurrent
  )

  # Add Windows-specific libraries
  if(WIN32)
    list(APPEND QT_LINK_LIBRARIES comctl32 crypt32 bcrypt)
  endif()

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
  target_include_directories(CriptoGualetQt SYSTEM PUBLIC
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Svg_INCLUDE_DIRS}
    ${Qt6Network_INCLUDE_DIRS}
    ${Qt6Concurrent_INCLUDE_DIRS}
  )

  # Cross-platform compiler flags with security for Qt target
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter -Wno-ctad-maybe-unsupported)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-ctad-maybe-unsupported)
    # Critical ClangCL + Qt ABI compatibility
    target_compile_definitions(CriptoGualetQt PRIVATE 
      QT_QML_DEBUG_NO_WARNING
      QT_NO_EXCEPTIONS
      QT_DISABLE_DEPRECATED_BEFORE=0x060000)
    # Ensure consistent runtime library
    if(CMAKE_CONFIGURATION_TYPES)
      set_property(TARGET CriptoGualetQt PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
  elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(CriptoGualetQt PRIVATE /W4 /permissive- /utf-8 /EHsc /GS /Zc:__cplusplus)
  else()
    target_compile_options(CriptoGualetQt PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-ctad-maybe-unsupported)
    target_link_options(CriptoGualetQt PRIVATE -fPIE -pie)
  endif()

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
      "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/debug/bin/sqlcipher.dll>$<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/bin/sqlcipher.dll>"
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
  elseif(UNIX AND NOT APPLE)
    # Linux: Copy vcpkg libraries to build directory for development
    # RPATH will be set to find these at runtime
    if(LIBQRENCODE_FOUND)
      add_custom_command(TARGET CriptoGualetQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-linux/lib/libqrencode.so"
        "$<TARGET_FILE_DIR:CriptoGualetQt>"
        COMMENT "Copying libqrencode to output directory")
    endif()

  endif()

  # -----------------------------------------------------------------------------
  # Installation & Packaging
  # -----------------------------------------------------------------------------
  # This section configures proper dependency bundling for CPack installers.
  # The key challenge is that CPack stages binaries into an isolated directory,
  # stripping build-time metadata including RPATH entries. We must ensure all
  # transitive dependencies are correctly bundled and discoverable at runtime.

  # 1. Install the main executable
  install(TARGETS CriptoGualetQt 
          RUNTIME DESTINATION bin 
          COMPONENT Runtime)

  # 2. Install Assets (BIP39 wordlists, etc.)
  # CRITICAL: The assets folder contains the BIP39 wordlist required for seed generation.
  # Without this, user registration will fail with "Cannot load BIP-39 wordlist" error.
  # 
  # Directory structure after installation:
  #   C:\Program Files\CriptoGualet\
  #     bin\
  #       CriptoGualetQt.exe
  #       assets\
  #         bip39\
  #           english.txt    <-- REQUIRED for seed phrase generation
  
  # Use CMAKE_SOURCE_DIR for reliable path resolution
  set(ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets")
  
  if(EXISTS "${ASSETS_SOURCE_DIR}")
    message(STATUS "Installing assets from: ${ASSETS_SOURCE_DIR}")
    install(DIRECTORY "${ASSETS_SOURCE_DIR}/"
            DESTINATION bin/assets
            COMPONENT Runtime
            FILES_MATCHING 
            PATTERN "*.txt"
            PATTERN "*.json")
    
    # Also verify the critical BIP39 wordlist exists
    if(EXISTS "${ASSETS_SOURCE_DIR}/bip39/english.txt")
      message(STATUS "  BIP39 wordlist found: ${ASSETS_SOURCE_DIR}/bip39/english.txt")
    else()
      message(WARNING "  BIP39 wordlist NOT FOUND - seed generation will fail!")
    endif()
  else()
    message(WARNING "Assets directory not found at ${ASSETS_SOURCE_DIR}")
    message(WARNING "Seed phrase generation will fail without the BIP39 wordlist!")
    
    # Try alternative locations
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets")
      message(STATUS "Found assets at alternative location, using that instead")
      install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/"
              DESTINATION bin/assets
              COMPONENT Runtime)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
      message(STATUS "Found assets in frontend/qt/assets, using that")
      install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
              DESTINATION bin/assets
              COMPONENT Runtime)
    endif()
  endif()

  # 3. Handle Dependencies (DLLs) using platform-specific mechanisms
  if(WIN32)
    # -----------------------------------------------------------------------------
    # Windows DLL Bundling Strategy
    # -----------------------------------------------------------------------------
    # On Windows, the "DLL Hell" problem manifests when transitive dependencies
    # are missing. CPack only includes files explicitly mentioned in install().
    # 
    # Our strategy:
    # 1. Run windeployqt to copy Qt plugins and Qt DLLs
    # 2. Use fixup_bundle to recursively scan and copy ALL transitive dependencies
    # 3. Explicitly copy known vcpkg dependencies as a safety net
    # -----------------------------------------------------------------------------
    
    # Define search directories for DLL resolution
    set(VCPKG_REL_BIN_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin")
    set(VCPKG_DBG_BIN_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin")
    set(QT_BIN_DIR "${Qt6_DIR}/../../../bin")
    
    # Store paths for use in install(CODE) blocks - these are evaluated at install time
    set(STORED_VCPKG_REL_BIN "${VCPKG_REL_BIN_DIR}")
    set(STORED_VCPKG_DBG_BIN "${VCPKG_DBG_BIN_DIR}")
    set(STORED_QT_BIN "${QT_BIN_DIR}")
    
    # Step 1: Run windeployqt for Qt-specific deployment
    # windeployqt handles Qt plugins (platforms, imageformats, styles, etc.) and Qt DLLs
    find_program(QT_DEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(QT_DEPLOYQT_EXECUTABLE)
      install(CODE "
        set(DEPLOYQT_EXE \"${QT_DEPLOYQT_EXECUTABLE}\")
        set(INSTALL_BIN_DIR \"\${CMAKE_INSTALL_PREFIX}/bin\")
        set(TARGET_EXE \"\${INSTALL_BIN_DIR}/CriptoGualetQt.exe\")
        
        if(EXISTS \"\${TARGET_EXE}\")
          message(STATUS \"Running windeployqt on \${TARGET_EXE}...\")
          execute_process(
            COMMAND \"\${DEPLOYQT_EXE}\"
                    --no-translations
                    --no-compiler-runtime
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --dir \"\${INSTALL_BIN_DIR}\"
                    \"\${TARGET_EXE}\"
            RESULT_VARIABLE DEPLOYQT_RESULT
            OUTPUT_VARIABLE DEPLOYQT_OUTPUT
            ERROR_VARIABLE DEPLOYQT_ERROR
          )
          if(NOT DEPLOYQT_RESULT EQUAL 0)
            message(WARNING \"windeployqt failed with code \${DEPLOYQT_RESULT}: \${DEPLOYQT_ERROR}\")
          else()
            message(STATUS \"windeployqt completed successfully\")
          endif()
        else()
          message(WARNING \"Target executable not found: \${TARGET_EXE}\")
        endif()
      " COMPONENT Runtime)
    else()
      message(WARNING "windeployqt not found - Qt plugins may not be deployed correctly")
    endif()

    # Step 2: Explicitly copy ALL known vcpkg dependencies
    # This is the PRIMARY mechanism for ensuring DLLs are present.
    # fixup_bundle can be unreliable on Windows, so we explicitly copy all known DLLs.
    # 
    # CRITICAL: The QR generation failure is typically caused by missing:
    # - qrencode.dll (the QR library itself)
    # - libpng16.dll (qrencode dependency)
    # - zlib1.dll (libpng dependency)
    
    # Convert paths to native format for Windows compatibility
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin" VCPKG_BIN_NATIVE)
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin" VCPKG_DBG_BIN_NATIVE)
    
    install(CODE "
      # Use the paths passed from configure time
      set(VCPKG_BIN \"${VCPKG_REL_BIN_DIR}\")
      set(VCPKG_DBG_BIN \"${VCPKG_DBG_BIN_DIR}\")
      set(INSTALL_BIN_DIR \"\${CMAKE_INSTALL_PREFIX}/bin\")
      
      message(STATUS \"======================================================\")
      message(STATUS \"Copying vcpkg dependencies\")
      message(STATUS \"  Source (Release): \${VCPKG_BIN}\")
      message(STATUS \"  Source (Debug):   \${VCPKG_DBG_BIN}\")
      message(STATUS \"  Destination:      \${INSTALL_BIN_DIR}\")
      message(STATUS \"======================================================\")
      
      # Complete list of ALL vcpkg DLLs required by the application
      # Order matters for debugging - grouped by functionality
      set(VCPKG_DLLS
        # ===========================================
        # QR Code generation - CRITICAL for QR feature
        # ===========================================
        \"qrencode.dll\"      # Main QR encoding library
        \"libpng16.dll\"      # PNG support for qrencode
        \"zlib1.dll\"         # Compression library (libpng dependency)
        
        # ===========================================
        # Database - SQLCipher encrypted storage
        # ===========================================
        \"sqlcipher.dll\"     # Encrypted SQLite
        \"sqlite3.dll\"       # SQLite core
        \"tcl90.dll\"         # TCL runtime (sqlcipher may need this)
        
        # ===========================================
        # OpenSSL - Cryptography and TLS
        # ===========================================
        \"libcrypto-3-x64.dll\"  # OpenSSL crypto
        \"libssl-3-x64.dll\"     # OpenSSL SSL/TLS
        \"legacy.dll\"           # OpenSSL legacy algorithms
        
        # ===========================================
        # HTTP/Network - CPR and libcurl
        # ===========================================
        \"cpr.dll\"           # C++ HTTP library
        \"libcurl.dll\"       # curl library (cpr dependency)
        
        # ===========================================
        # Character encoding - libcurl dependencies
        # ===========================================
        \"iconv-2.dll\"       # Character set conversion
        \"charset-1.dll\"     # Character set support
      )
      
      # Track what we successfully copied
      set(COPIED_DLLS \"\")
      set(MISSING_DLLS \"\")
      
      foreach(DLL \${VCPKG_DLLS})
        set(DLL_FOUND FALSE)
        
        # Try release directory first
        set(DLL_SRC \"\${VCPKG_BIN}/\${DLL}\")
        if(EXISTS \"\${DLL_SRC}\")
          file(COPY \"\${DLL_SRC}\" DESTINATION \"\${INSTALL_BIN_DIR}\")
          list(APPEND COPIED_DLLS \"\${DLL}\")
          set(DLL_FOUND TRUE)
        else()
          # Try debug directory as fallback
          set(DLL_DBG_SRC \"\${VCPKG_DBG_BIN}/\${DLL}\")
          if(EXISTS \"\${DLL_DBG_SRC}\")
            file(COPY \"\${DLL_DBG_SRC}\" DESTINATION \"\${INSTALL_BIN_DIR}\")
            list(APPEND COPIED_DLLS \"\${DLL} (debug)\")
            set(DLL_FOUND TRUE)
          endif()
        endif()
        
        if(NOT DLL_FOUND)
          list(APPEND MISSING_DLLS \"\${DLL}\")
        endif()
      endforeach()
      
      # Report results
      message(STATUS \"------------------------------------------------------\")
      message(STATUS \"DLL Copy Summary:\")
      list(LENGTH COPIED_DLLS COPIED_COUNT)
      list(LENGTH MISSING_DLLS MISSING_COUNT)
      message(STATUS \"  Successfully copied: \${COPIED_COUNT} DLLs\")
      
      if(MISSING_COUNT GREATER 0)
        message(STATUS \"  Missing DLLs (\${MISSING_COUNT}):\")
        foreach(DLL \${MISSING_DLLS})
          message(STATUS \"    - \${DLL}\")
        endforeach()
        message(STATUS \"  Note: Some missing DLLs may be optional or statically linked\")
      endif()
      message(STATUS \"------------------------------------------------------\")
    " COMPONENT Runtime)

    # Step 3: Use BundleUtilities (fixup_bundle) for comprehensive dependency resolution
    # fixup_bundle performs a recursive scan of the executable's import table
    # and copies ALL required DLLs, including transitive dependencies.
    # 
    # NOTE: On Windows, fixup_bundle can be unreliable. The explicit DLL copy
    # in Step 2 is the primary mechanism. This step is a safety net.
    install(CODE "
      include(BundleUtilities)
      
      set(INSTALL_BIN_DIR \"\${CMAKE_INSTALL_PREFIX}/bin\")
      set(TARGET_EXE \"\${INSTALL_BIN_DIR}/CriptoGualetQt.exe\")
      
      # Comprehensive search directories for prerequisites
      # Order matters: prefer release over debug
      set(SEARCH_DIRS 
        \"${VCPKG_REL_BIN_DIR}\"
        \"${QT_BIN_DIR}\"
        \"${VCPKG_DBG_BIN_DIR}\"
      )
      
      # Add Windows System directories for system DLLs
      if(DEFINED ENV{SystemRoot})
        list(APPEND SEARCH_DIRS \"\$ENV{SystemRoot}/System32\")
      endif()
      
      message(STATUS \"======================================================\")
      message(STATUS \"Running fixup_bundle for transitive dependencies\")
      message(STATUS \"======================================================\")
      
      if(EXISTS \"\${TARGET_EXE}\")
        message(STATUS \"Target: \${TARGET_EXE}\")
        message(STATUS \"Search paths:\")
        foreach(DIR \${SEARCH_DIRS})
          message(STATUS \"  - \${DIR}\")
        endforeach()
        
        # Collect all DLLs already in the install directory
        file(GLOB EXISTING_DLLS \"\${INSTALL_BIN_DIR}/*.dll\")
        list(LENGTH EXISTING_DLLS DLL_COUNT)
        message(STATUS \"Existing DLLs in install dir: \${DLL_COUNT}\")
        
        # Run fixup_bundle with error handling
        # On Windows, this may not find all dependencies, but it's worth trying
        message(STATUS \"Scanning for additional dependencies...\")
        
        fixup_bundle(
          \"\${TARGET_EXE}\"
          \"\${EXISTING_DLLS}\"
          \"\${SEARCH_DIRS}\"
        )
        
        # Count DLLs after fixup
        file(GLOB FINAL_DLLS \"\${INSTALL_BIN_DIR}/*.dll\")
        list(LENGTH FINAL_DLLS FINAL_COUNT)
        math(EXPR NEW_DLLS \"\${FINAL_COUNT} - \${DLL_COUNT}\")
        
        message(STATUS \"fixup_bundle added \${NEW_DLLS} additional DLL(s)\")
        message(STATUS \"Total DLLs in install directory: \${FINAL_COUNT}\")
      else()
        message(WARNING \"Cannot run fixup_bundle: executable not found at \${TARGET_EXE}\")
      endif()
      message(STATUS \"======================================================\")
    " COMPONENT Runtime)

    # Step 4: Final verification and DLL inventory
    # This provides a complete list of all DLLs in the install directory
    # and verifies that critical dependencies are present.
    install(CODE "
      include(BundleUtilities)
      
      set(INSTALL_BIN_DIR \"\${CMAKE_INSTALL_PREFIX}/bin\")
      set(TARGET_EXE \"\${INSTALL_BIN_DIR}/CriptoGualetQt.exe\")
      
      message(STATUS \"======================================================\")
      message(STATUS \"FINAL VERIFICATION - Installed DLLs\")
      message(STATUS \"======================================================\")
      
      # List all installed DLLs
      file(GLOB ALL_DLLS \"\${INSTALL_BIN_DIR}/*.dll\")
      list(SORT ALL_DLLS)
      
      message(STATUS \"DLLs in \${INSTALL_BIN_DIR}:\")
      foreach(DLL \${ALL_DLLS})
        get_filename_component(DLL_NAME \"\${DLL}\" NAME)
        message(STATUS \"  - \${DLL_NAME}\")
      endforeach()
      
      # Check for critical DLLs that must be present for QR functionality
      set(CRITICAL_DLLS
        \"qrencode.dll\"
        \"libpng16.dll\"
        \"zlib1.dll\"
      )
      
      message(STATUS \"\")
      message(STATUS \"Critical QR Dependencies Check:\")
      set(QR_DEPS_OK TRUE)
      foreach(DLL \${CRITICAL_DLLS})
        if(EXISTS \"\${INSTALL_BIN_DIR}/\${DLL}\")
          message(STATUS \"  [OK] \${DLL}\")
        else()
          message(STATUS \"  [MISSING] \${DLL} - QR generation will fail!\")
          set(QR_DEPS_OK FALSE)
        endif()
      endforeach()
      
      if(NOT QR_DEPS_OK)
        message(WARNING \"QR code generation dependencies are incomplete!\")
        message(STATUS \"Make sure vcpkg has installed libqrencode: vcpkg install libqrencode\")
      endif()
      
      # Check for BIP39 wordlist - CRITICAL for seed phrase generation
      message(STATUS \"\")
      message(STATUS \"Critical Asset Check (BIP39 Wordlist):\")
      set(WORDLIST_PATH \"\${INSTALL_BIN_DIR}/assets/bip39/english.txt\")
      if(EXISTS \"\${WORDLIST_PATH}\")
        # Count lines to verify it's complete (should be 2048 words)
        file(STRINGS \"\${WORDLIST_PATH}\" WORDLIST_LINES)
        list(LENGTH WORDLIST_LINES WORD_COUNT)
        if(WORD_COUNT EQUAL 2048)
          message(STATUS \"  [OK] BIP39 wordlist found with \${WORD_COUNT} words\")
        else()
          message(WARNING \"  [INCOMPLETE] BIP39 wordlist has \${WORD_COUNT} words (expected 2048)\")
        endif()
      else()
        message(STATUS \"  [MISSING] BIP39 wordlist at \${WORDLIST_PATH}\")
        message(WARNING \"  Seed phrase generation will FAIL without the BIP39 wordlist!\")
        message(STATUS \"  Expected location: \${WORDLIST_PATH}\")
        
        # Check if assets directory exists at all
        if(EXISTS \"\${INSTALL_BIN_DIR}/assets\")
          message(STATUS \"  Assets directory exists, checking contents...\")
          file(GLOB_RECURSE ASSET_FILES \"\${INSTALL_BIN_DIR}/assets/*\")
          foreach(ASSET \${ASSET_FILES})
            message(STATUS \"    - \${ASSET}\")
          endforeach()
        else()
          message(STATUS \"  Assets directory does NOT exist at \${INSTALL_BIN_DIR}/assets\")
        endif()
      endif()
      
      # Run verify_app to check for any missing dependencies
      if(EXISTS \"\${TARGET_EXE}\")
        message(STATUS \"\")
        message(STATUS \"Running verify_app() to check for missing dependencies...\")
        verify_app(\"\${TARGET_EXE}\")
        message(STATUS \"Verification completed\")
      endif()
      
      message(STATUS \"======================================================\")
    " COMPONENT Runtime)
    
  elseif(APPLE)
    # -----------------------------------------------------------------------------
    # macOS Bundle Handling
    # -----------------------------------------------------------------------------
    # On macOS, we create an app bundle and use fixup_bundle to handle @rpath
    # and @executable_path references properly.
    
    install(CODE "
      include(BundleUtilities)
      
      set(BUNDLE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/CriptoGualetQt.app\")
      
      if(EXISTS \"\${BUNDLE_PATH}\")
        set(SEARCH_DIRS 
          \"${Qt6_DIR}/../../../lib\"
          \"/usr/local/lib\"
          \"/opt/homebrew/lib\"
        )
        
        fixup_bundle(\"\${BUNDLE_PATH}\" \"\" \"\${SEARCH_DIRS}\")
      endif()
    " COMPONENT Runtime)
    
  elseif(UNIX)
    # -----------------------------------------------------------------------------
    # Linux Handling
    # -----------------------------------------------------------------------------
    # On Linux, RPATH settings (configured in root CMakeLists.txt) handle most cases.
    # We use fixup_bundle to verify and copy any missing vcpkg libraries.
    
    install(CODE "
      include(BundleUtilities)
      
      set(TARGET_EXE \"\${CMAKE_INSTALL_PREFIX}/bin/CriptoGualetQt\")
      
      # Search paths for Linux dependencies
      set(SEARCH_DIRS 
        \"/usr/lib\"
        \"/usr/lib/x86_64-linux-gnu\"
        \"/usr/local/lib\"
      )
      
      # Add vcpkg-installed libraries if present
      if(EXISTS \"${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux\")
        list(APPEND SEARCH_DIRS \"${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux/lib\")
      endif()
      
      if(EXISTS \"\${TARGET_EXE}\")
        message(STATUS \"Running fixup_bundle on Linux to verify dependencies...\")
        fixup_bundle(\"\${TARGET_EXE}\" \"\" \"\${SEARCH_DIRS}\")
        
        # Verify RPATH is correctly set
        execute_process(
          COMMAND chrpath -l \"\${TARGET_EXE}\"
          OUTPUT_VARIABLE RPATH_OUTPUT
          ERROR_VARIABLE RPATH_ERROR
          RESULT_VARIABLE RPATH_RESULT
        )
        if(RPATH_RESULT EQUAL 0)
          message(STATUS \"RPATH info: \${RPATH_OUTPUT}\")
        endif()
      endif()
    " COMPONENT Runtime)
    
    # Install Qt6 plugins for Linux (if not using system Qt)
    install(CODE "
      set(INSTALL_BIN_DIR \"\${CMAKE_INSTALL_PREFIX}/bin\")
      set(QT_PLUGINS_DIR \"\")
      
      # Try to find Qt6 plugins in common locations
      if(EXISTS \"${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux/tools/Qt6/plugins\")
        set(QT_PLUGINS_DIR \"${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux/tools/Qt6/plugins\")
      elseif(EXISTS \"/usr/lib/x86_64-linux-gnu/qt6/plugins\")
        set(QT_PLUGINS_DIR \"/usr/lib/x86_64-linux-gnu/qt6/plugins\")
      endif()
      
      if(QT_PLUGINS_DIR)
        message(STATUS \"Installing Qt6 plugins from: \${QT_PLUGINS_DIR}\")
        
        # Create plugins directory
        file(MAKE_DIRECTORY \"\${INSTALL_BIN_DIR}/plugins\")
        
        # Install common Qt6 plugins
        set(QT_PLUGIN_TYPES "platforms;imageformats;styles")
        
        foreach(PLUGIN_TYPE \${QT_PLUGIN_TYPES})
          if(EXISTS \"\${QT_PLUGINS_DIR}/\${PLUGIN_TYPE}\")
            file(GLOB PLUGINS \"\${QT_PLUGINS_DIR}/\${PLUGIN_TYPE}/*.so\")
            foreach(PLUGIN \${PLUGINS})
              get_filename_component(PLUGIN_NAME \${PLUGIN} NAME)
              file(COPY \${PLUGIN} DESTINATION \"\${INSTALL_BIN_DIR}/plugins/\${PLUGIN_TYPE}\")
              message(STATUS \"  Installed Qt6 \${PLUGIN_TYPE}: \${PLUGIN_NAME}\")
            endforeach()
          endif()
        endforeach()
      endif()
    " COMPONENT Runtime)
  endif()
elseif(BUILD_GUI_QT AND NOT Qt6_FOUND)
  message(STATUS "Qt6 not found - install Qt6 from https://qt.io to build the modern Qt GUI")
endif()
