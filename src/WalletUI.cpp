#include "../include/WalletUI.h"
#include "../include/CriptoGualet.h"
#include <string>

extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

namespace {
    inline std::wstring Widen(const std::string& s) {
        if (s.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return std::wstring();
        std::wstring w(static_cast<size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
        return w;
    }
}

void CreateWalletUI(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    int centerX = rect.right / 2;
    int startY = 180;

    // Wallet information with modern dark styling
    if (!g_currentUser.empty() && g_users.find(g_currentUser) != g_users.end()) {
        const User& user = g_users[g_currentUser];
        
        // Welcome message with dark theme
        const std::wstring welcomeText = Widen("Welcome back, " + user.username + "!");
        HWND welcomeLabel = CreateWindowW(L"STATIC", welcomeText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER,
            centerX - 250, startY, 500, 30, hwnd, nullptr, nullptr, nullptr);
        
        // Bitcoin address display with dark theme
        const std::wstring addressText = Widen("Address: " + user.walletAddress);
        HWND addressLabel = CreateWindowW(L"STATIC", addressText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER,
            centerX - 350, startY + 50, 700, 25, hwnd, nullptr, nullptr, nullptr);
        
        // Apply font to labels
        extern HFONT g_buttonFont;
        SendMessage(welcomeLabel, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
        SendMessage(addressLabel, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    }

    // Modern styled action buttons with custom drawing
    HWND viewBalanceBtn = CreateWindowW(L"BUTTON", L"View Balance", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX - 180, startY + 120, 140, 50, hwnd,
        (HMENU)ID_VIEW_BALANCE_BUTTON, nullptr, nullptr);

    HWND sendBtn = CreateWindowW(L"BUTTON", L"Send Bitcoin", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX - 30, startY + 120, 140, 50, hwnd,
        (HMENU)ID_SEND_BUTTON, nullptr, nullptr);

    HWND receiveBtn = CreateWindowW(L"BUTTON", L"Receive Bitcoin", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX + 120, startY + 120, 140, 50, hwnd,
        (HMENU)ID_RECEIVE_BUTTON, nullptr, nullptr);

    // Logout button with custom styling
    HWND logoutBtn = CreateWindowW(L"BUTTON", L"Sign Out", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX - 60, startY + 200, 120, 40, hwnd,
        (HMENU)ID_LOGOUT_BUTTON, nullptr, nullptr);
}