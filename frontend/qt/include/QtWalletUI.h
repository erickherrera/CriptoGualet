#pragma once

#include "../../../backend/blockchain/include/EthereumService.h"
#include "../../../backend/blockchain/include/PriceService.h"
#include "../../../backend/core/include/Crypto.h"
#include "../../../backend/repository/include/Repository/TokenRepository.h"
#include "../../../backend/core/include/WalletAPI.h"
#include "../../../backend/repository/include/Repository/UserRepository.h"
#include "../../../backend/repository/include/Repository/WalletRepository.h"
#include "QtTokenCard.h"
#include <QTextEdit>
#include <QTimer>
#include <QColor>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class QtThemeManager;
class QResizeEvent;
class QtTokenImportDialog;
class QtTokenListWidget;

// Mock user data structure
struct MockTransaction {
    QString type;
    QString address;
    double amount;
    QString timestamp;
    QString status;
    QString txId;
};

struct MockUserData {
    QString username;
    QString password;
    QString primaryAddress;
    QStringList addresses;
    double balance;
    QList<MockTransaction> transactions;
};

class QtWalletUI : public QWidget {
    Q_OBJECT

  public:
    explicit QtWalletUI(QWidget* parent = nullptr);
    ~QtWalletUI() override = default;

    void setUserInfo(const QString& username, const QString& address);
    void setWallet(WalletAPI::SimpleWallet* wallet);
    void setLitecoinWallet(WalletAPI::LitecoinWallet* ltcWallet);
    void setLitecoinAddress(const QString& address);
    void setEthereumWallet(WalletAPI::EthereumWallet* ethWallet);  // PHASE 1 FIX
    void setEthereumAddress(const QString& address);
    void setRepositories(Repository::UserRepository* userRepo,
                         Repository::WalletRepository* walletRepo);
    void setTokenRepository(Repository::TokenRepository* tokenRepo);
    void setCurrentUserId(int userId);
    void setSessionId(const QString& sessionId);
    bool authenticateMockUser(const QString& username, const QString& password);
    void setMockUser(const QString& username);
    void applyTheme();

  public slots:
    void onThemeChanged();
    void refreshUSDBalance() { updateUSDBalance(); }

  signals:
    void logoutRequested();
    void sendBitcoinRequested();
    void receiveBitcoinRequested();
    void sendLitecoinRequested();
    void receiveLitecoinRequested();
    void receiveEthereumRequested();  // PHASE 2
    void viewBalanceRequested();

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;  // PHASE 3: Accessibility

  private slots:
    void onLogoutClicked();
    void onSendBitcoinClicked();
    void onReceiveBitcoinClicked();
    void onSendLitecoinClicked();
    void onReceiveLitecoinClicked();
    void onReceiveEthereumClicked();  // PHASE 2
    void onSendEthereumClicked();     // PHASE 2
    void onViewBalanceClicked();
    void onToggleBalanceClicked();
    void onRefreshClicked();      // PHASE 3: Manual refresh
    void onPriceUpdateTimer();    // PHASE 2
    void onBalanceUpdateTimer();  // Refresh balances periodically
    void onImportTokenClicked();
    void onPriceFetchFinished();
    void onBalanceFetchFinished();
     
     // Stablecoin handlers
     void onSendUSDTClicked();
     void onReceiveUSDTClicked();
     void onSendUSDCClicked();
     void onReceiveUSDCClicked();
     void onSendDAIClicked();
     void onReceiveDAIClicked();

  private:
    struct PriceFetchResult {
        double btcPrice = 0.0;
        double ltcPrice = 0.0;
        double ethPrice = 0.0;
    };

    struct TokenBalanceUpdate {
        std::string contractAddress;
        std::string balanceText;
        bool success = false;
    };

    struct BalanceFetchResult {
        bool btcSuccess = false;
        bool ltcSuccess = false;
        bool ethSuccess = false;
        double btcBalance = 0.0;
        double ltcBalance = 0.0;
        double ethBalance = 0.0;
        double tokenUsdValue = 0.0;
        std::vector<std::string> btcTransactions;
        std::vector<std::string> ltcTransactions;
        std::vector<EthereumService::Transaction> ethTransactions;
        std::vector<TokenBalanceUpdate> tokenBalances;
        std::string errorMessage;
    };

    void updateStyles();
    QIcon createColoredIcon(const QString& svgPath, const QColor& color);
    void setupUI();
    void createHeaderSection();
    void createActionButtons();
    void updateResponsiveLayout();
    void adjustButtonLayout();
    void updateCardSizes();
    void updateScrollAreaWidth();
    void initializeMockUsers();
    void updateMockTransactionHistory();
    void fetchBTCPrice();
    void fetchLTCPrice();
    void fetchETHPrice();   // PHASE 2
    void fetchAllPrices();  // PHASE 2
    void updateUSDBalance();
    void fetchRealBalance();
    void updateStatusLabel();                                  // PHASE 2
    void setLoadingState(bool loading, const QString& chain);  // PHASE 2
    void setErrorState(const QString& errorMessage);           // PHASE 2
    void clearErrorState();                                    // PHASE 2

    // Token management
    void setupTokenManagement();
    void loadImportedTokens();
    void updateTokenBalances();
    void onTokenImported(const TokenCardData& tokenData);
    void onTokenDeleted(const QString& contractAddress);
    int getEthereumWalletId();

    // Legacy formatting helpers for backward compatibility
    QString formatBitcoinTransactionHistory(const std::vector<std::string>& txHashes);
    QString formatEthereumTransactionHistory(const std::vector<EthereumService::Transaction>& txs,
                                             const std::string& userAddress);

    // Private helper for sending real transactions
    void sendRealTransaction(const QString& recipientAddress, uint64_t amountSatoshis,
                             uint64_t feeSatoshis, const QString& password);

    // Private helper for deriving private key
    std::vector<uint8_t> derivePrivateKeyForAddress(const QString& address,
                                                     const QString& password);
    
    // Private helper for calculating token USD value
    double calculateTotalTokenUSDValue();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_centeringLayout;
    QSpacerItem* m_leftSpacer;
    QSpacerItem* m_rightSpacer;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;
    QWidget* m_headerSection;
    QLabel* m_headerTitle;
    QLabel* m_balanceTitle;
    QLabel* m_balanceLabel;
    QPushButton* m_toggleBalanceButton;
    QPushButton* m_refreshButton;
    class QtExpandableWalletCard* m_bitcoinWalletCard;
    class QtExpandableWalletCard* m_litecoinWalletCard;
    class QtExpandableWalletCard* m_ethereumWalletCard;
    
    // Stablecoin section
    QLabel* m_stablecoinSectionHeader;
    class QtExpandableWalletCard* m_usdtWalletCard;
    class QtExpandableWalletCard* m_usdcWalletCard;
    class QtExpandableWalletCard* m_daiWalletCard;
    
    QPushButton* m_importTokenButton;
    QLabel* m_statusLabel;  // PHASE 2

    // State
    QString m_sessionId;
    QString m_currentUsername;
    QString m_currentAddress;
    QString m_litecoinAddress;
    QString m_ethereumAddress;  // PHASE 1 FIX
    MockUserData* m_currentMockUser;
    QMap<QString, MockUserData> m_mockUsers;
    QtThemeManager* m_themeManager;
    WalletAPI::SimpleWallet* m_wallet;
    WalletAPI::LitecoinWallet* m_litecoinWallet;
    WalletAPI::EthereumWallet* m_ethereumWallet;  // PHASE 1 FIX
    QTimer* m_balanceUpdateTimer;
    double m_realBalanceBTC;
    double m_realBalanceLTC;
    double m_realBalanceETH;
    double m_cachedTokenUsdValue;
    Repository::UserRepository* m_userRepository;
    Repository::WalletRepository* m_walletRepository;
    Repository::TokenRepository* m_tokenRepository;
    int m_currentUserId;

    // Token management
    QtTokenImportDialog* m_tokenImportDialog;
    QtTokenListWidget* m_tokenListWidget;

    // Price Service
    std::unique_ptr<PriceService::PriceFetcher> m_priceFetcher;
    QTimer* m_priceUpdateTimer;
    QFutureWatcher<PriceFetchResult>* m_priceFetchWatcher;
    QFutureWatcher<BalanceFetchResult>* m_balanceFetchWatcher;
    double m_currentBTCPrice;
    double m_currentLTCPrice;
    double m_currentETHPrice;
    bool m_isLoadingBTC;
    bool m_isLoadingLTC;
    bool m_isLoadingETH;
    QString m_lastErrorMessage;

    bool m_balanceVisible;
    bool m_mockMode;
};
