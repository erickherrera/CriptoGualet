#pragma once

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <memory>
#include <vector>
#include <string>
#include "PriceService.h"
#include "EthereumService.h"
#include "WalletAPI.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"

// Forward declarations
class QtThemeManager;
class QResizeEvent;

// Mock user data structure
struct MockTransaction {
  QString type;      // "SENT", "RECEIVED", "WAITING"
  QString address;   // recipient/sender address
  double amount;     // BTC amount
  QString timestamp; // transaction time
  QString status;    // "Confirmed", "Pending", "Waiting for payment"
  QString txId;      // transaction ID (mock)
};

struct MockUserData {
  QString username;
  QString password;
  QString primaryAddress;
  QStringList addresses; // multiple addresses for the user
  double balance;        // total balance in BTC
  QList<MockTransaction> transactions;
};

class QtWalletUI : public QWidget {
  Q_OBJECT

public:
  explicit QtWalletUI(QWidget *parent = nullptr);
  void setUserInfo(const QString &username, const QString &address);
  void applyTheme();

  // Real wallet integration
  void setWallet(WalletAPI::SimpleWallet *wallet);
  void setEthereumWallet(WalletAPI::EthereumWallet *ethWallet);  // PHASE 1 FIX
  void setEthereumAddress(const QString &address);  // PHASE 1 FIX
  void fetchRealBalance();
  void setRepositories(Repository::UserRepository* userRepo, Repository::WalletRepository* walletRepo);
  void setCurrentUserId(int userId);

  // Mock user system
  bool authenticateMockUser(const QString &username, const QString &password);
  void setMockUser(const QString &username);
  bool isMockMode() const { return m_mockMode; }
  MockUserData *getCurrentMockUser() { return m_currentMockUser; }

signals:
  void viewBalanceRequested();
  void sendBitcoinRequested();
  void receiveBitcoinRequested();
  void receiveEthereumRequested();  // PHASE 2: Separate Ethereum receive handler
  void logoutRequested();

private slots:
  void onViewBalanceClicked();
  void onSendBitcoinClicked();
  void onSendEthereumClicked();  // Ethereum send handler
  void onReceiveBitcoinClicked();
  void onReceiveEthereumClicked();  // PHASE 2: Separate Ethereum receive handler
  void onLogoutClicked();
  void onThemeChanged();
  void onToggleBalanceClicked();
  void onRefreshClicked();  // PHASE 3: Manual refresh handler
  void onPriceUpdateTimer();
  void onBalanceUpdateTimer();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;  // PHASE 3: Keyboard shortcuts

private:
  void setupUI();
  void createHeaderSection();
  void createAddressSection();
  QIcon createColoredIcon(const QString &svgPath, const QColor &color);
  void createActionButtons();
  void createWalletSection();
  void createBitcoinWalletActions(QVBoxLayout *parentLayout);
  void updateStyles();
  void updateScrollAreaWidth();
  void updateResponsiveLayout();
  void adjustButtonLayout();
  void updateCardSizes();

  // Theme
  QtThemeManager *m_themeManager;

  // Layout roots
  QVBoxLayout *m_mainLayout;
  QHBoxLayout *m_centeringLayout;
  QSpacerItem *m_leftSpacer;
  QSpacerItem *m_rightSpacer;
  QScrollArea *m_scrollArea;
  QWidget *m_scrollContent;
  QVBoxLayout *m_contentLayout;

  // Header section (replaces welcome card)
  QWidget *m_headerSection;
  QLabel *m_headerTitle;
  QLabel *m_balanceTitle;
  QLabel *m_balanceLabel;
  QPushButton *m_toggleBalanceButton;
  QPushButton *m_refreshButton;  // PHASE 3: Manual refresh button

  // Reusable wallet cards
  class QtExpandableWalletCard *m_bitcoinWalletCard;
  class QtExpandableWalletCard *m_ethereumWalletCard;

  // User data
  QString m_currentUsername;
  QString m_currentAddress;

  // Mock user system
  void initializeMockUsers();
  void updateMockTransactionHistory();
  QMap<QString, MockUserData> m_mockUsers;
  MockUserData *m_currentMockUser;

  // Real wallet integration
  WalletAPI::SimpleWallet *m_wallet;
  WalletAPI::EthereumWallet *m_ethereumWallet;
  QTimer *m_balanceUpdateTimer;
  double m_realBalanceBTC;
  double m_realBalanceETH;
  QString m_ethereumAddress;
  Repository::UserRepository* m_userRepository;
  Repository::WalletRepository* m_walletRepository;
  int m_currentUserId;

  // Transaction handling
  void sendRealTransaction(const QString& recipientAddress, uint64_t amountSatoshis,
                          uint64_t feeSatoshis, const QString& password);
  std::vector<uint8_t> derivePrivateKeyForAddress(const QString& address, const QString& password);

  // PHASE 3: Transaction history formatting
  QString formatBitcoinTransactionHistory(const std::vector<std::string>& txHashes);
  QString formatEthereumTransactionHistory(const std::vector<EthereumService::Transaction>& txs, const std::string& userAddress);

  // Price service
  std::unique_ptr<PriceService::PriceFetcher> m_priceFetcher;
  QTimer *m_priceUpdateTimer;
  double m_currentBTCPrice;
  double m_currentETHPrice;  // PHASE 2: ETH price for total balance
  void updateUSDBalance();
  void fetchBTCPrice();
  void fetchETHPrice();  // PHASE 2: Fetch Ethereum price
  void fetchAllPrices();  // PHASE 2: Fetch both BTC and ETH prices

  // PHASE 2: Loading and error state management
  bool m_isLoadingBTC;
  bool m_isLoadingETH;
  QString m_lastErrorMessage;
  QLabel *m_statusLabel;
  void setLoadingState(bool loading, const QString& chain);
  void setErrorState(const QString& errorMessage);
  void clearErrorState();
  void updateStatusLabel();

  // Flags
  bool m_balanceVisible;
  bool m_mockMode;
};
