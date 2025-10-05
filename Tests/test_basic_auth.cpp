#include "Auth.h"
#include "SharedTypes.h"
#include <iostream>
#include <string>

int main() {
  std::cout << "Testing Auth system directly...\n";

  // Test simple registration
  std::string username = "testuser";
  std::string password = "TestPass123!";

  std::cout << "Testing with username: " << username << std::endl;
  std::cout << "Testing with password: " << password << std::endl;

  Auth::AuthResponse response = Auth::RegisterUser(username, password);

  std::cout << "Result: " << (response.success() ? "SUCCESS" : "FAILED")
            << std::endl;
  std::cout << "Message: " << response.message << std::endl;
  std::cout << "Result code: " << static_cast<int>(response.result)
            << std::endl;

  if (response.success()) {
    std::cout << "User created successfully!" << std::endl;
    if (g_users.find(username) != g_users.end()) {
      std::cout << "User exists in map: YES" << std::endl;
      std::cout << "Wallet address: " << g_users[username].walletAddress
                << std::endl;
    } else {
      std::cout << "User exists in map: NO" << std::endl;
    }
  } else {
    std::cout << "Registration failed!" << std::endl;
  }

  return 0;
}
