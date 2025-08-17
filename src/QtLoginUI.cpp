#include "../include/QtLoginUI.h"
#include "../include/QtThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QSpacerItem>
#include <QComboBox>
#include <QApplication>

QtLoginUI::QtLoginUI(QWidget *parent)
    : QWidget(parent)
    , m_themeManager(&QtThemeManager::instance())
{
    setupUI();
    setupThemeSelector();
    applyTheme();
    
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtLoginUI::onThemeChanged);
}

void QtLoginUI::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(40, 40, 40, 40);
    m_mainLayout->setSpacing(20);
    
    m_mainLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    
    createLoginCard();
    
    m_mainLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    
    setupThemeSelector();
}

void QtLoginUI::createLoginCard() {
    m_loginCard = new QFrame(this);
    m_loginCard->setProperty("class", "card");
    m_loginCard->setFixedSize(400, 450);
    
    m_cardLayout = new QVBoxLayout(m_loginCard);
    m_cardLayout->setContentsMargins(30, 30, 30, 30);
    m_cardLayout->setSpacing(20);
    
    m_titleLabel = new QLabel("CriptoGualet", m_loginCard);
    m_titleLabel->setProperty("class", "title");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_cardLayout->addWidget(m_titleLabel);
    
    m_subtitleLabel = new QLabel("Secure Bitcoin Wallet", m_loginCard);
    m_subtitleLabel->setProperty("class", "subtitle");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_cardLayout->addWidget(m_subtitleLabel);
    
    m_cardLayout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));
    
    m_formLayout = new QFormLayout();
    m_formLayout->setSpacing(15);
    
    m_usernameEdit = new QLineEdit(m_loginCard);
    m_usernameEdit->setPlaceholderText("Enter your username");
    m_usernameEdit->setMinimumHeight(40);
    m_formLayout->addRow("Username:", m_usernameEdit);
    
    m_passwordEdit = new QLineEdit(m_loginCard);
    m_passwordEdit->setPlaceholderText("Enter your password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumHeight(40);
    m_formLayout->addRow("Password:", m_passwordEdit);
    
    m_cardLayout->addLayout(m_formLayout);
    
    m_cardLayout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));
    
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(15);
    
    m_loginButton = new QPushButton("Sign In", m_loginCard);
    m_loginButton->setMinimumHeight(45);
    m_loginButton->setMinimumWidth(120);
    m_buttonLayout->addWidget(m_loginButton);
    
    m_registerButton = new QPushButton("Create Account", m_loginCard);
    m_registerButton->setMinimumHeight(45);
    m_registerButton->setMinimumWidth(120);
    m_buttonLayout->addWidget(m_registerButton);
    
    m_cardLayout->addLayout(m_buttonLayout);
    
    QHBoxLayout *cardCenterLayout = new QHBoxLayout();
    cardCenterLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    cardCenterLayout->addWidget(m_loginCard);
    cardCenterLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    m_mainLayout->addLayout(cardCenterLayout);
    
    connect(m_loginButton, &QPushButton::clicked, this, &QtLoginUI::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &QtLoginUI::onRegisterClicked);
    
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &QtLoginUI::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &QtLoginUI::onLoginClicked);
}

void QtLoginUI::setupThemeSelector() {
    m_themeLayout = new QHBoxLayout();
    m_themeLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QLabel *themeLabel = new QLabel("Theme:");
    m_themeLayout->addWidget(themeLabel);
    
    m_themeSelector = new QComboBox();
    m_themeSelector->addItem("Crypto Dark", static_cast<int>(ThemeType::CRYPTO_DARK));
    m_themeSelector->addItem("Crypto Light", static_cast<int>(ThemeType::CRYPTO_LIGHT));
    m_themeSelector->addItem("Dark", static_cast<int>(ThemeType::DARK));
    m_themeSelector->addItem("Light", static_cast<int>(ThemeType::LIGHT));
    m_themeSelector->setCurrentIndex(0);
    m_themeLayout->addWidget(m_themeSelector);
    
    m_themeLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    m_mainLayout->addLayout(m_themeLayout);
    
    connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        ThemeType theme = static_cast<ThemeType>(m_themeSelector->itemData(index).toInt());
        m_themeManager->applyTheme(theme);
    });
}

void QtLoginUI::onLoginClicked() {
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        return;
    }
    
    emit loginRequested(username, password);
    m_passwordEdit->clear();
}

void QtLoginUI::onRegisterClicked() {
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        return;
    }
    
    emit registerRequested(username, password);
    m_passwordEdit->clear();
}

void QtLoginUI::onThemeChanged() {
    applyTheme();
}

void QtLoginUI::applyTheme() {
    updateStyles();
}

void QtLoginUI::updateStyles() {
    setStyleSheet(m_themeManager->getMainWindowStyleSheet());
    
    m_loginCard->setStyleSheet(m_themeManager->getCardStyleSheet());
    
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    m_titleLabel->setStyleSheet(labelStyle);
    m_subtitleLabel->setStyleSheet(labelStyle);
    
    QString buttonStyle = m_themeManager->getButtonStyleSheet();
    m_loginButton->setStyleSheet(buttonStyle);
    m_registerButton->setStyleSheet(buttonStyle);
    
    QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
    m_usernameEdit->setStyleSheet(lineEditStyle);
    m_passwordEdit->setStyleSheet(lineEditStyle);
    
    m_titleLabel->setFont(m_themeManager->titleFont());
    m_subtitleLabel->setFont(m_themeManager->textFont());
    m_loginButton->setFont(m_themeManager->buttonFont());
    m_registerButton->setFont(m_themeManager->buttonFont());
    m_usernameEdit->setFont(m_themeManager->textFont());
    m_passwordEdit->setFont(m_themeManager->textFont());
}