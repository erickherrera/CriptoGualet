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

    // Username label and edit
    CreateWindowW(L"STATIC", L"Username:", WS_VISIBLE | WS_CHILD,
        centerX - 150, centerY - 80, 100, 20, hwnd, nullptr, nullptr, nullptr);

    g_usernameEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
        centerX - 150, centerY - 55, 300, 25, hwnd, (HMENU)ID_USERNAME_EDIT, nullptr, nullptr);

    // Password label and edit
    CreateWindowW(L"STATIC", L"Password:", WS_VISIBLE | WS_CHILD,
        centerX - 150, centerY - 20, 100, 20, hwnd, nullptr, nullptr, nullptr);

    g_passwordEdit = CreateWindowW(
        L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
        centerX - 150, centerY + 5, 300, 25, hwnd, (HMENU)ID_PASSWORD_EDIT, nullptr, nullptr);

    // Buttons
    g_loginButton = CreateWindowW(L"BUTTON", L"Login", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX - 100, centerY + 50, 80, 30, hwnd,
        (HMENU)ID_LOGIN_BUTTON, nullptr, nullptr);

    g_registerButton = CreateWindowW(L"BUTTON", L"Register", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        centerX + 20, centerY + 50, 80, 30, hwnd,
        (HMENU)ID_REGISTER_BUTTON, nullptr, nullptr);

    // Fonts
    SendMessage(g_usernameEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    SendMessage(g_passwordEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    SendMessage(g_loginButton, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
    SendMessage(g_registerButton, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);

    // Set tab order
    SetWindowPos(g_usernameEdit, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(g_passwordEdit, g_usernameEdit, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}