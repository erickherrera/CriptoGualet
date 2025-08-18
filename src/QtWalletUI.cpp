// QtWalletUI.cpp
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

    createWelcomeSection();
    createAddressSection();
    createActionButtons();
    createTransactionHistory();

    m_contentLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);
}

void QtWalletUI::createWelcomeSection() {
    m_welcomeCard = new QFrame(m_scrollContent);
    m_welcomeCard->setProperty("class", "card");
    m_welcomeCard->setObjectName("welcomeCard");

    QVBoxLayout *welcomeLayout = new QVBoxLayout(m_welcomeCard);
    welcomeLayout->setContentsMargins(25, 25, 25, 25);
    welcomeLayout->setSpacing(15);

    m_welcomeLabel = new QLabel("Welcome back!", m_welcomeCard);
    m_welcomeLabel->setProperty("class", "title");
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(m_welcomeLabel);

    m_balanceLabel = new QLabel("Balance: 0.00000000 BTC", m_welcomeCard);
    m_balanceLabel->setProperty("class", "subtitle");
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(m_balanceLabel);

    m_contentLayout->addWidget(m_welcomeCard);
}

void QtWalletUI::createAddressSection() {
    m_addressCard = new QFrame(m_scrollContent);
    m_addressCard->setProperty("class", "card");
    m_addressCard->setObjectName("addressCard");

    QVBoxLayout *addressLayout = new QVBoxLayout(m_addressCard);
    addressLayout->setContentsMargins(25, 25, 25, 25);
    addressLayout->setSpacing(15);

    m_addressTitleLabel = new QLabel("Your Bitcoin Address", m_addressCard);
    m_addressTitleLabel->setProperty("class", "title");
    addressLayout->addWidget(m_addressTitleLabel);

    QHBoxLayout *addressRowLayout = new QHBoxLayout();

    m_addressLabel = new QLabel("", m_addressCard);
    m_addressLabel->setProperty("class", "address");
    m_addressLabel->setWordWrap(true);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addressRowLayout->addWidget(m_addressLabel, 1);

    m_copyAddressButton = new QPushButton("Copy", m_addressCard);
    m_copyAddressButton->setMaximumWidth(80);
    addressRowLayout->addWidget(m_copyAddressButton);

    addressLayout->addLayout(addressRowLayout);

    connect(m_copyAddressButton, &QPushButton::clicked, [this]() {
        if (!m_currentAddress.isEmpty()) {
            QApplication::clipboard()->setText(m_currentAddress);
            QMessageBox::information(this, "Copied", "Address copied to clipboard!");
        }
    });

    m_contentLayout->addWidget(m_addressCard);
}

void QtWalletUI::createActionButtons() {
    m_actionsCard = new QFrame(m_scrollContent);
    m_actionsCard->setProperty("class", "card");
    m_actionsCard->setObjectName("actionsCard");

    QVBoxLayout *actionsMainLayout = new QVBoxLayout(m_actionsCard);
    actionsMainLayout->setContentsMargins(25, 25, 25, 25);
    actionsMainLayout->setSpacing(20);

    // Title styled the same as other sections
    m_actionsTitle = new QLabel("Wallet Actions", m_actionsCard);
    m_actionsTitle->setProperty("class", "title");
    actionsMainLayout->addWidget(m_actionsTitle);

    m_actionsLayout = new QGridLayout();
    m_actionsLayout->setSpacing(15);

    m_viewBalanceButton = new QPushButton("View Balance", m_actionsCard);
    m_viewBalanceButton->setMinimumHeight(60);
    m_actionsLayout->addWidget(m_viewBalanceButton, 0, 0);

    m_sendButton = new QPushButton("Send Bitcoin", m_actionsCard);
    m_sendButton->setMinimumHeight(60);
    m_actionsLayout->addWidget(m_sendButton, 0, 1);

    m_receiveButton = new QPushButton("Receive Bitcoin", m_actionsCard);
    m_receiveButton->setMinimumHeight(60);
    m_actionsLayout->addWidget(m_receiveButton, 1, 0);

    m_logoutButton = new QPushButton("Sign Out", m_actionsCard);
    m_logoutButton->setMinimumHeight(60);
    m_actionsLayout->addWidget(m_logoutButton, 1, 1);

    actionsMainLayout->addLayout(m_actionsLayout);

    connect(m_viewBalanceButton, &QPushButton::clicked, this, &QtWalletUI::onViewBalanceClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &QtWalletUI::onSendBitcoinClicked);
    connect(m_receiveButton, &QPushButton::clicked, this, &QtWalletUI::onReceiveBitcoinClicked);
    connect(m_logoutButton, &QPushButton::clicked, this, &QtWalletUI::onLogoutClicked);

    m_contentLayout->addWidget(m_actionsCard);
}

void QtWalletUI::createTransactionHistory() {
    m_historyCard = new QFrame(m_scrollContent);
    m_historyCard->setProperty("class", "card");
    m_historyCard->setObjectName("historyCard");

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

    m_welcomeLabel->setText(QString("Welcome back, %1!").arg(username));
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

void QtWalletUI::onLogoutClicked() {
    emit logoutRequested();
}

void QtWalletUI::onThemeChanged() {
    applyTheme();
}

void QtWalletUI::applyTheme() {
    updateStyles();
}

void QtWalletUI::updateStyles() {
    setStyleSheet(m_themeManager->getMainWindowStyleSheet());

    const QString accent = m_themeManager->accentColor().name();
    const QString surface = m_themeManager->surfaceColor().name();

    auto cardCss = [&](const QString& objName) {
        return QString(R"(
            QFrame#%1 {
                background-color: %2;
                border: 2px solid %3;
                border-radius: 12px;
                padding: 20px;
            }
        )").arg(objName, surface, accent);
    };

    m_welcomeCard->setStyleSheet(cardCss("welcomeCard"));
    m_addressCard->setStyleSheet(cardCss("addressCard"));
    m_actionsCard->setStyleSheet(cardCss("actionsCard"));
    m_historyCard->setStyleSheet(cardCss("historyCard"));

    QString labelStyle = m_themeManager->getLabelStyleSheet();
    m_welcomeLabel->setStyleSheet(labelStyle);
    m_balanceLabel->setStyleSheet(labelStyle);
    m_addressTitleLabel->setStyleSheet(labelStyle);
    m_addressLabel->setStyleSheet(labelStyle);
    m_actionsTitle->setStyleSheet(labelStyle);
    m_historyTitleLabel->setStyleSheet(labelStyle);

    QString buttonStyle = m_themeManager->getButtonStyleSheet();
    m_viewBalanceButton->setStyleSheet(buttonStyle);
    m_sendButton->setStyleSheet(buttonStyle);
    m_receiveButton->setStyleSheet(buttonStyle);
    m_logoutButton->setStyleSheet(buttonStyle);
    m_copyAddressButton->setStyleSheet(buttonStyle);

    QString textEditStyle = QString(R"(
        QTextEdit {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 6px;
            padding: 8px;
            font-family: %3;
            font-size: %4px;
        }
    )").arg(surface)
       .arg(m_themeManager->textColor().name())
       .arg(m_themeManager->textFont().family())
       .arg(m_themeManager->textFont().pointSize());

    m_historyText->setStyleSheet(textEditStyle);

    m_welcomeLabel->setFont(m_themeManager->titleFont());
    m_balanceLabel->setFont(m_themeManager->textFont());
    m_addressTitleLabel->setFont(m_themeManager->titleFont());
    m_addressLabel->setFont(m_themeManager->monoFont());
    m_actionsTitle->setFont(m_themeManager->titleFont());
    m_historyTitleLabel->setFont(m_themeManager->titleFont());
    m_historyText->setFont(m_themeManager->textFont());

    m_viewBalanceButton->setFont(m_themeManager->buttonFont());
    m_sendButton->setFont(m_themeManager->buttonFont());
    m_receiveButton->setFont(m_themeManager->buttonFont());
    m_logoutButton->setFont(m_themeManager->buttonFont());
    m_copyAddressButton->setFont(m_themeManager->buttonFont());
}
