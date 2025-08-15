// CriptoGualet.cpp : Defines the entry point for the application.
//

#include "../include/CriptoGualet.h"

#pragma comment(lib, "Comctl32.lib")

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include "../include/Auth.h"   // <-- use the separated auth module
#include "../include/LoginUI.h"
#include "../include/WalletUI.h"

// ------------------------------ Globals ---------------------------------
AppState g_currentState = AppState::LOGIN_SCREEN;
std::map<std::string, User> g_users;
std::string g_currentUser;
HWND g_mainWindow;
HFONT g_titleFont;
HFONT g_buttonFont;

// Control handles
HWND g_usernameEdit;
HWND g_passwordEdit;
HWND g_loginButton;
HWND g_registerButton;

// ----------------------- Forward declarations ---------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ClearWindow(HWND hwnd);
std::string GenerateBitcoinAddress();
std::string GeneratePrivateKey();

// ---- UTF-8 / UTF-16 helpers (keep Win32 wide + app strings in UTF-8) ----
namespace {
    inline std::wstring Widen(const std::string& s) {
        if (s.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return std::wstring();
        std::wstring w(static_cast<size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
        return w;
    }

    inline std::string Narrow(const std::wstring& w) {
        if (w.empty()) return std::string();
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return std::string();
        std::string s(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, nullptr, nullptr);
        return s;
    }

    inline std::string GetEditTextUTF8(HWND hEdit) {
        int wlen = GetWindowTextLengthW(hEdit);
        if (wlen <= 0) return {};
        std::wstring wbuf(static_cast<size_t>(wlen) + 1, L'\0'); // +1 for NUL
        int got = GetWindowTextW(hEdit, &wbuf[0], wlen + 1);
        if (got < 0) return {};
        wbuf.resize(static_cast<size_t>(got));
        return Narrow(wbuf);
    }
} // namespace

// -------------------------------- WinMain --------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    InitCommonControls();

    // Load existing user data
    Auth::LoadUserDatabase();

    const wchar_t CLASS_NAME[] = L"CriptoGualetWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(50, 30, 50));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassW(&wc);

    // Get screen dimensions for fullscreen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"CriptoGualet - Secure Bitcoin Wallet",
        WS_OVERLAPPEDWINDOW, 0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 0;

    g_mainWindow = hwnd;

    // Create fonts
    g_titleFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_buttonFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteObject(g_titleFont);
    DeleteObject(g_buttonFont);
    return 0;
}

// ------------------------------ WindowProc ------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateLoginUI(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_LOGIN_BUTTON: {
            const std::string username = GetEditTextUTF8(g_usernameEdit);
            const std::string password = GetEditTextUTF8(g_passwordEdit);

            if (Auth::LoginUser(username, password)) {
                g_currentUser = username;
                g_currentState = AppState::MAIN_WALLET;
                ClearWindow(hwnd);
                CreateWalletUI(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else {
                MessageBoxW(hwnd, L"Invalid credentials!", L"Login Failed", MB_OK | MB_ICONERROR);
            }
        } break;

        case ID_REGISTER_BUTTON: {
            const std::string username = GetEditTextUTF8(g_usernameEdit);
            const std::string password = GetEditTextUTF8(g_passwordEdit);

            // Enhanced input validation
            if (username.empty() || password.empty()) {
                MessageBoxW(hwnd, L"Username and password cannot be empty!", 
                    L"Registration Failed", MB_OK | MB_ICONERROR);
            }
            else if (username.length() < 3 || password.length() < 6) {
                MessageBoxW(hwnd,
                    L"Username must be at least 3 characters and password at least 6 characters!",
                    L"Registration Failed", MB_OK | MB_ICONERROR);
            }
            else if (Auth::RegisterUser(username, password)) {
                Auth::SaveUserDatabase(); // Save after registration
                MessageBoxW(hwnd, L"Account created successfully! You can now log in.",
                    L"Registration Successful", MB_OK | MB_ICONINFORMATION);
                SetWindowTextW(g_usernameEdit, L"");
                SetWindowTextW(g_passwordEdit, L"");
            }
            else {
                MessageBoxW(hwnd, L"Username already exists!", L"Registration Failed",
                    MB_OK | MB_ICONERROR);
            }
        } break;

        case ID_LOGOUT_BUTTON:
            g_currentState = AppState::LOGIN_SCREEN;
            g_currentUser.clear();
            ClearWindow(hwnd);
            CreateLoginUI(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case ID_VIEW_BALANCE_BUTTON: {
            if (!g_currentUser.empty() && g_users.find(g_currentUser) != g_users.end()) {
                const User& user = g_users[g_currentUser];
                std::string balanceInfo =
                    "Wallet Address: " + user.walletAddress +
                    "\n\nBalance: 0.00000000 BTC\n\n(Note: This is a demo wallet. "
                    "Real Bitcoin integration would require blockchain API)";
                const std::wstring wInfo = Widen(balanceInfo);
                MessageBoxW(hwnd, wInfo.c_str(), L"Wallet Balance", MB_OK | MB_ICONINFORMATION);
            }
        } break;
        }
    } return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Background
        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 30, 50));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        // Title
        HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
        SetTextAlign(hdc, TA_CENTER | TA_TOP);

        if (g_currentState == AppState::LOGIN_SCREEN) {
            const wchar_t* title = L"CriptoGualet";
            const wchar_t* subtitle = L"Secure Bitcoin Wallet";
            TextOutW(hdc, rect.right / 2, 50, title, (int)wcslen(title));
            SelectObject(hdc, g_buttonFont);
            TextOutW(hdc, rect.right / 2, 120, subtitle, (int)wcslen(subtitle));
        }
        else {
            const wchar_t* title = L"Bitcoin Wallet Dashboard";
            TextOutW(hdc, rect.right / 2, 50, title, (int)wcslen(title));
        }

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
    } return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ------------------------------ UI helpers ------------------------------

void ClearWindow(HWND hwnd) {
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
}

// ------------------------- Demo crypto / RNG ----------------------------
std::string GeneratePrivateKey() {
    // WARNING: This is still a demo implementation
    // Real crypto wallet should use proper cryptographic libraries
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::string privateKey;
    privateKey.reserve(64);
    for (int i = 0; i < 64; i++) {
        int r = dis(gen);
        privateKey += (r < 10) ? static_cast<char>('0' + r)
            : static_cast<char>('a' + r - 10);
    }
    return privateKey;
}

std::string GenerateBitcoinAddress() {
    // WARNING: This is a demo implementation, not valid Base58Check encoding
    // Real Bitcoin addresses require proper cryptographic key derivation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 57);

    std::string address = "1";
    const std::string chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    for (int i = 0; i < 33; i++) {
        address += chars[static_cast<size_t>(dis(gen))];
    }
    return address;
}

// NOTE: Data persistence is now handled by the Auth module with proper security
