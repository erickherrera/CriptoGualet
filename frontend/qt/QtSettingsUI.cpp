#include "QtSettingsUI.h"
#include "QtThemeManager.h"
#include "Auth.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>

extern std::string g_currentUser;

QtSettingsUI::QtSettingsUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSettingsUI::applyTheme);
}

void QtSettingsUI::setupUI() {
    // Main layout with horizontal centering
    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Center container with max width
    QWidget *centerContainer = new QWidget(this);
    centerContainer->setMaximumWidth(900);

    m_mainLayout = new QVBoxLayout(centerContainer);
    m_mainLayout->setContentsMargins(
        m_themeManager->spacing(10),  // 40px
        m_themeManager->spacing(10),  // 40px
        m_themeManager->spacing(10),  // 40px
        m_themeManager->spacing(10)   // 40px
    );
    m_mainLayout->setSpacing(m_themeManager->spacing(8));  // 32px (was 30)

    // Add stretch before and after to center the container
    outerLayout->addStretch();
    outerLayout->addWidget(centerContainer);
    outerLayout->addStretch();

    // Title
    m_titleLabel = new QLabel("Settings", centerContainer);
    m_titleLabel->setProperty("class", "settings-title");
    m_titleLabel->setAlignment(Qt::AlignLeft);
    m_mainLayout->addWidget(m_titleLabel);

    // Theme Settings Group
    QGroupBox *themeGroup = new QGroupBox("Appearance", centerContainer);
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    themeLayout->setContentsMargins(20, 20, 20, 20);
    themeLayout->setSpacing(15);

    // Theme selector
    m_themeSelector = new QComboBox(centerContainer);
    m_themeSelector->addItem("Dark Theme", static_cast<int>(ThemeType::DARK));
    m_themeSelector->addItem("Light Theme", static_cast<int>(ThemeType::LIGHT));
    m_themeSelector->addItem("Crypto Dark", static_cast<int>(ThemeType::CRYPTO_DARK));
    m_themeSelector->addItem("Crypto Light", static_cast<int>(ThemeType::CRYPTO_LIGHT));

    // Set current theme
    ThemeType currentTheme = m_themeManager->getCurrentTheme();
    m_themeSelector->setCurrentIndex(m_themeSelector->findData(static_cast<int>(currentTheme)));

    connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        int themeValue = m_themeSelector->itemData(index).toInt();
        m_themeManager->applyTheme(static_cast<ThemeType>(themeValue));
    });

    themeLayout->addRow("Theme:", m_themeSelector);

    m_mainLayout->addWidget(themeGroup);

    // Security Settings Group - 2FA
    QGroupBox *securityGroup = new QGroupBox("Security", centerContainer);
    QVBoxLayout *securityLayout = new QVBoxLayout(securityGroup);
    securityLayout->setContentsMargins(20, 20, 20, 20);
    securityLayout->setSpacing(15);

    // 2FA Checkbox
    m_2FACheckbox = new QCheckBox("Enable Two-Factor Authentication (2FA)", centerContainer);
    m_2FACheckbox->setProperty("class", "settings-checkbox");
    securityLayout->addWidget(m_2FACheckbox);

    // 2FA Description
    m_2FADescriptionLabel = new QLabel(
        "Two-factor authentication adds an extra layer of security by requiring "
        "email verification when signing in. When enabled, you'll receive a "
        "verification code via email each time you log in.",
        centerContainer
    );
    m_2FADescriptionLabel->setProperty("class", "subtitle");
    m_2FADescriptionLabel->setWordWrap(true);
    QFont descFont = m_themeManager->textFont();
    descFont.setPointSize(10);
    m_2FADescriptionLabel->setFont(descFont);
    securityLayout->addWidget(m_2FADescriptionLabel);

    // 2FA Status Label
    m_2FAStatusLabel = new QLabel("", centerContainer);
    m_2FAStatusLabel->setProperty("class", "subtitle");
    QFont statusFont = m_themeManager->textFont();
    statusFont.setPointSize(11);
    statusFont.setBold(true);
    m_2FAStatusLabel->setFont(statusFont);
    securityLayout->addWidget(m_2FAStatusLabel);

    // Disable 2FA Button (shown when 2FA is enabled)
    m_disable2FAButton = new QPushButton("Disable 2FA", centerContainer);
    m_disable2FAButton->setProperty("class", "settings-button");
    m_disable2FAButton->setMaximumWidth(150);
    securityLayout->addWidget(m_disable2FAButton);
    m_disable2FAButton->hide(); // Initially hidden

    // Connect signals
    connect(m_2FACheckbox, &QCheckBox::stateChanged, this, &QtSettingsUI::on2FAToggled);
    connect(m_disable2FAButton, &QPushButton::clicked, this, &QtSettingsUI::onDisable2FAClicked);

    // Update 2FA status
    update2FAStatus();

    m_mainLayout->addWidget(securityGroup);

    QGroupBox *walletGroup = new QGroupBox("Wallet", centerContainer);
    QVBoxLayout *walletLayout = new QVBoxLayout(walletGroup);
    walletLayout->setContentsMargins(20, 20, 20, 20);
    m_walletPlaceholder = new QLabel("Wallet settings will be added here", centerContainer);
    m_walletPlaceholder->setProperty("class", "subtitle");
    QFont italicFont = m_themeManager->textFont();
    italicFont.setItalic(true);
    m_walletPlaceholder->setFont(italicFont);
    walletLayout->addWidget(m_walletPlaceholder);
    m_mainLayout->addWidget(walletGroup);

    // Add stretch at the end
    m_mainLayout->addStretch();
}

void QtSettingsUI::applyTheme() {
    // Set main background color without affecting child widgets
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, m_themeManager->backgroundColor());
    setPalette(palette);
    setAutoFillBackground(true);

    // Title styling
    QString titleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 32px;
            font-weight: 700;
            background-color: transparent;
        }
    )").arg(m_themeManager->textColor().name());
    m_titleLabel->setStyleSheet(titleStyle);

    // Group box styling
    QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-size: 16px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->textColor().name());

    // Apply to all group boxes
    for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
        groupBox->setStyleSheet(groupBoxStyle);
    }

    // Combo box styling
    QString comboBoxStyle = QString(R"(
        QComboBox {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 8px;
            min-width: 200px;
        }
        QComboBox:hover {
            border-color: %4;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid %2;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            selection-background-color: %4;
            selection-color: %5;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->accentColor().name())
        .arg(m_themeManager->backgroundColor().name());

    m_themeSelector->setStyleSheet(comboBoxStyle);

    // Apply theme-aware styling to placeholder labels
    QString placeholderStyle = QString(R"(
        QLabel {
            color: %1;
            font-style: italic;
            background-color: transparent;
        }
    )").arg(m_themeManager->subtitleColor().name());

    m_walletPlaceholder->setStyleSheet(placeholderStyle);

    // 2FA Checkbox styling
    QString checkboxStyle = QString(R"(
        QCheckBox {
            color: %1;
            font-size: 14px;
            font-weight: 500;
            background-color: transparent;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid %2;
            border-radius: 4px;
            background-color: %3;
        }
        QCheckBox::indicator:hover {
            border-color: %4;
        }
        QCheckBox::indicator:checked {
            background-color: %4;
            border-color: %4;
            image: none;
        }
        QCheckBox::indicator:unchecked {
            background-color: %3;
            border-color: %2;
        }
    )")
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->accentColor().name());

    m_2FACheckbox->setStyleSheet(checkboxStyle);

    // 2FA Description styling
    QString descStyle = QString(R"(
        QLabel {
            color: %1;
            background-color: transparent;
            padding: 5px 0px;
        }
    )").arg(m_themeManager->subtitleColor().name());
    m_2FADescriptionLabel->setStyleSheet(descStyle);

    // 2FA Status label styling
    QString statusStyle = QString(R"(
        QLabel {
            color: %1;
            background-color: transparent;
            padding: 10px 0px 5px 0px;
        }
    )").arg(m_themeManager->accentColor().name());
    m_2FAStatusLabel->setStyleSheet(statusStyle);

    // Button styling
    QString buttonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %4;
            border-color: %4;
        }
        QPushButton:pressed {
            background-color: %5;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->accentColor().name())
        .arg(m_themeManager->secondaryColor().name());

    m_disable2FAButton->setStyleSheet(buttonStyle);

    // Apply fonts
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    QFont textFont = m_themeManager->textFont();
    textFont.setPointSize(12);
    m_themeSelector->setFont(textFont);

    // Refresh 2FA status when theme changes
    update2FAStatus();
}

void QtSettingsUI::update2FAStatus() {
    if (g_currentUser.empty()) {
        m_2FACheckbox->setEnabled(false);
        m_2FAStatusLabel->setText("Please sign in to manage 2FA settings.");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->subtitleColor().name()));
        m_disable2FAButton->hide();
        return;
    }

    bool isVerified = Auth::IsEmailVerified(g_currentUser);
    m_2FACheckbox->setChecked(isVerified);
    m_2FACheckbox->setEnabled(true);

    if (isVerified) {
        m_2FAStatusLabel->setText("✓ Two-factor authentication is enabled");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->accentColor().name()));
        m_disable2FAButton->show();
    } else {
        m_2FAStatusLabel->setText("Two-factor authentication is disabled");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->subtitleColor().name()));
        m_disable2FAButton->hide();
    }
}

void QtSettingsUI::refresh2FAStatus() {
    update2FAStatus();
}

void QtSettingsUI::on2FAToggled(int state) {
    if (g_currentUser.empty()) {
        m_2FACheckbox->setChecked(false);
        QMessageBox::warning(this, "Not Signed In",
                            "Please sign in to manage 2FA settings.");
        return;
    }

    if (state == Qt::Checked) {
        // User wants to enable 2FA
        // Note: 2FA is enabled by verifying email, not by checkbox
        // So we'll show a message directing them to verify their email
        QMessageBox::information(this, "Enable 2FA",
                                "To enable two-factor authentication, you need to verify your email address.\n\n"
                                "If you haven't verified your email yet, please sign out and complete the "
                                "email verification process during registration or login.");
        
        // Check current status
        bool isVerified = Auth::IsEmailVerified(g_currentUser);
        if (!isVerified) {
            // Not verified, uncheck the box
            m_2FACheckbox->setChecked(false);
            update2FAStatus();
        }
    } else {
        // User wants to disable 2FA - handled by the disable button
        // Just update status
        update2FAStatus();
    }
}

void QtSettingsUI::onDisable2FAClicked() {
    if (g_currentUser.empty()) {
        QMessageBox::warning(this, "Not Signed In",
                            "Please sign in to manage 2FA settings.");
        return;
    }

    // Confirm with user
    int ret = QMessageBox::question(this, "Disable 2FA",
                                   "Are you sure you want to disable two-factor authentication?\n\n"
                                   "This will reduce the security of your account. You can re-enable it "
                                   "by verifying your email address again.",
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        Auth::AuthResponse response = Auth::DisableTwoFactorAuth(g_currentUser);
        
        if (response.success()) {
            QMessageBox::information(this, "2FA Disabled",
                                   "Two-factor authentication has been disabled successfully.");
            update2FAStatus();
        } else {
            QMessageBox::warning(this, "Error",
                                QString("Failed to disable 2FA: %1").arg(QString::fromStdString(response.message)));
            update2FAStatus(); // Refresh to show correct status
        }
    }
}
