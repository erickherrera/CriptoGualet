// CriptoGualet.cpp : Defines the entry point for the application.
//
#pragma comment(lib, "Comctl32.lib")

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include "../include/Auth.h"   // <-- use the separated auth module
#include "../include/LoginUI.h"
#include "../include/WalletUI.h"
#include "../include/CriptoGualet.h"

// ------------------------------ Globals ---------------------------------
AppState g_currentState = AppState::LOGIN_SCREEN;
std::map<std::string, User> g_users;
std::string g_currentUser;
#ifndef QT_VERSION  // Only compile Windows GUI code for non-Qt builds
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
void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color);
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
#ifndef QT_VERSION  // Only compile WinMain for non-Qt builds
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    InitCommonControls();

    // Load existing user data
    Auth::LoadUserDatabase();

    const wchar_t CLASS_NAME[] = L"CriptoGualetWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(15, 23, 42));
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

    // Create modern fonts
    g_titleFont = CreateFontW(66, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI Light");
    g_buttonFont = CreateFontW(20, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup
    DeleteObject(g_titleFont);
    DeleteObject(g_buttonFont);
    return 0;
}
#endif // QT_VERSION

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

            Auth::AuthResponse response = Auth::LoginUser(username, password);
            if (response.success()) {
                g_currentUser = username;
                g_currentState = AppState::MAIN_WALLET;
                ClearWindow(hwnd);
                CreateWalletUI(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else {
                const std::wstring wMessage = Widen(response.message);
                MessageBoxW(hwnd, wMessage.c_str(), L"Login Failed", MB_OK | MB_ICONERROR);
            }
        } break;

        case ID_REGISTER_BUTTON: {
            const std::string username = GetEditTextUTF8(g_usernameEdit);
            const std::string password = GetEditTextUTF8(g_passwordEdit);

            Auth::AuthResponse response = Auth::RegisterUser(username, password);
            if (response.success()) {
                const std::wstring wMessage = Widen("Account created successfully!\nYour Bitcoin address: " + 
                    g_users[username].walletAddress + "\n\nYou can now log in.");
                MessageBoxW(hwnd, wMessage.c_str(), L"Registration Successful", MB_OK | MB_ICONINFORMATION);
                SetWindowTextW(g_usernameEdit, L"");
                SetWindowTextW(g_passwordEdit, L"");
            }
            else {
                const std::wstring wMessage = Widen(response.message);
                MessageBoxW(hwnd, wMessage.c_str(), L"Registration Failed", MB_OK | MB_ICONERROR);
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

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hControl = (HWND)lParam;
        
        // Check if this is one of our edit controls
        if (hControl == g_usernameEdit || hControl == g_passwordEdit) {
            SetTextColor(hdc, RGB(248, 250, 252)); // White text
            SetBkColor(hdc, RGB(45, 55, 72)); // Dark background
            return (LRESULT)CreateSolidBrush(RGB(45, 55, 72));
        }
        
        // For labels, use transparent background
        SetTextColor(hdc, RGB(248, 250, 252)); // White text
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        
        if (dis->CtlType == ODT_BUTTON) {
            bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
            bool isHovered = (dis->itemState & ODS_FOCUS) != 0;
            
            const wchar_t* buttonText = L"";
            if (dis->CtlID == ID_LOGIN_BUTTON) {
                buttonText = L"Sign In";
            } else if (dis->CtlID == ID_REGISTER_BUTTON) {
                buttonText = L"Create Account";
            } else if (dis->CtlID == ID_LOGOUT_BUTTON) {
                buttonText = L"Sign Out";
            } else if (dis->CtlID == ID_VIEW_BALANCE_BUTTON) {
                buttonText = L"View Balance";
            } else if (dis->CtlID == ID_SEND_BUTTON) {
                buttonText = L"Send Bitcoin";
            } else if (dis->CtlID == ID_RECEIVE_BUTTON) {
                buttonText = L"Receive Bitcoin";
            }
            
            DrawModernButton(dis->hDC, dis->rcItem, buttonText, isPressed, isHovered);
        }
        return TRUE;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Modern gradient background
        RECT gradientRect = rect;
        
        // Create gradient from primary teal to darker shade
        for (int y = 0; y < rect.bottom; y++) {
            float factor = (float)y / rect.bottom;
            int r = (int)(20 * (1 - factor * 0.3));
            int g = (int)(70 * (1 - factor * 0.2));
            int b = (int)(80 * (1 - factor * 0.1));
            
            HBRUSH lineBrush = CreateSolidBrush(RGB(r, g, b));
            RECT lineRect = {0, y, rect.right, y + 1};
            FillRect(hdc, &lineRect, lineBrush);
            DeleteObject(lineBrush);
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(248, 250, 252));

        // Title
        HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
        SetTextAlign(hdc, TA_CENTER | TA_TOP);

        if (g_currentState == AppState::LOGIN_SCREEN) {
            // Draw modern card background for login form
            RECT cardRect;
            cardRect.left = rect.right / 2 - 200;
            cardRect.right = rect.right / 2 + 200;
            cardRect.top = rect.bottom / 2 - 120;
            cardRect.bottom = rect.bottom / 2 + 145;
            
            // Draw shadow first (behind the card)
            RECT shadowRect = cardRect;
            OffsetRect(&shadowRect, 4, 4);
            DrawRoundedRect(hdc, shadowRect, 16, RGB(8, 10, 15));
            
            // Draw the main card on top
            DrawRoundedRect(hdc, cardRect, 16, RGB(30, 41, 59));
            
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

void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Draw filled rounded rectangle
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// Modern button drawing with rounded corners and gradient
void DrawModernButton(HDC hdc, RECT rect, const wchar_t* text, bool isPressed, bool isHovered) {
    // Button colors
    COLORREF baseColor = RGB(51, 65, 85);  // slate-600
    COLORREF hoverColor = RGB(71, 85, 105); // slate-500
    COLORREF pressedColor = RGB(30, 41, 59); // slate-800
    COLORREF textColor = RGB(248, 250, 252); // slate-50
    
    COLORREF buttonColor = isPressed ? pressedColor : (isHovered ? hoverColor : baseColor);
    
    // First, fill the entire rect with the window background color to eliminate white corners
    HBRUSH bgBrush = CreateSolidBrush(RGB(15, 23, 42)); // Match window background
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);
    
    // Draw button background with rounded corners
    DrawRoundedRect(hdc, rect, 8, buttonColor);
    
    // Draw button text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    SetTextAlign(hdc, TA_CENTER | TA_TOP);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, g_buttonFont);
    
    // Calculate vertical center manually
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int textY = (rect.top + rect.bottom - tm.tmHeight) / 2;
    
    TextOutW(hdc, (rect.left + rect.right) / 2, textY, text, (int)wcslen(text));
    SelectObject(hdc, oldFont);
}

// Modern edit field drawing
void DrawModernEdit(HDC hdc, RECT rect, COLORREF bgColor) {
    // Draw rounded border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(71, 85, 105)); // slate-500
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bgBrush);
    
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(bgBrush);
}

// Custom window procedure for modern edit controls
WNDPROC g_originalEditProc = nullptr;
LRESULT CALLBACK ModernEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        // Dark background for edit control
        COLORREF editBgColor = RGB(45, 55, 72); // slate-700
        DrawModernEdit(hdc, rect, editBgColor);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(248, 250, 252)); // White text
        SetBkColor(hdcEdit, RGB(45, 55, 72)); // Dark background
        return (LRESULT)CreateSolidBrush(RGB(45, 55, 72));
    }
    }
    
    return CallWindowProc(g_originalEditProc, hwnd, uMsg, wParam, lParam);
}
#endif // QT_VERSION

// ------------------------- Cryptographic Functions ----------------------------

// Base58 alphabet for Bitcoin addresses
static const std::string BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Convert bytes to Base58 encoding
std::string EncodeBase58(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> temp(data);
    
    // Count leading zeros
    int leadingZeros = 0;
    for (size_t i = 0; i < temp.size() && temp[i] == 0; i++) {
        leadingZeros++;
    }
    
    std::string result;
    
    // Convert to base58
    while (!temp.empty()) {
        int remainder = 0;
        for (size_t i = 0; i < temp.size(); i++) {
            int num = remainder * 256 + temp[i];
            temp[i] = num / 58;
            remainder = num % 58;
        }
        
        result = BASE58_ALPHABET[remainder] + result;
        
        // Remove leading zeros from temp
        while (!temp.empty() && temp[0] == 0) {
            temp.erase(temp.begin());
        }
    }
    
    // Add leading '1's for each leading zero byte
    for (int i = 0; i < leadingZeros; i++) {
        result = '1' + result;
    }
    
    return result;
}

// SHA256 hash function using Windows CryptoAPI
std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<uint8_t> hash(32);
    
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("Failed to open SHA256 provider");
    }
    
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to create hash");
    }
    
    status = BCryptHashData(hHash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to hash data");
    }
    
    status = BCryptFinishHash(hHash, hash.data(), 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to finish hash");
    }
    
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hash;
}

// Generate cryptographically secure random private key
std::string GeneratePrivateKey() {
    unsigned char privateKeyBytes[32];
    
    // Generate 32 secure random bytes using Windows CryptoAPI
    HCRYPTPROV hProv;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Failed to acquire crypto context");
    }
    
    if (!CryptGenRandom(hProv, 32, privateKeyBytes)) {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    
    CryptReleaseContext(hProv, 0);
    
    // Ensure the private key is within valid range for secp256k1
    // Must be between 1 and n-1 where n is the order of secp256k1
    // For simplicity, we'll just ensure it's not all zeros
    bool allZeros = true;
    for (int i = 0; i < 32; i++) {
        if (privateKeyBytes[i] != 0) {
            allZeros = false;
            break;
        }
    }
    
    if (allZeros) {
        privateKeyBytes[31] = 1; // Ensure non-zero
    }
    
    // Convert to hex string
    std::string privateKey;
    privateKey.reserve(64);
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf_s(hex, "%02x", privateKeyBytes[i]);
        privateKey += hex;
    }
    
    return privateKey;
}

std::string GenerateBitcoinAddress() {
    try {
        // Generate private key
        std::string privateKeyHex = GeneratePrivateKey();
        
        // Convert hex private key to bytes
        std::vector<uint8_t> privateKeyBytes(32);
        for (int i = 0; i < 32; i++) {
            sscanf_s(privateKeyHex.substr(i * 2, 2).c_str(), "%02hhx", &privateKeyBytes[i]);
        }
        
        // For this demo, we'll generate a simplified public key hash
        // In a real implementation, you would:
        // 1. Derive public key from private key using secp256k1 elliptic curve
        // 2. Hash the public key with SHA256 then RIPEMD160
        
        // Simulate public key hash (20 bytes)
        unsigned char pubKeyHash[20];
        
        // Use SHA256 of private key as a simplified public key hash
        std::vector<uint8_t> sha256Result = SHA256Hash(privateKeyBytes);
        
        // For this demo, use first 20 bytes of SHA256 (instead of RIPEMD160)
        // In production, you would use proper RIPEMD160 after SHA256
        for (int i = 0; i < 20; i++) {
            pubKeyHash[i] = sha256Result[i];
        }
        
        // Create address payload: version byte (0x00 for mainnet) + pubKeyHash
        std::vector<uint8_t> addressPayload;
        addressPayload.push_back(0x00); // Version byte for P2PKH mainnet
        for (int i = 0; i < 20; i++) {
            addressPayload.push_back(pubKeyHash[i]);
        }
        
        // Calculate checksum (first 4 bytes of double SHA256)
        std::vector<uint8_t> firstHash = SHA256Hash(addressPayload);
        std::vector<uint8_t> secondHash = SHA256Hash(firstHash);
        
        // Append checksum to payload
        for (int i = 0; i < 4; i++) {
            addressPayload.push_back(secondHash[i]);
        }
        
        // Encode with Base58
        return EncodeBase58(addressPayload);
        
    } catch (const std::exception&) {
        // Fallback to a recognizable demo address if crypto fails
        return "1Demo" + std::to_string(rand() % 100000) + "BitcoinAddress";
    }
}

// NOTE: Data persistence is now handled by the Auth module with proper security
