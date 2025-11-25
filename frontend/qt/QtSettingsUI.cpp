#include "QtSettingsUI.h"
#include "QtThemeManager.h"
#include "QtPasswordConfirmDialog.h"
#include "QtEmailVerificationDialog.h"
#include "Auth.h"
#include "Database/DatabaseManager.h"
#include "Repository/UserRepository.h"

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

    // 2FA Title
    QLabel *twoFactorTitle = new QLabel("Two-Factor Authentication (2FA)", centerContainer);
    twoFactorTitle->setProperty("class", "settings-section-title");
    QFont sectionFont = m_themeManager->textFont();
    sectionFont.setPointSize(13);
    sectionFont.setBold(true);
    twoFactorTitle->setFont(sectionFont);
    securityLayout->addWidget(twoFactorTitle);

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

    // Button container
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // Enable 2FA Button (shown when 2FA is disabled)
    m_enable2FAButton = new QPushButton("Enable 2FA", centerContainer);
    m_enable2FAButton->setProperty("class", "settings-button");
    m_enable2FAButton->setMinimumWidth(120);
    m_enable2FAButton->setMaximumWidth(150);
    buttonLayout->addWidget(m_enable2FAButton);
    m_enable2FAButton->hide(); // Initially hidden

    // Disable 2FA Button (shown when 2FA is enabled)
    m_disable2FAButton = new QPushButton("Disable 2FA", centerContainer);
    m_disable2FAButton->setProperty("class", "settings-button");
    m_disable2FAButton->setMinimumWidth(120);
    m_disable2FAButton->setMaximumWidth(150);
    buttonLayout->addWidget(m_disable2FAButton);
    m_disable2FAButton->hide(); // Initially hidden

    buttonLayout->addStretch();
    securityLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_enable2FAButton, &QPushButton::clicked, this, &QtSettingsUI::onEnable2FAClicked);
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

    // Enable button styling (accent color)
    QString enableButtonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )")
        .arg(m_themeManager->accentColor().name())
        .arg(m_themeManager->accentColor().lighter(110).name())
        .arg(m_themeManager->accentColor().darker(110).name());

    m_enable2FAButton->setStyleSheet(enableButtonStyle);

    // Disable button styling (secondary color)
    QString disableButtonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %3;
            border-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->secondaryColor().darker(110).name());

    m_disable2FAButton->setStyleSheet(disableButtonStyle);

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
        m_2FAStatusLabel->setText("Please sign in to manage 2FA settings.");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->subtitleColor().name()));
        m_enable2FAButton->hide();
        m_disable2FAButton->hide();
        return;
    }

    bool isVerified = Auth::IsEmailVerified(g_currentUser);

    if (isVerified) {
        m_2FAStatusLabel->setText("✓ Two-factor authentication is enabled");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->accentColor().name()));
        m_enable2FAButton->hide();
        m_disable2FAButton->show();
    } else {
        m_2FAStatusLabel->setText("Two-factor authentication is disabled");
        m_2FAStatusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->subtitleColor().name()));
        m_enable2FAButton->show();
        m_disable2FAButton->hide();
    }
}

void QtSettingsUI::refresh2FAStatus() {
    update2FAStatus();
}

void QtSettingsUI::onEnable2FAClicked() {
    if (g_currentUser.empty()) {
        QMessageBox::warning(this, "Not Signed In",
                            "Please sign in to manage 2FA settings.");
        return;
    }

    // Step 1: Password confirmation dialog
    QtPasswordConfirmDialog passwordDialog(
        QString::fromStdString(g_currentUser),
        "Enable Two-Factor Authentication",
        "Please enter your password to enable 2FA:",
        this
    );

    if (passwordDialog.exec() == QDialog::Accepted && passwordDialog.isConfirmed()) {
        QString password = passwordDialog.getPassword();

        // Step 2: Enable 2FA (verifies password and sends code)
        Auth::AuthResponse response = Auth::EnableTwoFactorAuth(
            g_currentUser,
            password.toStdString()
        );

        if (response.success()) {
            // Step 3: Show verification code entry dialog
            // Get user email for display
            QMessageBox::information(this, "Verification Code Sent",
                                   "A verification code has been sent to your email.\n\n"
                                   "Please enter the code to complete 2FA setup.");

            // Get user email
            QString userEmail = "";
            try {
                auto &dbManager = Database::DatabaseManager::getInstance();
                Repository::UserRepository userRepo(dbManager);
                auto userResult = userRepo.getUserByUsername(g_currentUser);
                if (userResult.success) {
                    userEmail = QString::fromStdString(userResult.data.email);
                }
            } catch (...) {
                // Fallback - show dialog without email
            }

            QtEmailVerificationDialog verifyDialog(
                QString::fromStdString(g_currentUser),
                userEmail,
                this
            );

            if (verifyDialog.exec() == QDialog::Accepted && verifyDialog.isVerified()) {
                QMessageBox::information(this, "2FA Enabled",
                                       "Two-factor authentication has been enabled successfully!\n\n"
                                       "You'll now need to verify your email each time you sign in.");
                update2FAStatus();
            } else {
                // User cancelled or verification failed
                QMessageBox::information(this, "2FA Not Enabled",
                                       "Two-factor authentication was not enabled.\n\n"
                                       "The verification code remains valid for 10 minutes if you want to try again.");
            }
        } else {
            // Failed to send code
            QMessageBox::warning(this, "Error",
                               QString("Failed to enable 2FA: %1").arg(QString::fromStdString(response.message)));
        }
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
                                   "later through the settings.",
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        // Require password verification for security
        QtPasswordConfirmDialog passwordDialog(
            QString::fromStdString(g_currentUser),
            "Disable Two-Factor Authentication",
            "Please enter your password to disable 2FA:",
            this
        );

        if (passwordDialog.exec() == QDialog::Accepted && passwordDialog.isConfirmed()) {
            QString password = passwordDialog.getPassword();

            // Disable 2FA with password verification
            Auth::AuthResponse response = Auth::DisableTwoFactorAuth(
                g_currentUser,
                password.toStdString()
            );

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
}
