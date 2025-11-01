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
#include "PriceService.h"
#include "WalletAPI.h"

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
  void fetchRealBalance();

  // Mock user system
  bool authenticateMockUser(const QString &username, const QString &password);
  void setMockUser(const QString &username);
  bool isMockMode() const { return m_mockMode; }
  MockUserData *getCurrentMockUser() { return m_currentMockUser; }

signals:
  void viewBalanceRequested();
  void sendBitcoinRequested();
  void receiveBitcoinRequested();
  void logoutRequested();

private slots:
  void onViewBalanceClicked();
  void onSendBitcoinClicked();
  void onReceiveBitcoinClicked();
  void onLogoutClicked();
  void onThemeChanged();
  void onToggleBalanceClicked();
  void onPriceUpdateTimer();
  void onBalanceUpdateTimer();

protected:
  void resizeEvent(QResizeEvent *event) override;

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

  // Reusable wallet cards
  class QtExpandableWalletCard *m_bitcoinWalletCard;

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
  QTimer *m_balanceUpdateTimer;
  double m_realBalanceBTC;

  // Price service
  std::unique_ptr<PriceService::PriceFetcher> m_priceFetcher;
  QTimer *m_priceUpdateTimer;
  double m_currentBTCPrice;
  void updateUSDBalance();
  void fetchBTCPrice();

  // Flags
  bool m_balanceVisible;
  bool m_mockMode;
};
