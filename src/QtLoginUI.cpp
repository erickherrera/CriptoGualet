#include "../include/QtLoginUI.h"
#include "../include/QtThemeManager.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

QtLoginUI::QtLoginUI(QWidget* parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()) {
    setupUI();
    applyTheme();

    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtLoginUI::onThemeChanged);
}

void QtLoginUI::setupUI() {
    // Root layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(40, 10, 40, 40);
    m_mainLayout->setSpacing(20);

    // Top stretch for nice vertical centering
    m_mainLayout->addItem(new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // ===== Header (Title + Subtitle) OUTSIDE the card =====
    {
        QVBoxLayout* headerLayout = new QVBoxLayout();
        headerLayout->setSpacing(6);

        m_titleLabel = new QLabel("CriptoGualet", this);
        m_titleLabel->setProperty("class", "title");
        m_titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(m_titleLabel);

        m_subtitleLabel = new QLabel("Secure Bitcoin Wallet", this);
        m_subtitleLabel->setProperty("class", "subtitle");
        m_subtitleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(m_subtitleLabel);

        // Center the header
        QHBoxLayout* headerRow = new QHBoxLayout();
        headerRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
        headerRow->addLayout(headerLayout);
        headerRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

        m_mainLayout->addLayout(headerRow);
    }

    // Slight spacing between header and card
    m_mainLayout->addSpacing(15);

    // ===== Card (fields + buttons only) =====
    createLoginCard();

    // Bottom stretch for vertical centering
    m_mainLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // Theme selector at bottom
    setupThemeSelector();
}

void QtLoginUI::createLoginCard() {
    m_loginCard = new QFrame(this);
    m_loginCard->setProperty("class", "card");
    m_loginCard->setFixedSize(420, 300);  // more compact now that title/subtitle moved out

    m_cardLayout = new QVBoxLayout(m_loginCard);
    m_cardLayout->setContentsMargins(30, 30, 30, 30);
    m_cardLayout->setSpacing(16);

    // Form with NO left-side labels (removes those "weird boxes")
    m_formLayout = new QFormLayout();
    m_formLayout->setFormAlignment(Qt::AlignTop);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_formLayout->setSpacing(12);

    m_usernameEdit = new QLineEdit(m_loginCard);
    m_usernameEdit->setPlaceholderText("Username");
    m_usernameEdit->setMinimumHeight(40);
    // Add row without label (overload)
    m_formLayout->addRow(m_usernameEdit);

    m_passwordEdit = new QLineEdit(m_loginCard);
    m_passwordEdit->setPlaceholderText("Password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumHeight(40);
    m_formLayout->addRow(m_passwordEdit);

    m_cardLayout->addLayout(m_formLayout);

    // Buttons row
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);

    m_loginButton = new QPushButton("Sign In", m_loginCard);
    m_loginButton->setMinimumHeight(44);
    m_loginButton->setMinimumWidth(120);
    m_buttonLayout->addWidget(m_loginButton);

    m_registerButton = new QPushButton("Create Account", m_loginCard);
    m_registerButton->setMinimumHeight(44);
    m_registerButton->setMinimumWidth(140);
    m_buttonLayout->addWidget(m_registerButton);

    // Space above buttons for breathing room
    m_cardLayout->addSpacing(6);
    m_cardLayout->addLayout(m_buttonLayout);

    // Center the card
    QHBoxLayout* cardCenterLayout = new QHBoxLayout();
    cardCenterLayout->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    cardCenterLayout->addWidget(m_loginCard);
    cardCenterLayout->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_mainLayout->addLayout(cardCenterLayout);

    // Signals
    connect(m_loginButton, &QPushButton::clicked, this, &QtLoginUI::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &QtLoginUI::onRegisterClicked);
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &QtLoginUI::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &QtLoginUI::onLoginClicked);
}

void QtLoginUI::setupThemeSelector() {
    m_themeLayout = new QHBoxLayout();
    m_themeLayout->setSpacing(8);
    m_themeLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QLabel* themeLabel = new QLabel("Theme:");
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

    connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
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

void QtLoginUI::onThemeChanged() { applyTheme(); }

void QtLoginUI::applyTheme() { updateStyles(); }

void QtLoginUI::updateStyles() {
    // Base application stylesheet
    setStyleSheet(m_themeManager->getMainWindowStyleSheet());

    // Card visuals
    m_loginCard->setStyleSheet(m_themeManager->getCardStyleSheet());

    // Typography
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    m_titleLabel->setStyleSheet(labelStyle);
    m_subtitleLabel->setStyleSheet(labelStyle);

    // Buttons keep hover styling
    QString buttonStyle = m_themeManager->getButtonStyleSheet();
    m_loginButton->setStyleSheet(buttonStyle);
    m_registerButton->setStyleSheet(buttonStyle);

    // Inputs
    QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
    m_usernameEdit->setStyleSheet(lineEditStyle);
    m_passwordEdit->setStyleSheet(lineEditStyle);

    // Fonts
    m_titleLabel->setFont(m_themeManager->titleFont());
    m_subtitleLabel->setFont(m_themeManager->textFont());
    m_loginButton->setFont(m_themeManager->buttonFont());
    m_registerButton->setFont(m_themeManager->buttonFont());
    m_usernameEdit->setFont(m_themeManager->textFont());
    m_passwordEdit->setFont(m_themeManager->textFont());

    // ==== Neutralize hover border changes for inputs (buttons unaffected) ====
    // This ensures only buttons show hover feedback.
    const QString hoverNeutralizer = R"CSS(
QLineEdit:hover,
QTextEdit:hover,
QPlainTextEdit:hover,
QComboBox:hover {
    /* Keep the same border color as non-hover to avoid flicker/shift */
    border: 1px solid palette(mid);
}
)CSS";

    // Append the override so it wins over theme rules
    setStyleSheet(styleSheet() + "\n" + hoverNeutralizer);
}
