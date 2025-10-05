// CriptoGualet.h : Include file for standard system include files,
// or project specific include files.

#pragma once

// Windows headers need to be first with proper defines
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

// Standard library headers
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// Application constants
#define ID_LOGIN_BUTTON 1001
#define ID_REGISTER_BUTTON 1002
#define ID_USERNAME_EDIT 1003
#define ID_PASSWORD_EDIT 1004
#define ID_CREATE_WALLET_BUTTON 1005
#define ID_VIEW_BALANCE_BUTTON 1006
#define ID_SEND_BUTTON 1007
#define ID_RECEIVE_BUTTON 1008
#define ID_LOGOUT_BUTTON 1009

// Application states
enum class AppState { LOGIN_SCREEN, MAIN_WALLET };

// User structure
struct User {
  std::string username;
  std::string email;
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

// Modern UI styling functions
void DrawModernButton(HDC hdc, RECT rect, const wchar_t* text, bool isPressed, bool isHovered);
void DrawModernEdit(HDC hdc, RECT rect, COLORREF bgColor);
LRESULT CALLBACK ModernEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Function declarations
std::string GenerateBitcoinAddress();
std::string GeneratePrivateKey();
