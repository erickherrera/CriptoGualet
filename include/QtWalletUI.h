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
  void createTransactionHistory();
  void updateStyles();
  void updateScrollAreaWidth();
  void updateResponsiveLayout();
  void adjustButtonLayout();
  void updateCardSizes();

  // Theme
  QtThemeManager *m_themeManager;

  // Layout roots
  QVBoxLayout *m_mainLayout;
  QScrollArea *m_scrollArea;
  QWidget *m_scrollContent;
  QVBoxLayout *m_contentLayout;

  // Header section (replaces welcome card)
  QWidget *m_headerSection;
  QLabel *m_headerTitle;
  QLabel *m_balanceLabel;
  QPushButton *m_toggleBalanceButton;
  bool m_balanceVisible;

  // Address section
  QFrame *m_addressCard;
  QLabel *m_addressTitleLabel;
  QLabel *m_addressSectionLabel; // "Address:" label
  QLabel *m_addressLabel;        // actual address text
  QPushButton *m_copyAddressButton;

  // Actions section
  QFrame *m_actionsCard;
  QLabel *m_actionsTitle; // <-- Added: title label for "Wallet Actions"
  QHBoxLayout *m_actionsLayout;
  QPushButton *m_viewBalanceButton;
  QPushButton *m_sendButton;
  QPushButton *m_receiveButton;

  // Transaction history
  QFrame *m_historyCard;
  QLabel *m_historyTitleLabel;
  QTextEdit *m_historyText;

  // User data
  QString m_currentUsername;
  QString m_currentAddress;

  // Mock user system
  void initializeMockUsers();
  void updateMockTransactionHistory();
  QMap<QString, MockUserData> m_mockUsers;
  MockUserData *m_currentMockUser;
  bool m_mockMode;
};
