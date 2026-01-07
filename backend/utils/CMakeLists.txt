# Utility Libraries (QRGenerator, SharedSymbols, SecureCredentialStore)

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

# Platform-specific linking for SharedSymbols (uses crypto functions)
if(WIN32)
  target_link_libraries(SharedSymbols PRIVATE Bcrypt Crypt32)
endif()

# ---- RLP Encoder library ----
add_library(RLPEncoder STATIC
  "RLPEncoder.cpp"
  "include/RLPEncoder.h"
)
target_include_directories(RLPEncoder PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---- Secure Credential Store library ----
# Platform-specific source files
if(WIN32)
  set(SECURE_STORE_SOURCES "SecureCredentialStore_Windows.cpp")
elseif(APPLE)
  set(SECURE_STORE_SOURCES "SecureCredentialStore_macOS.cpp")
else()
  # Linux - placeholder
  message(WARNING "SecureCredentialStore not implemented for Linux")
  set(SECURE_STORE_SOURCES "")
endif()

if(SECURE_STORE_SOURCES)
  add_library(SecureCredentialStore STATIC
    ${SECURE_STORE_SOURCES}
    "include/SecureCredentialStore.h"
  )
  target_include_directories(SecureCredentialStore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )

  # Platform-specific linking for SecureCredentialStore
  if(WIN32)
    target_link_libraries(SecureCredentialStore PRIVATE Advapi32 Credui)
  elseif(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(SecureCredentialStore PRIVATE 
      ${SECURITY_FRAMEWORK} 
      ${COREFOUNDATION_FRAMEWORK})
  endif()
endif()
