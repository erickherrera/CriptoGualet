#pragma once

#include "../../../backend/core/include/WalletAPI.h"
#include "../../../backend/database/include/Database/DatabaseManager.h"
#include "../../../backend/repository/include/Repository/UserRepository.h"
#include "../../../backend/repository/include/Repository/WalletRepository.h"
#include "../../../backend/repository/include/Repository/SettingsRepository.h"
#include "../../../backend/repository/include/Repository/TokenRepository.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <memory>
#include <QThreadPool>
#include <QRunnable>
#include <QMutex>
#include <QFutureWatcher>
#include <QtConcurrent>

class QtLoginUI;
class QtWalletUI;
class QtSettingsUI;
class QtTopCryptosPage;
class QtThemeManager;
class QtSidebar;

namespace Auth {
    struct AuthResponse;
}

class CriptoGualetQt : public QMainWindow {
    Q_OBJECT

  public:
    explicit CriptoGualetQt(QWidget* parent = nullptr);
    ~CriptoGualetQt() override = default;

  public slots:
    void showLoginScreen();
    void showWalletScreen(const QString& sessionId = "");
    void showSettingsScreen();
    void showTopCryptosPage();
    void onThemeChanged();
    void onSidebarWidthChanged(int width);

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void createSidebar();
    void updateSidebarVisibility();
    void applyNavbarStyling();
    void applyBitcoinProviderSettings(int userId);
    void applyBitcoinProviderSettingsFromValues(const QString& providerType,
                                                const QString& rpcUrl,
                                                const QString& rpcUsername,
                                                const QString& rpcPassword,
                                                bool allowInsecureHttp,
                                                bool enableFallback);
    
    // Helper methods for reliability
    bool validateUserSession() const;
    bool initializeRepositories();
    void deriveMultiChainAddresses(int userId, const QString& password);
    void handleAuthenticationResult(const QString& username, const QString& password, 
                                   const Auth::AuthResponse& response);
    
    // Thread pool for background operations
    QThreadPool* m_threadPool;

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_contentLayout;
    QWidget* m_contentWidget;
    QStackedWidget* m_stackedWidget;
    QtLoginUI* m_loginUI;
    QtWalletUI* m_walletUI;
    QtSettingsUI* m_settingsUI;
    QtTopCryptosPage* m_topCryptosPage;
    QtSidebar* m_sidebar;
    QtThemeManager* m_themeManager;
    std::unique_ptr<WalletAPI::SimpleWallet> m_wallet;
    std::unique_ptr<WalletAPI::EthereumWallet> m_ethereumWallet;  // PHASE 1 FIX
    std::unique_ptr<WalletAPI::LitecoinWallet> m_litecoinWallet;  // PHASE 2 FIX
    std::unique_ptr<Repository::UserRepository> m_userRepository;
    std::unique_ptr<Repository::WalletRepository> m_walletRepository;
    std::unique_ptr<Repository::TokenRepository> m_tokenRepository;
    std::unique_ptr<Repository::SettingsRepository> m_settingsRepository;
};
