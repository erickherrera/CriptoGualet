// SharedTypes.h - Common types and structures used across the application
#pragma once

#include <string>
#include <map>
#include <vector>

// User structure
struct User {
    std::string username;
    std::string passwordHash;
    std::string walletAddress;
    std::string privateKey;
};

// Global variables
extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

// Function declarations
std::string GenerateBitcoinAddress();
std::string GeneratePrivateKey();
