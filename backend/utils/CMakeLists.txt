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
