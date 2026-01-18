# Core Backend Library (Auth, Crypto, WalletAPI)

# Find secp256k1 library
find_package(unofficial-secp256k1 CONFIG REQUIRED)

# ---- Crypto library (cryptographic functions) ----
add_library(Crypto STATIC
  "Crypto.cpp"
  "include/Crypto.h"
)
target_include_directories(Crypto PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(Crypto PUBLIC unofficial::secp256k1 unofficial::secp256k1_precomputed)

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Crypto PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(Crypto PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(Crypto PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(Crypto PRIVATE -fPIE -pie)
endif()

# ---- Auth library (portable, builds everywhere) ----
add_library(Auth STATIC
  "Auth.cpp"
  "include/Auth.h"
  "SessionManager.cpp"
  "include/SessionManager.h"
  "AuthManager.cpp"
  "include/AuthManager.h"
)
target_include_directories(Auth PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/backend/utils/include"
  "${CMAKE_SOURCE_DIR}/backend/database/include"
  "${CMAKE_SOURCE_DIR}/backend/repository/include"
)
target_link_libraries(Auth PRIVATE Crypto Repository Database SharedSymbols)

# Use cross-platform compiler-specific flags with security features
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT WIN32)
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE -Wno-unused-parameter)
  target_link_options(Auth PRIVATE -fPIE -pie)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(Auth PRIVATE /W4 /permissive- /utf-8 /EHsc /GS)
else()
  target_compile_options(Auth PRIVATE -Wall -Wextra -Wpedantic -fstack-protector-strong -fPIE)
  target_link_options(Auth PRIVATE -fPIE -pie)
endif()

# ---- Wallet API library (high-level wrapper) ----
if(BLOCKCYPHER_AVAILABLE)
  add_library(WalletAPI STATIC
    "WalletAPI.cpp"
    "include/WalletAPI.h"
  )
  target_include_directories(WalletAPI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(WalletAPI PRIVATE BlockCypher EthereumService)
endif()
