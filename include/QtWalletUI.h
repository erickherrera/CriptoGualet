#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpacerItem>

class QtThemeManager;

class QtWalletUI : public QWidget {
    Q_OBJECT

public:
    explicit QtWalletUI(QWidget *parent = nullptr);
    void setUserInfo(const QString &username, const QString &address);
    void applyTheme();

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

private:
    void setupUI();
    void createWelcomeSection();
    void createAddressSection();
    void createActionButtons();
    void createWalletSection();
    void createBitcoinWalletActions(QVBoxLayout *parentLayout);
    void createTransactionHistory();
    void updateStyles();

    // Theme
    QtThemeManager *m_themeManager;

    // Layout roots
    QVBoxLayout *m_mainLayout;
    QScrollArea *m_scrollArea;
    QWidget *m_scrollContent;
    QVBoxLayout *m_contentLayout;

    // Welcome section
    QFrame *m_welcomeCard;
    QLabel *m_welcomeLabel;
    QLabel *m_balanceLabel;

    // Address section
    QFrame *m_addressCard;
    QLabel *m_addressTitleLabel;
    QLabel *m_addressSectionLabel;  // "Address:" label
    QLabel *m_addressLabel;         // actual address text
    QPushButton *m_copyAddressButton;

    // Actions section
    QFrame *m_actionsCard;
    QLabel *m_actionsTitle;          // <-- Added: title label for "Wallet Actions"
    QGridLayout *m_actionsLayout;
    QPushButton *m_viewBalanceButton;
    QPushButton *m_sendButton;
    QPushButton *m_receiveButton;
    QPushButton *m_logoutButton;

    // Transaction history
    QFrame *m_historyCard;
    QLabel *m_historyTitleLabel;
    QTextEdit *m_historyText;

    // User data
    QString m_currentUsername;
    QString m_currentAddress;
};