#include "../include/QtWalletUI.h"
#include "../include/QtThemeManager.h"

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
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

QtWalletUI::QtWalletUI(QWidget *parent)
    : QWidget(parent)
    , m_themeManager(&QtThemeManager::instance())
{
    setupUI();
    applyTheme();
    
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtWalletUI::onThemeChanged);
}

void QtWalletUI::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(20);
    
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setContentsMargins(20, 20, 20, 20);
    m_contentLayout->setSpacing(20);
    
    createTitleSection();
    createWalletSection();
    createTransactionHistory();
    
    m_contentLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);
}

void QtWalletUI::createTitleSection() {
    m_welcomeLabel = new QLabel("Digital Wallets", m_scrollContent);
    m_welcomeLabel->setProperty("class", "title");
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    
    // Add some margin for spacing
    m_welcomeLabel->setContentsMargins(0, 10, 0, 10);
    
    m_contentLayout->addWidget(m_welcomeLabel);
}

void QtWalletUI::createWalletSection() {
    m_addressCard = new QFrame(m_scrollContent);
    m_addressCard->setProperty("class", "card");
    
    QVBoxLayout *walletLayout = new QVBoxLayout(m_addressCard);
    walletLayout->setContentsMargins(25, 25, 25, 25);
    walletLayout->setSpacing(20);
    
    // Bitcoin wallet header - centered
    m_addressTitleLabel = new QLabel("Bitcoin Wallet", m_addressCard);
    m_addressTitleLabel->setProperty("class", "title");
    m_addressTitleLabel->setAlignment(Qt::AlignCenter);
    walletLayout->addWidget(m_addressTitleLabel);
    
    // Balance section - centered
    m_balanceLabel = new QLabel("Balance: 0.00000000 BTC", m_addressCard);
    m_balanceLabel->setProperty("class", "subtitle");
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    walletLayout->addWidget(m_balanceLabel);
    
    // Address section - all in one row with margins
    QHBoxLayout *addressRowLayout = new QHBoxLayout();
    addressRowLayout->setContentsMargins(20, 0, 20, 0);
    
    m_addressSectionLabel = new QLabel("Address:", m_addressCard);
    m_addressSectionLabel->setProperty("class", "section-label");
    m_addressSectionLabel->setMinimumWidth(70);
    addressRowLayout->addWidget(m_addressSectionLabel);
    
    m_addressLabel = new QLabel("", m_addressCard);
    m_addressLabel->setProperty("class", "address");
    m_addressLabel->setWordWrap(false);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addressRowLayout->addWidget(m_addressLabel, 1);
    
    m_copyAddressButton = new QPushButton("Copy", m_addressCard);
    m_copyAddressButton->setMaximumWidth(80);
    addressRowLayout->addWidget(m_copyAddressButton);
    
    walletLayout->addLayout(addressRowLayout);
    
    // Bitcoin wallet actions
    createBitcoinWalletActions(walletLayout);
    
    connect(m_copyAddressButton, &QPushButton::clicked, [this]() {
        if (!m_currentAddress.isEmpty()) {
            QApplication::clipboard()->setText(m_currentAddress);
            QMessageBox::information(this, "Copied", "Address copied to clipboard!");
        }
    });
    
    m_contentLayout->addWidget(m_addressCard);
}

void QtWalletUI::createBitcoinWalletActions(QVBoxLayout *parentLayout) {
    // Actions section for this wallet with margins
    QHBoxLayout *actionsLabelLayout = new QHBoxLayout();
    actionsLabelLayout->setContentsMargins(20, 0, 20, 0);
    
    m_actionsSectionLabel = new QLabel("Actions:", m_addressCard);
    m_actionsSectionLabel->setProperty("class", "section-label");
    actionsLabelLayout->addWidget(m_actionsSectionLabel);
    actionsLabelLayout->addStretch();
    
    parentLayout->addLayout(actionsLabelLayout);
    
    m_actionsLayout = new QGridLayout();
    m_actionsLayout->setSpacing(12);
    m_actionsLayout->setContentsMargins(20, 0, 20, 0);
    
    m_viewBalanceButton = new QPushButton("View Balance", m_addressCard);
    m_viewBalanceButton->setMinimumHeight(45);
    m_actionsLayout->addWidget(m_viewBalanceButton, 0, 0);
    
    m_sendButton = new QPushButton("Send Bitcoin", m_addressCard);
    m_sendButton->setMinimumHeight(45);
    m_actionsLayout->addWidget(m_sendButton, 0, 1);
    
    m_receiveButton = new QPushButton("Receive Bitcoin", m_addressCard);
    m_receiveButton->setMinimumHeight(45);
    m_actionsLayout->addWidget(m_receiveButton, 1, 0);
    
    // Sign Out button moved to navbar - leave space or adjust grid as needed
    // For now, just keep the 3-button layout with proper spacing
    
    parentLayout->addLayout(m_actionsLayout);
    
    connect(m_viewBalanceButton, &QPushButton::clicked, this, &QtWalletUI::onViewBalanceClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &QtWalletUI::onSendBitcoinClicked);
    connect(m_receiveButton, &QPushButton::clicked, this, &QtWalletUI::onReceiveBitcoinClicked);
}


void QtWalletUI::createTransactionHistory() {
    m_historyCard = new QFrame(m_scrollContent);
    m_historyCard->setProperty("class", "card");
    
    QVBoxLayout *historyLayout = new QVBoxLayout(m_historyCard);
    historyLayout->setContentsMargins(25, 25, 25, 25);
    historyLayout->setSpacing(15);
    
    m_historyTitleLabel = new QLabel("Transaction History", m_historyCard);
    m_historyTitleLabel->setProperty("class", "title");
    historyLayout->addWidget(m_historyTitleLabel);
    
    m_historyText = new QTextEdit(m_historyCard);
    m_historyText->setReadOnly(true);
    m_historyText->setMaximumHeight(200);
    m_historyText->setText("No transactions yet.\n\nThis is a demo wallet. In a real implementation, transaction history would be displayed here.");
    historyLayout->addWidget(m_historyText);
    
    m_contentLayout->addWidget(m_historyCard);
}

void QtWalletUI::setUserInfo(const QString &username, const QString &address) {
    m_currentUsername = username;
    m_currentAddress = address;
    
    // Title remains "Digital Wallets" - no need to change it
    m_addressLabel->setText(address);
}

void QtWalletUI::onViewBalanceClicked() {
    emit viewBalanceRequested();
}

void QtWalletUI::onSendBitcoinClicked() {
    emit sendBitcoinRequested();
}

void QtWalletUI::onReceiveBitcoinClicked() {
    emit receiveBitcoinRequested();
}

void QtWalletUI::onThemeChanged() {
    applyTheme();
}

void QtWalletUI::applyTheme() {
    updateStyles();
}

void QtWalletUI::updateStyles() {
    setStyleSheet(m_themeManager->getMainWindowStyleSheet());
    
    QString cardStyle = m_themeManager->getCardStyleSheet();
    m_addressCard->setStyleSheet(cardStyle);
    m_historyCard->setStyleSheet(cardStyle);
    
    // Title with shadow effect - using same base style as card labels
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    QString titleStyle = labelStyle + QString(R"(
        QLabel {
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
            background-color: transparent;
            padding: 15px 0px;
        }
    )");
    m_welcomeLabel->setStyleSheet(titleStyle);
    
    m_balanceLabel->setStyleSheet(labelStyle);
    m_addressTitleLabel->setStyleSheet(labelStyle);
    m_addressSectionLabel->setStyleSheet(labelStyle);  // "Address:" label
    m_addressLabel->setStyleSheet(labelStyle);         // actual address
    m_actionsSectionLabel->setStyleSheet(labelStyle);  // "Actions:" label
    m_historyTitleLabel->setStyleSheet(labelStyle);
    
    QString buttonStyle = m_themeManager->getButtonStyleSheet();
    m_viewBalanceButton->setStyleSheet(buttonStyle);
    m_sendButton->setStyleSheet(buttonStyle);
    m_receiveButton->setStyleSheet(buttonStyle);
    m_copyAddressButton->setStyleSheet(buttonStyle);
    
    QString textEditStyle = QString(R"(
        QTextEdit {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 8px;
            font-family: %4;
            font-size: %5px;
        }
    )").arg(m_themeManager->surfaceColor().name())
       .arg(m_themeManager->textColor().name())
       .arg(m_themeManager->secondaryColor().name())
       .arg(m_themeManager->textFont().family())
       .arg(m_themeManager->textFont().pointSize());
    
    m_historyText->setStyleSheet(textEditStyle);
    
    m_welcomeLabel->setFont(m_themeManager->titleFont());
    m_balanceLabel->setFont(m_themeManager->textFont());
    m_addressTitleLabel->setFont(m_themeManager->titleFont());
    m_addressLabel->setFont(m_themeManager->monoFont());
    m_historyTitleLabel->setFont(m_themeManager->titleFont());
    m_historyText->setFont(m_themeManager->textFont());
    
    m_viewBalanceButton->setFont(m_themeManager->buttonFont());
    m_sendButton->setFont(m_themeManager->buttonFont());
    m_receiveButton->setFont(m_themeManager->buttonFont());
    m_copyAddressButton->setFont(m_themeManager->buttonFont());
}