// CriptoGualet.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <commctrl.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

// Application constants
#define ID_LOGIN_BUTTON 1001
#define ID_REGISTER_BUTTON 1002
#define ID_USERNAME_EDIT 1003
#define ID_PASSWORD_EDIT 1004
#define ID_CREATE_WALLET_BUTTON 1005
#define ID_VIEW_BALANCE_BUTTON 1006
#define ID_SEND_BUTTON 1007
#define ID_RECEIVE_BUTTON 1008s
#define ID_LOGOUT_BUTTON 1009

// Application states
enum class AppState { LOGIN_SCREEN, MAIN_WALLET };

// User structure
struct User {
  std::string username;
  std::string passwordHash;
  std::string walletAddress;
  std::string privateKey;
};

// Global variables
extern AppState g_currentState;
extern std::map<std::string, User> g_users;
extern std::string g_currentUser;
extern HWND g_mainWindow;
extern HFONT g_titleFont;
extern HFONT g_buttonFont;
