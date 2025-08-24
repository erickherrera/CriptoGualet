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
    void onThemeChanged();

private:
    void setupUI();
    void createTitleSection();
    void createWalletSection();
    void createBitcoinWalletActions(QVBoxLayout *parentLayout);
    void createTransactionHistory();
    void updateStyles();
    
    QtThemeManager *m_themeManager;
    
    QVBoxLayout *m_mainLayout;
    QScrollArea *m_scrollArea;
    QWidget *m_scrollContent;
    QVBoxLayout *m_contentLayout;
    
    // Title section (no card, direct on background)
    QLabel *m_welcomeLabel;
    
    // Wallet section (combined address and balance)
    QFrame *m_addressCard;
    QLabel *m_addressTitleLabel;
    QLabel *m_balanceLabel;
    QLabel *m_addressSectionLabel;  // "Address:" label
    QLabel *m_addressLabel;         // actual address text
    QPushButton *m_copyAddressButton;
    
    // Bitcoin wallet action buttons (integrated in wallet section)
    QLabel *m_actionsSectionLabel;  // "Actions:" label
    QGridLayout *m_actionsLayout;
    QPushButton *m_viewBalanceButton;
    QPushButton *m_sendButton;
    QPushButton *m_receiveButton;
    
    // Transaction history
    QFrame *m_historyCard;
    QLabel *m_historyTitleLabel;
    QTextEdit *m_historyText;
    
    QString m_currentUsername;
    QString m_currentAddress;
};