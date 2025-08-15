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
    int startY = 150;

    // Wallet information
    if (!g_currentUser.empty() && g_users.find(g_currentUser) != g_users.end()) {
        const User& user = g_users[g_currentUser];
        const std::wstring welcomeText = Widen("Welcome back, " + user.username + "!");
        CreateWindowW(L"STATIC", welcomeText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER,
            centerX - 200, startY, 400, 25, hwnd, nullptr, nullptr, nullptr);

        const std::wstring addressText = Widen("Your Bitcoin Address: " + user.walletAddress);
        CreateWindowW(L"STATIC", addressText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER,
            centerX - 300, startY + 40, 600, 25, hwnd, nullptr, nullptr, nullptr);
    }

    // Wallet action buttons
    CreateWindowW(L"BUTTON", L"View Balance", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX - 150, startY + 100, 120, 40, hwnd,
        (HMENU)ID_VIEW_BALANCE_BUTTON, nullptr, nullptr);

    CreateWindowW(L"BUTTON", L"Send Bitcoin", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX - 20, startY + 100, 120, 40, hwnd,
        (HMENU)ID_SEND_BUTTON, nullptr, nullptr);

    CreateWindowW(L"BUTTON", L"Receive Bitcoin", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX + 110, startY + 100, 120, 40, hwnd,
        (HMENU)ID_RECEIVE_BUTTON, nullptr, nullptr);

    // Logout
    CreateWindowW(L"BUTTON", L"Logout", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX - 50, startY + 170, 100, 30, hwnd,
        (HMENU)ID_LOGOUT_BUTTON, nullptr, nullptr);
}