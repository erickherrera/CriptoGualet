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
  if(TARGET libqrencode::qrencode)
    target_link_libraries(QRGenerator PUBLIC libqrencode::qrencode)
  elseif(TARGET qrencode)
    target_link_libraries(QRGenerator PUBLIC qrencode)
  else()
    target_include_directories(QRGenerator PUBLIC ${QRENCODE_INCLUDE_DIR})
    target_link_libraries(QRGenerator PUBLIC ${QRENCODE_LIBRARIES})
  endif()
  target_compile_definitions(QRGenerator PUBLIC QRENCODE_AVAILABLE)
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

if(NOT WIN32)
  find_package(unofficial-sodium CONFIG REQUIRED)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(SharedSymbols PRIVATE unofficial-sodium::sodium OpenSSL::Crypto)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(SharedSymbols PRIVATE
    -Wno-global-constructors
    -Wno-exit-time-destructors
    -Wno-unsafe-buffer-usage
    -Wno-unsafe-buffer-usage-in-libc-call
  )
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
add_library(SecureCredentialStore STATIC
  "SecureCredentialStore.cpp"
  "include/SecureCredentialStore.h"
)
target_include_directories(SecureCredentialStore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

