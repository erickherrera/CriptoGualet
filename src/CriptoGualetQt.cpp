#include "../frontend/qt/include/CriptoGualetQt.h"
#include "../backend/core/include/Auth.h"
#include "../backend/core/include/AuthManager.h"
#include "../backend/core/include/Crypto.h"
#include "../backend/core/include/WalletAPI.h"
#include "../backend/repository/include/Repository/RepositoryTypes.h"
#include "../backend/utils/include/SharedTypes.h"
#include "../frontend/qt/include/QtLoginUI.h"
#include "../frontend/qt/include/QtSeedDisplayDialog.h"
#include "../frontend/qt/include/QtSettingsUI.h"
#include "../frontend/qt/include/QtSidebar.h"
#include "../frontend/qt/include/QtSuccessDialog.h"
#include "../frontend/qt/include/QtThemeManager.h"
#include "../frontend/qt/include/QtTopCryptosPage.h"
#include "../frontend/qt/include/QtWalletUI.h"
#include "../frontend/qt/include/QtTestConsole.h"

#include <QTimer>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QRunnable>
#include <QStackedWidget>
#include <QStatusBar>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace {
constexpr const char* kProviderTypeKey = "btc.provider";
constexpr const char* kRpcUrlKey = "btc.rpc.url";
constexpr const char* kRpcUsernameKey = "btc.rpc.username";
constexpr const char* kRpcPasswordKey = "btc.rpc.password";
constexpr const char* kRpcAllowInsecureKey = "btc.rpc.allow_insecure";
constexpr const char* kProviderFallbackKey = "btc.provider.fallback";
}  // namespace

CriptoGualetQt::CriptoGualetQt(QWidget* parent)
    : QMainWindow(parent),
      m_threadPool(nullptr),
      m_stackedWidget(nullptr),
      m_loginUI(nullptr),
      m_walletUI(nullptr),
      m_themeManager(nullptr),
      m_centralWidget(nullptr),
      m_mainLayout(nullptr),
      m_contentLayout(nullptr),
      m_contentWidget(nullptr),
      m_sidebar(nullptr) {
    // Use global thread pool for background operations
    m_threadPool = QThreadPool::globalInstance();
    m_threadPool->setMaxThreadCount(4);  // Limit concurrent threads

    // Initialize theme manager first - ensure singleton is properly constructed
    try {
        m_themeManager = &QtThemeManager::instance();
    } catch (...) {
        qCritical() << "Failed to initialize theme manager";
        m_themeManager = nullptr;
    }

    // Clean up sessions
    Auth::AuthManager::getInstance().cleanupSessions();
    setWindowTitle("CriptoGualet - Securely own your cryptos");
    setWindowIcon(QIcon(":/icons/icons/logo_criptogualet.ico"));
    setMinimumSize(800, 600);

    // Set window to windowed fullscreen (maximized)
    setWindowState(Qt::WindowMaximized);

    // Ensure window is visible and properly positioned
    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    // Initialize database and repositories
    if (!initializeRepositories()) {
        qCritical() << "Critical: Failed to initialize repositories. Application may not function "
                       "correctly.";
    }

    // Initialize Bitcoin wallet
    m_wallet = std::make_unique<WalletAPI::SimpleWallet>("btc/test3");

    // Initialize Ethereum wallet (PHASE 1 FIX: Multi-chain support)
    m_ethereumWallet = std::make_unique<WalletAPI::EthereumWallet>("mainnet");

    // Initialize Litecoin wallet
    m_litecoinWallet = std::make_unique<WalletAPI::LitecoinWallet>("ltc/main");

    setupUI();
    setupMenuBar();
    setupStatusBar();

    // Apply initial styling
    if (m_themeManager) {
        applyNavbarStyling();
        connect(m_themeManager, &QtThemeManager::themeChanged, this,
                &CriptoGualetQt::onThemeChanged);
    }
}

bool CriptoGualetQt::initializeRepositories() {
    try {
        auto& dbManager = Database::DatabaseManager::getInstance();

        // Derive secure machine-specific encryption key
        std::string encryptionKey;
        if (!Auth::DeriveSecureEncryptionKey(encryptionKey)) {
            qCritical() << "Failed to derive encryption key for database";
            QTimer::singleShot(0, [this]() {
                QMessageBox::critical(this, "Security Error",
                                      "Failed to derive secure encryption key. Cannot "
                                      "initialize database.");
            });
            return false;
        }

        // Get database path from environment or use a secure location in Local AppData
        std::string dbPath;
        
#ifdef Q_OS_WIN
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData) {
            std::string appDir = std::string(localAppData) + "\\CriptoGualet";
            
            // Ensure directory exists using Windows API
            DWORD dwAttrib = GetFileAttributesA(appDir.c_str());
            if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                CreateDirectoryA(appDir.c_str(), NULL);
            }
            
            dbPath = appDir + "\\criptogualet.db";
        } else {
            dbPath = "criptogualet.db"; // Fallback
        }
#else
        dbPath = "criptogualet.db";
#endif

        auto dbResult = dbManager.initialize(dbPath, encryptionKey);

        // Securely wipe the key from memory after use
        std::fill(encryptionKey.begin(), encryptionKey.end(), '\0');

        if (!dbResult) {
            qCritical() << "Database initialization failed:" << dbResult.message.c_str()
                        << "Error code:" << dbResult.errorCode;
            QString errorMsg = QString("Failed to initialize database: %1")
                                   .arg(QString::fromStdString(dbResult.message));
            QTimer::singleShot(0, this, [this, errorMsg]() {
                QMessageBox::critical(this, "Database Error", errorMsg);
            });
            return false;
        }

        m_userRepository = std::make_unique<Repository::UserRepository>(dbManager);
        m_walletRepository = std::make_unique<Repository::WalletRepository>(dbManager);
        m_tokenRepository = std::make_unique<Repository::TokenRepository>(dbManager);
        m_settingsRepository = std::make_unique<Repository::SettingsRepository>(dbManager);

        return true;
    } catch (const std::exception& e) {
        qCritical() << "Exception during database initialization:" << e.what();
        QString errorMsg = QString("Failed to initialize database: %1").arg(e.what());
        QTimer::singleShot(0, this, [this, errorMsg]() {
            QMessageBox::critical(this, "Initialization Error", errorMsg);
        });
        return false;
    }
}

bool CriptoGualetQt::validateUserSession() const {
    std::lock_guard<std::mutex> lock(g_usersMutex);
    if (g_currentUser.empty()) {
        qWarning() << "No active user session";
        return false;
    }
    if (g_users.find(g_currentUser) == g_users.end()) {
        qWarning() << "User not found in global users map";
        return false;
    }
    return true;
}

void CriptoGualetQt::deriveMultiChainAddresses(int userId, const QString& password) {
    if (!m_walletRepository || userId <= 0 || password.isEmpty()) {
        qWarning() << "Cannot derive addresses: invalid parameters";
        return;
    }

    auto seedResult = m_walletRepository->retrieveDecryptedSeed(userId, password.toStdString());
    if (!seedResult.success || seedResult.data.empty()) {
        qWarning() << "Failed to retrieve seed for address derivation";
        return;
    }

    std::array<uint8_t, 64> seed{};
    if (!Crypto::BIP39_SeedFromMnemonic(seedResult.data, "", seed)) {
        qWarning() << "Failed to convert mnemonic to seed";
        return;
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        qWarning() << "Failed to derive master key";
        seed.fill(uint8_t(0));
        return;
    }

    // Derive Ethereum address
    std::string ethAddress;
    if (Crypto::BIP44_GetEthereumAddress(masterKey, 0, false, 0, ethAddress)) {
        m_walletUI->setEthereumAddress(QString::fromStdString(ethAddress));
    }

    // Derive Litecoin address
    std::string ltcAddress;
    if (Crypto::DeriveChainAddress(masterKey, Crypto::ChainType::LITECOIN, 0, false, 0,
                                   ltcAddress)) {
        m_walletUI->setLitecoinAddress(QString::fromStdString(ltcAddress));
    }

    seed.fill(uint8_t(0));  // Securely wipe
}

void CriptoGualetQt::setupUI() {
    // Create central widget with main layout
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create horizontal layout container for sidebar + content
    QWidget* horizontalContainer = new QWidget(m_centralWidget);
    m_contentLayout = new QHBoxLayout(horizontalContainer);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    // Create sidebar (part of horizontal layout, not overlay)
    m_sidebar = new QtSidebar(m_themeManager, horizontalContainer);
    m_sidebar->hide();  // Initially hidden (login screen)
    m_contentLayout->addWidget(m_sidebar);

    // Create content widget that will hold the stacked widget
    m_contentWidget = new QWidget(horizontalContainer);
    m_contentWidget->setObjectName("contentWidget");

    QVBoxLayout* contentInnerLayout = new QVBoxLayout(m_contentWidget);
    contentInnerLayout->setContentsMargins(0, 0, 0, 0);
    contentInnerLayout->setSpacing(0);

    // Create stacked widget for pages
    m_stackedWidget = new QStackedWidget(m_contentWidget);

    m_loginUI = new QtLoginUI(this);
    m_walletUI = new QtWalletUI(this);
    m_settingsUI = new QtSettingsUI(this);
    m_topCryptosPage = new QtTopCryptosPage(this);

    if (m_settingsUI && m_userRepository && m_walletRepository && m_settingsRepository) {
        m_settingsUI->setRepositories(m_userRepository.get(), m_walletRepository.get(),
                                      m_settingsRepository.get());
        connect(
            m_settingsUI, &QtSettingsUI::bitcoinProviderSettingsChanged, this,
            [this](const QString& providerType, const QString& rpcUrl, const QString& rpcUsername,
                   const QString& rpcPassword, bool allowInsecureHttp, bool enableFallback) {
                applyBitcoinProviderSettingsFromValues(providerType, rpcUrl, rpcUsername,
                                                       rpcPassword, allowInsecureHttp,
                                                       enableFallback);
            });
    }

    // Pass wallet instances and repositories to wallet UI
    m_walletUI->setWallet(m_wallet.get());
    m_walletUI->setEthereumWallet(m_ethereumWallet.get());  // PHASE 1 FIX
    m_walletUI->setLitecoinWallet(m_litecoinWallet.get());
    if (m_userRepository && m_walletRepository) {
        m_walletUI->setRepositories(m_userRepository.get(), m_walletRepository.get());
    }
    if (m_tokenRepository) {
        m_walletUI->setTokenRepository(m_tokenRepository.get());
    }

    m_stackedWidget->addWidget(m_loginUI);
    m_stackedWidget->addWidget(m_walletUI);
    m_stackedWidget->addWidget(m_settingsUI);
    m_stackedWidget->addWidget(m_topCryptosPage);

    contentInnerLayout->addWidget(m_stackedWidget);

    // Add content widget to horizontal layout (takes remaining space)
    m_contentLayout->addWidget(m_contentWidget, 1);

    m_mainLayout->addWidget(horizontalContainer);

    // Connect sidebar navigation and width change signals
    createSidebar();
    connect(m_sidebar, &QtSidebar::sidebarWidthChanged, this,
            &CriptoGualetQt::onSidebarWidthChanged);

    connect(m_loginUI, &QtLoginUI::loginRequested,
            [this](const QString& username, const QString& password) {
                // Check for mock user first (synchronous - no thread needed)
                if (m_walletUI->authenticateMockUser(username, password)) {
                    {
                        std::lock_guard<std::mutex> lock(g_usersMutex);
                        g_currentUser = username.toStdString();
                    }
                    showWalletScreen();
                    statusBar()->showMessage("Mock login successful (testuser)", 3000);
                    m_loginUI->onLoginResult(true,
                                             "Mock login successful - testuser authenticated");
                    return;
                }

                // Run authentication in thread pool to keep UI responsive
                class AuthTask : public QRunnable {
                  public:
                    AuthTask(CriptoGualetQt* app, const QString& user, const QString& pass)
                        : m_app(app), m_username(user), m_password(pass) {
                        setAutoDelete(false);
                    }

                    void run() override {
                        std::string stdUsername = m_username.toStdString();
                        std::string stdPassword = m_password.toStdString();
                        Auth::AuthResponse response = Auth::AuthManager::getInstance().LoginUser(stdUsername, stdPassword);

                        // Report back to UI thread
                        QMetaObject::invokeMethod(
                            m_app,
                            [this, response]() {
                                if (!m_app)
                                    return;
                                handleAuthResponse(response);
                            },
                            Qt::QueuedConnection);
                    }

                  private:
                    void handleAuthResponse(const Auth::AuthResponse& response) {
                        QString message = QString::fromStdString(response.message);

                        if (response.success()) {
                            {
                                std::lock_guard<std::mutex> lock(g_usersMutex);
                                g_currentUser = m_username.toStdString();
                            }
                            m_app->handleAuthenticationResult(m_username, m_password, response);
                        } else {
                            m_app->statusBar()->showMessage("Login failed", 3000);
                            m_app->m_loginUI->onLoginResult(false, message);
                        }
                        delete this;
                    }

                    QPointer<CriptoGualetQt> m_app;
                    QString m_username;
                    QString m_password;
                };

                if (m_threadPool) {
                    m_threadPool->start(new AuthTask(this, username, password));
                } else {
                    // Fallback to synchronous execution if thread pool unavailable
                    std::string stdUsername = username.toStdString();
                    std::string stdPassword = password.toStdString();
                    Auth::AuthResponse response = Auth::AuthManager::getInstance().LoginUser(stdUsername, stdPassword);

                    if (response.success()) {
                        {
                            std::lock_guard<std::mutex> lock(g_usersMutex);
                            g_currentUser = stdUsername;
                        }
                        handleAuthenticationResult(username, password, response);
                    } else {
                        statusBar()->showMessage("Login failed", 3000);
                        m_loginUI->onLoginResult(false, QString::fromStdString(response.message));
                    }
                }
            });

    connect(
        m_loginUI, &QtLoginUI::registerRequested,
        [this](const QString& username, const QString& password) {
            std::string stdUsername = username.toStdString();
            std::string stdPassword = password.toStdString();

            // SECURITY: Use QThreadPool instead of detached std::thread
            // to ensure proper cleanup on application exit
            class RegisterTask : public QRunnable {
              public:
                RegisterTask(CriptoGualetQt* app, const QString& user, const std::string& stdUser,
                             const std::string& stdPass)
                    : m_app(app), m_username(user), m_stdUsername(stdUser), m_stdPassword(stdPass) {
                    setAutoDelete(true);
                }

                void run() override {
                    std::vector<std::string> mnemonic;
                    Auth::AuthResponse response =
                        Auth::RegisterUserWithMnemonic(m_stdUsername, m_stdPassword, mnemonic);
                    QString message = QString::fromStdString(response.message);

                    // Report back to UI thread
                    QMetaObject::invokeMethod(
                        m_app,
                        [app = m_app, response, message, mnemonic, username = m_username]() {
                            if (!app)
                                return;
                            if (response.success() && !mnemonic.empty()) {
                                QtSeedDisplayDialog seedDialog(mnemonic, app);
                                int result = seedDialog.exec();

                                if (result == QDialog::Accepted &&
                                    seedDialog.userConfirmedBackup()) {
                                    app->statusBar()->showMessage(
                                        "Registration and backup completed", 3000);
                                    QtSuccessDialog successDialog(username, app);
                                    successDialog.exec();
                                    app->m_loginUI->onRegisterResult(
                                        true, "Account created and seed phrase backed up!");
                                } else {
                                    app->statusBar()->showMessage(
                                        "Registration completed - backup required", 5000);
                                    QMessageBox::warning(
                                        app, "Backup Required",
                                        "Your account has been created successfully, but you "
                                        "must backup your seed phrase!\n\n"
                                        "WARNING: Without a backup of your seed phrase, you "
                                        "may lose access to your wallet permanently.\n\n"
                                        "Please use the 'Reveal Seed' button after signing in "
                                        "to backup your seed phrase.");
                                    app->m_loginUI->onRegisterResult(
                                        true,
                                        "Account created - please backup your "
                                        "seed phrase using 'Reveal Seed'");
                                }
                            } else if (response.success()) {
                                app->statusBar()->showMessage("Registration successful", 3000);
                                QMessageBox::information(
                                    app, "Registration Successful",
                                    QString("Account created for %1!\n\nNote: Seed phrase "
                                            "generation had issues. Please use 'Reveal Seed' "
                                            "after signing in.")
                                        .arg(username));
                                app->m_loginUI->onRegisterResult(response.success(), message);
                            } else {
                                app->statusBar()->showMessage("Registration failed", 3000);
                                app->m_loginUI->onRegisterResult(response.success(), message);
                            }
                        },
                        Qt::QueuedConnection);
                }

              private:
                QPointer<CriptoGualetQt> m_app;
                QString m_username;
                std::string m_stdUsername;
                std::string m_stdPassword;
            };

            if (m_threadPool) {
                m_threadPool->start(new RegisterTask(this, username, stdUsername, stdPassword));
            }
        });

    connect(m_loginUI, &QtLoginUI::totpVerificationRequired,
            [this](const QString& username, const QString& password, const QString& totpCode) {
                std::string stdUsername = username.toStdString();
                std::string stdPassword = password.toStdString();
                std::string stdTotpCode = totpCode.toStdString();

                Auth::AuthResponse response = Auth::AuthManager::getInstance().VerifyTwoFactorCode(stdUsername, stdTotpCode);
                QString message = QString::fromStdString(response.message);

                if (response.success()) {
                    {
                        std::lock_guard<std::mutex> lock(g_usersMutex);
                        g_currentUser = stdUsername;
                    }

                    // Get user ID from repository
                    if (m_userRepository) {
                        auto userResult = m_userRepository->getUserByUsername(stdUsername);
                        if (userResult.success) {
                            int userId = userResult.data.id;
                            m_walletUI->setCurrentUserId(userId);
                            if (m_settingsUI) {
                                m_settingsUI->setCurrentUserId(userId);
                            }
                            applyBitcoinProviderSettings(userId);

                            // PHASE 1 FIX: Derive Ethereum address from seed
                            if (m_walletRepository) {
                                auto seedResult =
                                    m_walletRepository->retrieveDecryptedSeed(userId, stdPassword);
                                if (seedResult.success && !seedResult.data.empty()) {
                                    // Convert mnemonic to seed
                                    std::array<uint8_t, 64> seed{};
                                    if (Crypto::BIP39_SeedFromMnemonic(seedResult.data, "", seed)) {
                                        // Derive master key
                                        Crypto::BIP32ExtendedKey masterKey;
                                        if (Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
                                            // Derive Ethereum address
                                            std::string ethAddress;
                                            if (Crypto::BIP44_GetEthereumAddress(
                                                    masterKey, 0, false, 0, ethAddress)) {
                                                m_walletUI->setEthereumAddress(
                                                    QString::fromStdString(ethAddress));
                                            }

                                            // Derive Litecoin address
                                            std::string ltcAddress;
                                            if (Crypto::DeriveChainAddress(
                                                    masterKey, Crypto::ChainType::LITECOIN, 0,
                                                    false, 0, ltcAddress)) {
                                                m_walletUI->setLitecoinAddress(
                                                    QString::fromStdString(ltcAddress));
                                            }
                                        }
                                        seed.fill(uint8_t(0));  // Securely wipe
                                    }
                                }
                            }
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(g_usersMutex);
                        m_walletUI->setUserInfo(
                            username, QString::fromStdString(g_users[stdUsername].walletAddress));
                    }
                    showWalletScreen(QString::fromStdString(response.sessionId));
                    statusBar()->showMessage("Login successful with 2FA", 3000);
                } else {
                    statusBar()->showMessage("TOTP verification failed", 3000);
                }

                // Send result back to login UI for visual feedback
                m_loginUI->onLoginResult(response.success(), message);
            });

    connect(m_walletUI, &QtWalletUI::viewBalanceRequested, [this]() {
        std::string walletAddress;
        {
            std::lock_guard<std::mutex> lock(g_usersMutex);
            if (!m_wallet || g_currentUser.empty() ||
                g_users.find(g_currentUser) == g_users.end()) {
                QMessageBox::warning(this, "Error", "Wallet not initialized or user not logged in");
                return;
            }
            walletAddress = g_users[g_currentUser].walletAddress;
        }

        // Get balance information
        WalletAPI::ReceiveInfo info = m_wallet->GetAddressInfo(walletAddress);

        // Convert satoshis to BTC for display
        double balanceBTC = m_wallet->ConvertSatoshisToBTC(info.confirmed_balance);
        double unconfirmedBTC = m_wallet->ConvertSatoshisToBTC(info.unconfirmed_balance);

        QString balanceText = QString(
                                  "Confirmed Balance: %1 BTC\n"
                                  "Unconfirmed Balance: %2 BTC\n"
                                  "Total Transactions: %3\n"
                                  "Address: %4")
                                  .arg(balanceBTC, 0, 'f', 8)
                                  .arg(unconfirmedBTC, 0, 'f', 8)
                                  .arg(info.transaction_count)
                                  .arg(QString::fromStdString(info.address));

        QMessageBox::information(this, "Wallet Balance", balanceText);
    });

    connect(m_walletUI, &QtWalletUI::sendBitcoinRequested, [this]() {
        // Logging or background status updates can go here
        statusBar()->showMessage("Bitcoin transaction requested", 3000);
    });

    connect(m_walletUI, &QtWalletUI::receiveBitcoinRequested, [this]() {
        // Logging or background status updates can go here
        statusBar()->showMessage("Bitcoin receive dialog opened", 3000);
    });

    // PHASE 2: Connect Ethereum receive handler
    connect(m_walletUI, &QtWalletUI::receiveEthereumRequested, [this]() {
        std::string currentUsername;
        std::string passwordHash;
        {
            std::lock_guard<std::mutex> lock(g_usersMutex);
            if (!m_ethereumWallet || g_currentUser.empty() ||
                g_users.find(g_currentUser) == g_users.end()) {
                QMessageBox::warning(this, "Error",
                                     "Ethereum wallet not initialized or User not logged in");
                return;
            }
            currentUsername = g_currentUser;
            passwordHash = g_users[g_currentUser].passwordHash;
        }

        // Get Ethereum address (with EIP-55 checksum)
        QString ethAddress;
        if (m_walletRepository && m_userRepository) {
            // Get proper user from repository to access user ID
            auto userResult = m_userRepository->getUserByUsername(currentUsername);
            if (!userResult.hasValue()) {
                QMessageBox::warning(this, "Error", "Failed to retrieve user information");
                return;
            }

            // Note: Using passwordHash as password - this may need proper password
            // handling
            auto seedResult =
                m_walletRepository->retrieveDecryptedSeed(userResult->id, passwordHash);
            if (seedResult.success && !seedResult.data.empty()) {
                std::array<uint8_t, 64> seed{};
                if (Crypto::BIP39_SeedFromMnemonic(seedResult.data, "", seed)) {
                    Crypto::BIP32ExtendedKey masterKey;
                    if (Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
                        std::string ethAddr;
                        if (Crypto::BIP44_GetEthereumAddress(masterKey, 0, false, 0, ethAddr)) {
                            ethAddress = QString::fromStdString(ethAddr);
                        }
                    }
                    seed.fill(uint8_t(0));  // Securely wipe
                }
            }
        }

        if (ethAddress.isEmpty()) {
            QMessageBox::warning(this, "Error", "Failed to retrieve Ethereum address");
            return;
        }

        QString receiveText = QString(
                                  "Your Ethereum Address:\n%1\n\n"
                                  "Share this address to receive Ethereum payments.\n\n"
                                  "Note: This address is in EIP-55 checksum format for extra "
                                  "safety.\n"
                                  "You can use this address on Ethereum mainnet.")
                                  .arg(ethAddress);

        // Copy address to clipboard
        QApplication::clipboard()->setText(ethAddress);
        receiveText += "\n\nAddress copied to clipboard!";

        QMessageBox::information(this, "Receive Ethereum", receiveText);
    });

    // Connect top cryptos page back button
    connect(m_topCryptosPage, &QtTopCryptosPage::backRequested, this, [this]() {
        showWalletScreen("");
    });
}

void CriptoGualetQt::createSidebar() {
    // Sidebar is now created in setupUI() since it needs to be a child of
    // horizontalContainer
    // Connect navigation signals
    connect(m_sidebar, &QtSidebar::navigateToWallet, this, [this]() {
        showWalletScreen("");
    });
    connect(m_sidebar, &QtSidebar::navigateToTopCryptos, this, &CriptoGualetQt::showTopCryptosPage);
    connect(m_sidebar, &QtSidebar::navigateToSettings, this, &CriptoGualetQt::showSettingsScreen);

    // Connect sign out requested signal
    connect(m_sidebar, &QtSidebar::signOutRequested, this, [this]() {
        showLoginScreen();
    });
}

void CriptoGualetQt::handleAuthenticationResult(const QString& username, const QString& password,
                                                const Auth::AuthResponse& response) {
    // Get user ID from repository
    if (!m_userRepository) {
        qWarning() << "User repository not available";
        return;
    }

    std::string stdUsername = username.toStdString();
    auto userResult = m_userRepository->getUserByUsername(stdUsername);

    if (!userResult.success) {
        qWarning() << "Failed to get user by username:" << username;
        m_loginUI->onLoginResult(false, "User not found in database");
        return;
    }

    int userId = userResult.data.id;
    m_walletUI->setCurrentUserId(userId);

    if (m_settingsUI) {
        m_settingsUI->setCurrentUserId(userId);
    }

    applyBitcoinProviderSettings(userId);

    // Derive multi-chain addresses
    deriveMultiChainAddresses(userId, password);
    {
        std::lock_guard<std::mutex> lock(g_usersMutex);
        m_walletUI->setUserInfo(username,
                                QString::fromStdString(g_users[stdUsername].walletAddress));
    }
    showWalletScreen(QString::fromStdString(response.sessionId));
    statusBar()->showMessage("Login successful", 3000);

    QString message = QString::fromStdString(response.message);
    m_loginUI->onLoginResult(true, message);
}

void CriptoGualetQt::setupMenuBar() {
    QMenu* themeMenu = menuBar()->addMenu("&Theme");

    QAction* darkAction = themeMenu->addAction("Dark - Blue");
    QAction* lightAction = themeMenu->addAction("Light - Blue");
    QAction* cryptoDarkAction = themeMenu->addAction("Dark - Purple");
    QAction* cryptoLightAction = themeMenu->addAction("Light - Purple");

    connect(darkAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::DARK);
    });

    connect(lightAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::LIGHT);
    });

    connect(cryptoDarkAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::CRYPTO_DARK);
    });

    connect(cryptoLightAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::CRYPTO_LIGHT);
    });

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* diagnosticsAction = helpMenu->addAction("System &Diagnostics");
    QAction* aboutAction = helpMenu->addAction("&About");

    connect(diagnosticsAction, &QAction::triggered, this, &CriptoGualetQt::showDiagnosticsConsole);

    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About CriptoGualet",
                           "CriptoGualet v1.0\n\nA secure Bitcoin wallet application built with "
                           "Qt.\n\n"
                           "Features:\n• Modern Qt UI with theming\n• Secure authentication\n• "
                           "Bitcoin address generation\n• Secure wallet functionality");
    });
}

void CriptoGualetQt::showDiagnosticsConsole() {
    QtTestConsole* console = new QtTestConsole(this);
    console->setAttribute(Qt::WA_DeleteOnClose);
    console->show();
    console->raise();
    console->activateWindow();
}

void CriptoGualetQt::setupStatusBar() {
    statusBar()->showMessage("Ready");
}

void CriptoGualetQt::showLoginScreen() {
    // SECURITY: Clear all cached user data on logout to prevent memory exposure
    {
        std::lock_guard<std::mutex> lock(g_usersMutex);
        // Securely wipe all cached credential fields before clearing
        for (auto& [key, user] : g_users) {
            std::fill(user.passwordHash.begin(), user.passwordHash.end(), '\0');
            user.passwordHash.clear();
            std::fill(user.walletAddress.begin(), user.walletAddress.end(), '\0');
            user.walletAddress.clear();
            std::fill(user.username.begin(), user.username.end(), '\0');
            user.username.clear();
        }
        g_users.clear();
        g_currentUser.clear();
    }

    if (m_stackedWidget && m_loginUI) {
        m_stackedWidget->setCurrentWidget(m_loginUI);
        m_loginUI->clearLoginFields();
    }
    if (m_settingsUI) {
        m_settingsUI->setCurrentUserId(-1);
    }
    updateSidebarVisibility();
    if (statusBar()) {
        statusBar()->showMessage("Please log in or create an account");
    }
}

void CriptoGualetQt::showWalletScreen(const QString& sessionId) {
    if (m_walletUI && !sessionId.isEmpty()) {
        m_walletUI->setSessionId(sessionId);
    }
    if (m_stackedWidget && m_walletUI) {
        m_stackedWidget->setCurrentWidget(m_walletUI);
    }
    updateSidebarVisibility();
    if (statusBar()) {
        std::string currentUser;
        {
            std::lock_guard<std::mutex> lock(g_usersMutex);
            currentUser = g_currentUser;
        }
        statusBar()->showMessage(
            QString("Logged in as: %1").arg(QString::fromStdString(currentUser)));
    }
}

void CriptoGualetQt::showSettingsScreen() {
    if (m_stackedWidget && m_settingsUI) {
        m_stackedWidget->setCurrentWidget(m_settingsUI);
    }
    updateSidebarVisibility();
    if (statusBar()) {
        statusBar()->showMessage("Settings");
    }
    // Refresh 2FA status when settings page is shown
    if (m_settingsUI) {
        m_settingsUI->refresh2FAStatus();
    }
    // Re-apply sidebar theme to prevent style bleeding from settings page
    if (m_sidebar) {
        m_sidebar->applyTheme();
    }
}

void CriptoGualetQt::showTopCryptosPage() {
    if (m_stackedWidget && m_topCryptosPage) {
        m_stackedWidget->setCurrentWidget(m_topCryptosPage);
        m_topCryptosPage->refreshData();
    }
    updateSidebarVisibility();
    if (statusBar()) {
        statusBar()->showMessage("Top Cryptocurrencies by Market Cap");
    }
}

void CriptoGualetQt::updateSidebarVisibility() {
    // Show sidebar only when not on login screen
    if (m_stackedWidget && m_sidebar && m_loginUI) {
        if (m_stackedWidget->currentWidget() != m_loginUI) {
            m_sidebar->show();
        } else {
            m_sidebar->hide();
        }
    }
}

void CriptoGualetQt::onThemeChanged() {
    // Apply theme to all UI pages with null checks
    if (m_loginUI)
        m_loginUI->applyTheme();
    if (m_walletUI)
        m_walletUI->applyTheme();
    if (m_settingsUI)
        m_settingsUI->applyTheme();
    if (m_topCryptosPage)
        m_topCryptosPage->applyTheme();

    // Apply theme to sidebar
    if (m_sidebar) {
        m_sidebar->applyTheme();
    }

    // Apply navbar styling
    applyNavbarStyling();

    // Force visual refresh of the entire UI
    if (m_centralWidget) {
        m_centralWidget->update();
    }
    update();
}

void CriptoGualetQt::applyNavbarStyling() {
    // Apply main window stylesheet (includes navbar styles)
    if (m_themeManager) {
        setStyleSheet(m_themeManager->getMainWindowStyleSheet());
    }

    // Style menu bar with theme-appropriate colors
    if (m_themeManager && menuBar()) {
        QString bgColor = m_themeManager->backgroundColor().name();
        QString textColor = m_themeManager->textColor().name();
        QString accentColor = m_themeManager->accentColor().name();
        QString accentColorDarker = m_themeManager->accentColor().darker(110).name();
        QString surfaceColor = m_themeManager->surfaceColor().name();
        QString secondaryColor = m_themeManager->secondaryColor().name();

        // Fallback to a distinct color if any color name is empty
        if (bgColor.isEmpty()) {
            qWarning() << "backgroundColor is empty!";
            bgColor = "#FF00FF";
        }
        if (textColor.isEmpty()) {
            qWarning() << "textColor is empty!";
            textColor = "#FF00FF";
        }
        if (accentColor.isEmpty()) {
            qWarning() << "accentColor is empty!";
            accentColor = "#FF00FF";
        }
        if (accentColorDarker.isEmpty()) {
            qWarning() << "accentColorDarker is empty!";
            accentColorDarker = "#FF00FF";
        }
        if (surfaceColor.isEmpty()) {
            qWarning() << "surfaceColor is empty!";
            surfaceColor = "#FF00FF";
        }
        if (secondaryColor.isEmpty()) {
            qWarning() << "secondaryColor is empty!";
            secondaryColor = "#FF00FF";
        }

        QString menuBarStyle = QString(R"(
            QMenuBar {
                background-color: %1;
                color: %2;
                border: none;
                padding: 2px;
            }
            QMenuBar::item {
                background-color: transparent;
                color: %2;
                padding: 4px 10px;
                border-radius: 4px;
            }
            QMenuBar::item:selected {
                background-color: %3;
            }
            QMenuBar::item:pressed {
                background-color: %4;
            }
            QMenu {
                background-color: %5;
                color: %2;
                border: 1px solid %6;
                border-radius: 4px;
                padding: 4px;
            }
            QMenu::item {
                padding: 6px 20px;
                border-radius: 4px;
            }
            QMenu::item:selected {
                background-color: %3;
            }
        )")
                                   .arg(bgColor)
                                   .arg(textColor)
                                   .arg(accentColor)
                                   .arg(accentColorDarker)
                                   .arg(surfaceColor)
                                   .arg(secondaryColor);

        menuBar()->setStyleSheet(menuBarStyle);
    }
}

void CriptoGualetQt::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    // Sidebar is now part of layout, so no manual geometry update needed
}

void CriptoGualetQt::onSidebarWidthChanged(int width) {
    Q_UNUSED(width);
    // The sidebar is part of the horizontal layout, so the layout
    // automatically adjusts the content widget when sidebar width changes.
    // Force a layout update to ensure smooth transition.
    if (m_contentLayout) {
        m_contentLayout->update();
    }
}

void CriptoGualetQt::applyBitcoinProviderSettings(int userId) {
    if (!m_settingsRepository || !m_wallet || userId <= 0) {
        return;
    }

    std::vector<std::string> keys = {kProviderTypeKey, kRpcUrlKey,           kRpcUsernameKey,
                                     kRpcPasswordKey,  kRpcAllowInsecureKey, kProviderFallbackKey};
    auto settingsResult = m_settingsRepository->getUserSettings(userId, keys);
    std::map<std::string, std::string> settings;
    if (settingsResult.success) {
        settings = settingsResult.data;
    }

    std::string providerType = "blockcypher";
    auto providerIt = settings.find(kProviderTypeKey);
    if (providerIt != settings.end()) {
        providerType = providerIt->second;
    }

    auto urlIt = settings.find(kRpcUrlKey);
    auto userIt = settings.find(kRpcUsernameKey);
    auto passIt = settings.find(kRpcPasswordKey);
    auto allowIt = settings.find(kRpcAllowInsecureKey);
    auto fallbackIt = settings.find(kProviderFallbackKey);

    WalletAPI::BitcoinProviderConfig config;
    if (providerType == "rpc") {
        config.type = WalletAPI::BitcoinProviderType::BitcoinRpc;
    } else {
        config.type = WalletAPI::BitcoinProviderType::BlockCypher;
    }
    if (urlIt != settings.end()) {
        config.rpcUrl = urlIt->second;
    }
    if (userIt != settings.end()) {
        config.rpcUsername = userIt->second;
    }
    if (passIt != settings.end()) {
        config.rpcPassword = passIt->second;
    }
    // SECURITY: Default to false -- require explicit opt-in for insecure HTTP
    config.allowInsecureHttp =
        allowIt != settings.end() && (allowIt->second == "true" || allowIt->second == "1");
    config.enableFallback =
        fallbackIt == settings.end() || fallbackIt->second == "true" || fallbackIt->second == "1";

    m_wallet->ApplyProviderConfig(config);
}

void CriptoGualetQt::applyBitcoinProviderSettingsFromValues(
    const QString& providerType, const QString& rpcUrl, const QString& rpcUsername,
    const QString& rpcPassword, bool allowInsecureHttp, bool enableFallback) {
    if (!m_wallet) {
        return;
    }

    WalletAPI::BitcoinProviderConfig config;
    if (providerType == "rpc") {
        config.type = WalletAPI::BitcoinProviderType::BitcoinRpc;
    } else {
        config.type = WalletAPI::BitcoinProviderType::BlockCypher;
    }
    config.rpcUrl = rpcUrl.toStdString();
    config.rpcUsername = rpcUsername.toStdString();
    config.rpcPassword = rpcPassword.toStdString();
    config.allowInsecureHttp = allowInsecureHttp;
    config.enableFallback = enableFallback;

    m_wallet->ApplyProviderConfig(config);
}

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // Set the AppUserModelID to ensure the taskbar icon is displayed correctly on Windows.
    // This must be done before any windows are created.
    const wchar_t* appId = L"ErickHerrera.CriptoGualet.1.0";
    typedef HRESULT (WINAPI *SetAppIDFunc)(PCWSTR);
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    if (hShell32) {
        SetAppIDFunc setAppID = (SetAppIDFunc)GetProcAddress(hShell32, "SetCurrentProcessExplicitAppUserModelID");
        if (setAppID) {
            setAppID(appId);
        }
    }
#endif

    QApplication app(argc, argv);

    app.setApplicationName("CriptoGualet");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("CriptoGualet");
    app.setWindowIcon(QIcon(":/icons/icons/logo_criptogualet.ico"));

    qDebug() << "Creating main window...";
    CriptoGualetQt window;

    qDebug() << "Showing window...";
    window.show();
    window.raise();
    window.activateWindow();

    qDebug() << "Window should be visible now. Starting event loop...";
    return app.exec();
}
