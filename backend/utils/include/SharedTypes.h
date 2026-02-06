// SharedTypes.h - Common types and structures used across the application
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

// User structure
// SECURITY: privateKey removed -- keys are derived on-demand from encrypted seed
//           and wiped immediately after use. Never cache private keys in memory.
struct User {
    std::string username;
    std::string passwordHash;
    std::string walletAddress;
};

// Thread-safe access to global user state
// All reads/writes to g_users and g_currentUser MUST hold this mutex.
extern std::mutex g_usersMutex;
extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

// Function declarations
std::string GenerateBitcoinAddress();
std::string GeneratePrivateKey();
