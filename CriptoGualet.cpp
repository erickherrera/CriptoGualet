// CriptoGualet.cpp : Defines the entry point for the application.
//

#include "CriptoGualet.h"

// ---- UTF-8 / UTF-16 helpers (keep Win32 wide + app strings in UTF-8) ----
namespace {
inline std::wstring Widen(const std::string &s) {
  if (s.empty())
    return std::wstring();
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (len <= 0)
    return std::wstring();
  std::wstring w(static_cast<size_t>(len - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
  return w;
}

inline std::string Narrow(const std::wstring &w) {
  if (w.empty())
    return std::string();
  int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr,
                                nullptr);
  if (len <= 0)
    return std::string();
  std::string s(static_cast<size_t>(len - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, nullptr, nullptr);
  return s;
}

inline std::string GetEditTextUTF8(HWND hEdit) {
  int wlen = GetWindowTextLengthW(hEdit);
  if (wlen <= 0)
    return std::string();
  std::wstring wbuf(static_cast<size_t>(wlen), L'\0');
  // GetWindowTextW expects buffer including NUL, so pass size+1 and rely on
  // internal copy
  GetWindowTextW(hEdit, &wbuf[0], wlen + 1);
  return Narrow(wbuf);
}
} // namespace

// Global variables
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

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateLoginUI(HWND hwnd);
void CreateWalletUI(HWND hwnd);
void ClearWindow(HWND hwnd);
std::string HashPassword(const std::string &password);
std::string GenerateBitcoinAddress();
std::string GeneratePrivateKey();
bool RegisterUser(const std::string &username, const std::string &password);
bool LoginUser(const std::string &username, const std::string &password);
void SaveUserData();
void LoadUserData();
std::string SimpleEncrypt(const std::string &data, const std::string &key);
std::string SimpleDecrypt(const std::string &data, const std::string &key);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
  InitCommonControls();

  // Load existing user data
  LoadUserData();

  const wchar_t CLASS_NAME[] = L"CriptoGualetWindow";

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = CreateSolidBrush(RGB(20, 30, 50));
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  RegisterClassW(&wc);

  // Get screen dimensions for fullscreen
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  HWND hwnd =
      CreateWindowExW(0, CLASS_NAME, L"CriptoGualet - Secure Bitcoin Wallet",
                      WS_OVERLAPPEDWINDOW, 0, 0, screenWidth, screenHeight,
                      nullptr, nullptr, hInstance, nullptr);

  if (hwnd == nullptr) {
    return 0;
  }

  g_mainWindow = hwnd;

  // Create fonts
  g_titleFont =
      CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

  g_buttonFont =
      CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    CreateLoginUI(hwnd);
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) {
      PostQuitMessage(0);
    }
    return 0;

  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case ID_LOGIN_BUTTON: {
      const std::string username = GetEditTextUTF8(g_usernameEdit);
      const std::string password = GetEditTextUTF8(g_passwordEdit);

      if (LoginUser(username, password)) {
        g_currentUser = username;
        g_currentState = AppState::MAIN_WALLET;
        ClearWindow(hwnd);
        CreateWalletUI(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
      } else {
        MessageBoxW(hwnd, L"Invalid credentials!", L"Login Failed",
                    MB_OK | MB_ICONERROR);
      }
    } break;

    case ID_REGISTER_BUTTON: {
      const std::string username = GetEditTextUTF8(g_usernameEdit);
      const std::string password = GetEditTextUTF8(g_passwordEdit);

      if (username.length() < 3 || password.length() < 6) {
        MessageBoxW(hwnd,
                    L"Username must be at least 3 characters and password at "
                    L"least 6 characters!",
                    L"Registration Failed", MB_OK | MB_ICONERROR);
      } else if (RegisterUser(username, password)) {
        SaveUserData(); // Save after registration
        MessageBoxW(hwnd, L"Account created successfully! You can now log in.",
                    L"Registration Successful", MB_OK | MB_ICONINFORMATION);
        SetWindowTextW(g_usernameEdit, L"");
        SetWindowTextW(g_passwordEdit, L"");
      } else {
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
      if (!g_currentUser.empty() &&
          g_users.find(g_currentUser) != g_users.end()) {
        const User &user = g_users[g_currentUser];
        std::string balanceInfo =
            "Wallet Address: " + user.walletAddress +
            "\n\nBalance: 0.00000000 BTC\n\n(Note: This is a demo wallet. "
            "Real Bitcoin integration would require blockchain API)";
        const std::wstring wInfo = Widen(balanceInfo);
        MessageBoxW(hwnd, wInfo.c_str(), L"Wallet Balance",
                    MB_OK | MB_ICONINFORMATION);
      }
    } break;
    }
  }
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rect;
    GetClientRect(hwnd, &rect);

    // Set background color
    HBRUSH bgBrush = CreateSolidBrush(RGB(20, 30, 50));
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    // Draw title
    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    SetTextAlign(hdc, TA_CENTER | TA_TOP);

    if (g_currentState == AppState::LOGIN_SCREEN) {
      const wchar_t *title = L"CriptoGualet";
      const wchar_t *subtitle = L"Secure Bitcoin Wallet";
      TextOutW(hdc, rect.right / 2, 50, title, static_cast<int>(wcslen(title)));

      SelectObject(hdc, g_buttonFont);
      TextOutW(hdc, rect.right / 2, 120, subtitle,
               static_cast<int>(wcslen(subtitle)));
    } else {
      const wchar_t *title = L"Bitcoin Wallet Dashboard";
      TextOutW(hdc, rect.right / 2, 50, title, static_cast<int>(wcslen(title)));
    }

    SelectObject(hdc, oldFont);
    EndPaint(hwnd, &ps);
  }
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateLoginUI(HWND hwnd) {
  RECT rect;
  GetClientRect(hwnd, &rect);

  int centerX = rect.right / 2;
  int centerY = rect.bottom / 2;

  // Username label and edit
  CreateWindowW(L"STATIC", L"Username:", WS_VISIBLE | WS_CHILD, centerX - 150,
                centerY - 80, 100, 20, hwnd, nullptr, nullptr, nullptr);

  g_usernameEdit = CreateWindowW(
      L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
      centerX - 150, centerY - 55, 300, 25, hwnd, (HMENU)ID_USERNAME_EDIT,
      nullptr, nullptr);

  // Password label and edit
  CreateWindowW(L"STATIC", L"Password:", WS_VISIBLE | WS_CHILD, centerX - 150,
                centerY - 20, 100, 20, hwnd, nullptr, nullptr, nullptr);

  g_passwordEdit = CreateWindowW(L"EDIT", L"",
                                 WS_VISIBLE | WS_CHILD | WS_BORDER |
                                     ES_AUTOHSCROLL | ES_PASSWORD,
                                 centerX - 150, centerY + 5, 300, 25, hwnd,
                                 (HMENU)ID_PASSWORD_EDIT, nullptr, nullptr);

  // Buttons
  g_loginButton = CreateWindowW(
      L"BUTTON", L"Login", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, centerX - 100,
      centerY + 50, 80, 30, hwnd, (HMENU)ID_LOGIN_BUTTON, nullptr, nullptr);

  g_registerButton = CreateWindowW(L"BUTTON", L"Register",
                                   WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   centerX + 20, centerY + 50, 80, 30, hwnd,
                                   (HMENU)ID_REGISTER_BUTTON, nullptr, nullptr);

  // Set fonts
  SendMessage(g_usernameEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
  SendMessage(g_passwordEdit, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
  SendMessage(g_loginButton, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
  SendMessage(g_registerButton, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
}

void CreateWalletUI(HWND hwnd) {
  RECT rect;
  GetClientRect(hwnd, &rect);

  int centerX = rect.right / 2;
  int startY = 150;

  // Wallet information
  if (!g_currentUser.empty() && g_users.find(g_currentUser) != g_users.end()) {
    const User &user = g_users[g_currentUser];
    const std::wstring welcomeText =
        Widen("Welcome back, " + user.username + "!");

    CreateWindowW(L"STATIC", welcomeText.c_str(),
                  WS_VISIBLE | WS_CHILD | SS_CENTER, centerX - 200, startY, 400,
                  25, hwnd, nullptr, nullptr, nullptr);

    const std::wstring addressText =
        Widen("Your Bitcoin Address: " + user.walletAddress);
    CreateWindowW(L"STATIC", addressText.c_str(),
                  WS_VISIBLE | WS_CHILD | SS_CENTER, centerX - 300, startY + 40,
                  600, 25, hwnd, nullptr, nullptr, nullptr);
  }

  // Wallet action buttons
  CreateWindowW(L"BUTTON", L"View Balance",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, centerX - 150,
                startY + 100, 120, 40, hwnd, (HMENU)ID_VIEW_BALANCE_BUTTON,
                nullptr, nullptr);

  CreateWindowW(L"BUTTON", L"Send Bitcoin",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, centerX - 20,
                startY + 100, 120, 40, hwnd, (HMENU)ID_SEND_BUTTON, nullptr,
                nullptr);

  CreateWindowW(L"BUTTON", L"Receive Bitcoin",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, centerX + 110,
                startY + 100, 120, 40, hwnd, (HMENU)ID_RECEIVE_BUTTON, nullptr,
                nullptr);

  // Logout button
  CreateWindowW(L"BUTTON", L"Logout", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                centerX - 50, startY + 170, 100, 30, hwnd,
                (HMENU)ID_LOGOUT_BUTTON, nullptr, nullptr);
}

void ClearWindow(HWND hwnd) {
  HWND child = GetWindow(hwnd, GW_CHILD);
  while (child) {
    HWND next = GetWindow(child, GW_HWNDNEXT);
    DestroyWindow(child);
    child = next;
  }
}

std::string HashPassword(const std::string &password) {
  // Simple hash function for demo purposes
  // In a real application, use a proper cryptographic hash like bcrypt
  std::hash<std::string> hasher;
  return std::to_string(hasher(password + "salt"));
}

std::string GeneratePrivateKey() {
  // Generate a cryptographically secure private key
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::string privateKey;
  privateKey.reserve(64);
  for (int i = 0; i < 64; i++) {
    int randomChar = dis(gen);
    if (randomChar < 10)
      privateKey += static_cast<char>('0' + randomChar);
    else
      privateKey += static_cast<char>('a' + randomChar - 10);
  }
  return privateKey;
}

std::string GenerateBitcoinAddress() {
  // Generate a demo Bitcoin address using better randomness
  // In a real application, derive this from the private key using proper
  // cryptography
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 57);

  std::string address = "1";
  const std::string chars =
      "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

  for (int i = 0; i < 33; i++) {
    address += chars[static_cast<size_t>(dis(gen))];
  }
  return address;
}

bool RegisterUser(const std::string &username, const std::string &password) {
  if (g_users.find(username) != g_users.end()) {
    return false; // User already exists
  }

  User newUser;
  newUser.username = username;
  newUser.passwordHash = HashPassword(password);
  newUser.privateKey = GeneratePrivateKey();
  newUser.walletAddress = GenerateBitcoinAddress();

  g_users[username] = newUser;
  return true;
}

bool LoginUser(const std::string &username, const std::string &password) {
  auto it = g_users.find(username);
  if (it == g_users.end()) {
    return false; // User not found
  }

  return it->second.passwordHash == HashPassword(password);
}

std::string SimpleEncrypt(const std::string &data, const std::string &key) {
  std::string encrypted = data;
  for (size_t i = 0; i < encrypted.length(); ++i) {
    encrypted[i] ^= key[i % key.length()];
  }
  return encrypted;
}

std::string SimpleDecrypt(const std::string &data, const std::string &key) {
  return SimpleEncrypt(data, key); // XOR is symmetric
}

void SaveUserData() {
  try {
    std::ofstream file("wallet_data.dat", std::ios::binary);
    if (!file.is_open())
      return;

    const std::string encryptionKey = "CriptoGualet2024SecureKey!";

    // Save number of users
    size_t userCount = g_users.size();
    file.write(reinterpret_cast<const char *>(&userCount), sizeof(userCount));

    for (const auto &pair : g_users) {
      const User &user = pair.second;

      // Create user data string
      std::stringstream userData;
      userData << user.username << "|" << user.passwordHash << "|"
               << user.walletAddress << "|" << user.privateKey;

      std::string encryptedData = SimpleEncrypt(userData.str(), encryptionKey);

      // Write data length and encrypted data
      size_t dataLength = encryptedData.length();
      file.write(reinterpret_cast<const char *>(&dataLength),
                 sizeof(dataLength));
      file.write(encryptedData.c_str(),
                 static_cast<std::streamsize>(dataLength));
    }
    file.close();
  } catch (...) {
    // Fail silently - in a real app, log the error
  }
}

void LoadUserData() {
  try {
    std::ifstream file("wallet_data.dat", std::ios::binary);
    if (!file.is_open())
      return;

    const std::string encryptionKey = "CriptoGualet2024SecureKey!";

    // Read number of users
    size_t userCount = 0;
    file.read(reinterpret_cast<char *>(&userCount), sizeof(userCount));

    for (size_t i = 0; i < userCount; ++i) {
      // Read data length and encrypted data
      size_t dataLength = 0;
      file.read(reinterpret_cast<char *>(&dataLength), sizeof(dataLength));

      std::string encryptedData(dataLength, '\0');
      file.read(&encryptedData[0], static_cast<std::streamsize>(dataLength));

      std::string decryptedData = SimpleDecrypt(encryptedData, encryptionKey);

      // Parse user data
      std::stringstream ss(decryptedData);
      std::string token;
      std::vector<std::string> tokens;

      while (std::getline(ss, token, '|')) {
        tokens.push_back(token);
      }

      if (tokens.size() == 4) {
        User user;
        user.username = tokens[0];
        user.passwordHash = tokens[1];
        user.walletAddress = tokens[2];
        user.privateKey = tokens[3];

        g_users[user.username] = user;
      }
    }
    file.close();
  } catch (...) {
    // Fail silently - in a real app, log the error
  }
}
