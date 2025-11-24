# Utility Libraries (QRGenerator, SharedSymbols)

# ---- QR Code library ----
add_library(QRGenerator STATIC
  "QRGenerator.cpp"
  "include/QRGenerator.h"
)
target_include_directories(QRGenerator PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
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

# ---- Shared Symbols library ----
add_library(SharedSymbols STATIC
  "SharedSymbols.cpp"
  "include/SharedTypes.h"
)
target_include_directories(SharedSymbols PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---- RLP Encoder library ----
add_library(RLPEncoder STATIC
  "RLPEncoder.cpp"
  "include/RLPEncoder.h"
)
target_include_directories(RLPEncoder PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---- Secure Credential Store library ----
add_library(SecureCredentialStore STATIC
  "SecureCredentialStore.cpp"
  "include/SecureCredentialStore.h"
)
target_include_directories(SecureCredentialStore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---- Email Service library ----
add_library(EmailService STATIC
  "EmailService.cpp"
  "include/EmailService.h"
)
target_include_directories(EmailService PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
# Link Crypto library for secure random number generation
# Link SecureCredentialStore for secure password storage
# Link libcurl for SMTP communication
target_link_libraries(EmailService PRIVATE 
  Crypto 
  SecureCredentialStore
)

# Find libcurl (should be available via vcpkg since CPR depends on it)
find_package(CURL QUIET)
if(CURL_FOUND)
  target_link_libraries(EmailService PRIVATE ${CURL_LIBRARIES})
  target_include_directories(EmailService PRIVATE ${CURL_INCLUDE_DIRS})
  message(STATUS "EmailService will use libcurl for SMTP")
else()
  # Try to find libcurl in vcpkg_installed
  find_path(CURL_INCLUDE_DIR 
    NAMES curl/curl.h
    PATHS
      ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg_installed/x64-windows/include
  )
  find_library(CURL_LIBRARY
    NAMES libcurl curl
    PATHS
      ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed/x64-windows/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg_installed/x64-windows/lib
  )
  if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
    target_link_libraries(EmailService PRIVATE ${CURL_LIBRARY})
    target_include_directories(EmailService PRIVATE ${CURL_INCLUDE_DIR})
    message(STATUS "EmailService will use libcurl for SMTP (found in vcpkg)")
  else()
    message(WARNING "libcurl not found - EmailService SMTP functionality will not work. Install via: vcpkg install curl")
  endif()
endif()
