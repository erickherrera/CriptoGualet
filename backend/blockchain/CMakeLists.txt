# Blockchain Integration Library (BlockCypher API)

if(BLOCKCYPHER_AVAILABLE)
  message(STATUS "Building BlockCypher API integration")

  add_library(BlockCypher STATIC
    "BlockCypher.cpp"
    "include/BlockCypher.h"
  )
  target_include_directories(BlockCypher PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(BlockCypher PRIVATE cpr::cpr nlohmann_json::nlohmann_json)

  # Price Service Library
  message(STATUS "Building Price Service")
  add_library(PriceService STATIC
    "PriceService.cpp"
    "include/PriceService.h"
  )
  target_include_directories(PriceService PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(PriceService PRIVATE cpr::cpr nlohmann_json::nlohmann_json)

  # Ethereum Service Library
  message(STATUS "Building Ethereum Service")
  add_library(EthereumService STATIC
    "EthereumService.cpp"
    "include/EthereumService.h"
  )
  target_include_directories(EthereumService PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(EthereumService PRIVATE cpr::cpr nlohmann_json::nlohmann_json)
else()
  message(STATUS "BlockCypher API will not be built - dependencies not found")
  message(STATUS "Run 'vcpkg install' to install dependencies: cpr, nlohmann-json")
endif()
