#include "../include/LoginUI.h"
#include "../include/CriptoGualet.h"

extern HWND g_usernameEdit;
extern HWND g_passwordEdit; 
extern HWND g_loginButton;
extern HWND g_registerButton;
extern HFONT g_buttonFont;

void CreateLoginUI(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    int centerX = rect.right / 2;
    int centerY = rect.bottom / 2;

    // Modern styled labels with dark theme
    HWND usernameLabel = CreateWindowW(L"STATIC", L"Username", WS_VISIBLE | WS_CHILD,
        centerX - 140, centerY - 75, 100, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessage(usernameLabel, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);

    // Modern styled input fields with dark background
    g_usernameEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        centerX - 140, centerY - 50, 280, 35, hwnd, (HMENU)ID_USERNAME_EDIT, nullptr, nullptr);

    HWND passwordLabel = CreateWindowW(L"STATIC", L"Password", WS_VISIBLE | WS_CHILD,
        centerX - 140, centerY - 5, 100, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessage(passwordLabel, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);

    g_passwordEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
        centerX - 140, centerY + 20, 280, 35, hwnd, (HMENU)ID_PASSWORD_EDIT, nullptr, nullptr);

    // Custom styled buttons (we'll draw these ourselves)
    g_loginButton = CreateWindowW(L"BUTTON", L"Sign In", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX - 140, centerY + 75, 130, 40, hwnd,
        (HMENU)ID_LOGIN_BUTTON, nullptr, nullptr);

    g_registerButton = CreateWindowW(L"BUTTON", L"Create Account", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        centerX - 5, centerY + 75, 130, 40, hwnd,
        (HMENU)ID_REGISTER_BUTTON, nullptr, nullptr);

    // Apply modern fonts and styling
    SendMessage(g_usernameEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    SendMessage(g_passwordEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    
    // Set edit control background colors
    SetWindowSubclass(g_usernameEdit, [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) -> LRESULT {
        if (uMsg == WM_CTLCOLOREDIT || uMsg == WM_CTLCOLORSTATIC) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(248, 250, 252)); // White text
            SetBkColor(hdc, RGB(45, 55, 72)); // Dark background
            return (LRESULT)CreateSolidBrush(RGB(45, 55, 72));
        }
        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }, 0, 0);

    SetWindowSubclass(g_passwordEdit, [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) -> LRESULT {
        if (uMsg == WM_CTLCOLOREDIT || uMsg == WM_CTLCOLORSTATIC) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(248, 250, 252)); // White text
            SetBkColor(hdc, RGB(45, 55, 72)); // Dark background
            return (LRESULT)CreateSolidBrush(RGB(45, 55, 72));
        }
        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }, 0, 0);

    // Set tab order for better UX
    SetWindowPos(g_usernameEdit, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(g_passwordEdit, g_usernameEdit, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(g_loginButton, g_passwordEdit, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}