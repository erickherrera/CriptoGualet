#include "include/QtSettingsUI.h"
#include "include/QtThemeManager.h"
#include "include/QtPasswordConfirmDialog.h"
#include "../../backend/core/include/Auth.h"
#include "../../backend/utils/include/QRGenerator.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QPixmap>
#include <QImage>
#include <QClipboard>
#include <QGuiApplication>

extern std::string g_currentUser;

QtSettingsUI::QtSettingsUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()),
      m_2FATitleLabel(nullptr), m_2FAStatusLabel(nullptr),
      m_enable2FAButton(nullptr), m_disable2FAButton(nullptr),
      m_2FADescriptionLabel(nullptr) {

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

    // Use a Scroll Area for laptop screens
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("background-color: transparent; border: none;");

    // Center container with max width
    QWidget *centerContainer = new QWidget();
    centerContainer->setMaximumWidth(900);
    centerContainer->setStyleSheet("background-color: transparent;");

    m_mainLayout = new QVBoxLayout(centerContainer);
    // Optimized for laptop screens (reduced margins/spacing)
    m_mainLayout->setContentsMargins(
        m_themeManager->spacing(6),  // 24px (was 40px)
        m_themeManager->spacing(5),  // 20px (was 40px)
        m_themeManager->spacing(6),  // 24px (was 40px)
        m_themeManager->spacing(5)   // 20px (was 40px)
    );
    m_mainLayout->setSpacing(m_themeManager->spacing(4));  // 16px (was 32px)

    scrollArea->setWidget(centerContainer);

    // Add stretch before and after to center the container
    outerLayout->addStretch();
    outerLayout->addWidget(scrollArea);
    outerLayout->addStretch();

    // Title
    m_titleLabel = new QLabel("Settings", centerContainer);
    m_titleLabel->setProperty("class", "title");
    m_titleLabel->setAlignment(Qt::AlignLeft);
    m_mainLayout->addWidget(m_titleLabel);

    // Theme Settings Group
    QGroupBox *themeGroup = new QGroupBox("Appearance", centerContainer);
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    themeLayout->setContentsMargins(15, 15, 15, 15); // Compacted (was 20)
    themeLayout->setSpacing(10); // Compacted (was 15)

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
    securityGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    securityGroup->setVisible(true);
    QVBoxLayout *securityLayout = new QVBoxLayout(securityGroup);
    securityLayout->setContentsMargins(15, 20, 15, 15);  // Compacted (was 20, 25, 20, 20)
    securityLayout->setSpacing(8); // Compacted (was 12)
    // Apply group box styling immediately for visibility
    QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 20px;
            font-size: 16px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 15px;
            padding: 0 5px;
            margin-top: 5px;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->textColor().name());
    securityGroup->setStyleSheet(groupBoxStyle);

    // 2FA Title
    m_2FATitleLabel = new QLabel("Two-Factor Authentication (2FA)", securityGroup);
    m_2FATitleLabel->setProperty("class", "title");
    m_2FATitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_2FATitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_2FATitleLabel->setWordWrap(false);
    securityLayout->addWidget(m_2FATitleLabel);

    // 2FA Description
    m_2FADescriptionLabel = new QLabel(
        "Two-factor authentication adds an extra layer of security by requiring "
        "a code from your authenticator app when signing in. Compatible with "
        "Google Authenticator, Authy, Microsoft Authenticator, and other TOTP apps.",
        securityGroup
    );
    m_2FADescriptionLabel->setProperty("class", "subtitle");
    m_2FADescriptionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_2FADescriptionLabel->setWordWrap(true);
    m_2FADescriptionLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    securityLayout->addWidget(m_2FADescriptionLabel);

    // 2FA Status Label
    m_2FAStatusLabel = new QLabel("Loading...", securityGroup);
    m_2FAStatusLabel->setProperty("class", "subtitle");
    m_2FAStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_2FAStatusLabel->setWordWrap(true);
    m_2FAStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    securityLayout->addWidget(m_2FAStatusLabel);

    // Button container
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);

    // Enable 2FA Button (shown when 2FA is disabled)
    m_enable2FAButton = new QPushButton("Enable 2FA", securityGroup);
    m_enable2FAButton->setProperty("class", "accent-button");
    m_enable2FAButton->setMinimumWidth(120);
    m_enable2FAButton->setMaximumWidth(150);
    m_enable2FAButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_enable2FAButton->setEnabled(true);
    buttonLayout->addWidget(m_enable2FAButton);
    m_enable2FAButton->hide(); // Initially hidden, will be shown by update2FAStatus() if needed

    // Disable 2FA Button (shown when 2FA is enabled)
    m_disable2FAButton = new QPushButton("Disable 2FA", securityGroup);
    m_disable2FAButton->setProperty("class", "secondary-button");
    m_disable2FAButton->setMinimumWidth(120);
    m_disable2FAButton->setMaximumWidth(150);
    m_disable2FAButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_disable2FAButton->setEnabled(true);
    buttonLayout->addWidget(m_disable2FAButton);
    m_disable2FAButton->hide(); // Initially hidden, will be shown by update2FAStatus() if needed

    buttonLayout->addStretch();
    securityLayout->addLayout(buttonLayout);

    // Connect signals for 2FA enablement
    connect(m_enable2FAButton, &QPushButton::clicked, this, &QtSettingsUI::onEnable2FAClicked);
    connect(m_disable2FAButton, &QPushButton::clicked, this, &QtSettingsUI::onDisable2FAClicked);

    // Update 2FA status to show/hide appropriate buttons
    update2FAStatus();

    m_mainLayout->addWidget(securityGroup);

    QGroupBox *walletGroup = new QGroupBox("Wallet", centerContainer);
    QVBoxLayout *walletLayout = new QVBoxLayout(walletGroup);
    walletLayout->setContentsMargins(15, 15, 15, 15); // Compacted (was 20)
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

    // Apply the global label and button stylesheets to this widget
    // This ensures that class properties like "title" and "subtitle" are picked up
    QString globalStyle = m_themeManager->getLabelStyleSheet() + 
                          m_themeManager->getButtonStyleSheet();
    this->setStyleSheet(globalStyle);

    // Title styling (override for the main settings title)
    QString mainTitleStyle = QString(R"(
        QLabel[class="title"]#mainTitle {
            font-size: 28px;
            font-weight: 700;
            margin-bottom: 5px;
        }
    )");
    m_titleLabel->setObjectName("mainTitle");
    m_titleLabel->setStyleSheet(mainTitleStyle);

    // Group box styling
    QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 22px;
            font-size: 15px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 3px;
            margin-top: 4px;
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
    QString comboBoxStyle = m_themeManager->getLineEditStyleSheet() + QString(R"(
        QComboBox {
            min-width: 200px;
            padding: 8px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid %1;
            margin-right: 8px;
        }
    )").arg(m_themeManager->textColor().name());

    m_themeSelector->setStyleSheet(comboBoxStyle);

    // 2FA Section styling is now handled by class properties and global stylesheet
    // No need for manual setStyleSheet calls here for m_2FATitleLabel, m_2FADescriptionLabel, etc.

    // Enable button styling
    if (m_enable2FAButton) {
        m_enable2FAButton->setStyleSheet(m_themeManager->getButtonStyleSheet());
    }

    // Disable button styling
    if (m_disable2FAButton) {
        m_disable2FAButton->setStyleSheet(m_themeManager->getButtonStyleSheet());
    }

    // Apply fonts
    m_titleLabel->setFont(m_themeManager->titleFont());
    m_themeSelector->setFont(m_themeManager->textFont());

    // Style SMTP input fields
    QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
    // ... we can apply this to any line edits if needed ...

    // Refresh 2FA status when theme changes
    update2FAStatus();
}

void QtSettingsUI::update2FAStatus() {
    if (g_currentUser.empty()) {
        if (m_2FAStatusLabel) {
            m_2FAStatusLabel->setText("Please sign in to manage 2FA settings.");
        }
        if (m_enable2FAButton) m_enable2FAButton->hide();
        if (m_disable2FAButton) m_disable2FAButton->hide();
        return;
    }

    bool isEnabled = Auth::IsTwoFactorEnabled(g_currentUser);

    if (isEnabled) {
        if (m_2FAStatusLabel) {
            m_2FAStatusLabel->setText("✓ Two-factor authentication is enabled");
            // We can use dynamic properties for semantic coloring if needed
            m_2FAStatusLabel->setProperty("status", "success");
            m_2FAStatusLabel->style()->unpolish(m_2FAStatusLabel);
            m_2FAStatusLabel->style()->polish(m_2FAStatusLabel);
        }
        if (m_enable2FAButton) m_enable2FAButton->hide();
        if (m_disable2FAButton) {
            m_disable2FAButton->show();
            m_disable2FAButton->setEnabled(true);
        }
    } else {
        if (m_2FAStatusLabel) {
            m_2FAStatusLabel->setText("Two-factor authentication is disabled");
            m_2FAStatusLabel->setProperty("status", "normal");
            m_2FAStatusLabel->style()->unpolish(m_2FAStatusLabel);
            m_2FAStatusLabel->style()->polish(m_2FAStatusLabel);
        }
        if (m_enable2FAButton) {
            m_enable2FAButton->show();
            m_enable2FAButton->setEnabled(true);
        }
        if (m_disable2FAButton) m_disable2FAButton->hide();
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

        // Step 2: Generate TOTP secret and get QR code URI
        Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
            g_currentUser,
            password.toStdString()
        );

        if (!setupData.success) {
            QMessageBox::warning(this, "Error",
                               QString("Failed to initialize 2FA: %1").arg(QString::fromStdString(setupData.errorMessage)));
            return;
        }

        // Step 3: Show QR code dialog for scanning
        QDialog setupDialog(this);
        setupDialog.setWindowTitle("Set Up Two-Factor Authentication");
        setupDialog.setModal(true);
        setupDialog.setMinimumWidth(400);

        QVBoxLayout *layout = new QVBoxLayout(&setupDialog);
        layout->setSpacing(15);
        layout->setContentsMargins(20, 20, 20, 20);

        // Instructions
        QLabel *instructionsLabel = new QLabel(
            "Scan this QR code with your authenticator app\n"
            "(Google Authenticator, Authy, Microsoft Authenticator, etc.)",
            &setupDialog
        );
        instructionsLabel->setAlignment(Qt::AlignCenter);
        instructionsLabel->setWordWrap(true);
        layout->addWidget(instructionsLabel);

        // QR Code
        QLabel *qrLabel = new QLabel(&setupDialog);
        qrLabel->setAlignment(Qt::AlignCenter);
        qrLabel->setMinimumSize(200, 200);
        
        // Generate QR code from the otpauth URI
        QR::QRData qrData;
        if (QR::GenerateQRCode(setupData.otpauthUri, qrData) && qrData.width > 0) {
            // Convert QR data to QImage
            int scale = 200 / qrData.width;
            if (scale < 1) scale = 1;
            int imgSize = qrData.width * scale;
            QImage qrImage(imgSize, imgSize, QImage::Format_RGB32);
            qrImage.fill(Qt::white);
            
            for (int y = 0; y < qrData.height; ++y) {
                for (int x = 0; x < qrData.width; ++x) {
                    if (qrData.data[y * qrData.width + x]) {
                        // Fill a scaled block
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
                            }
                        }
                    }
                }
            }
            qrLabel->setPixmap(QPixmap::fromImage(qrImage));
        } else {
            qrLabel->setText("QR code generation failed.\nUse manual entry below.");
        }
        layout->addWidget(qrLabel);

        // Manual entry option
        QLabel *manualLabel = new QLabel("Or enter this code manually:", &setupDialog);
        manualLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(manualLabel);

        // Secret key display with copy button
        QHBoxLayout *secretLayout = new QHBoxLayout();
        QLineEdit *secretEdit = new QLineEdit(&setupDialog);
        secretEdit->setText(QString::fromStdString(setupData.secretBase32));
        secretEdit->setReadOnly(true);
        secretEdit->setAlignment(Qt::AlignCenter);
        secretEdit->setFont(QFont("Courier", 11));
        secretLayout->addWidget(secretEdit);
        
        QPushButton *copyButton = new QPushButton("Copy", &setupDialog);
        copyButton->setMaximumWidth(60);
        connect(copyButton, &QPushButton::clicked, [secretEdit]() {
            QGuiApplication::clipboard()->setText(secretEdit->text());
        });
        secretLayout->addWidget(copyButton);
        layout->addLayout(secretLayout);

        // Verification code entry
        layout->addSpacing(10);
        QLabel *verifyLabel = new QLabel("Enter the 6-digit code from your app to verify:", &setupDialog);
        verifyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(verifyLabel);

        QLineEdit *codeEdit = new QLineEdit(&setupDialog);
        codeEdit->setMaxLength(6);
        codeEdit->setAlignment(Qt::AlignCenter);
        codeEdit->setPlaceholderText("000000");
        codeEdit->setFont(QFont("Courier", 16));
        layout->addWidget(codeEdit);

        // Buttons
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &setupDialog
        );
        buttonBox->button(QDialogButtonBox::Ok)->setText("Verify & Enable");
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &setupDialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &setupDialog, &QDialog::reject);

        if (setupDialog.exec() == QDialog::Accepted) {
            QString code = codeEdit->text().trimmed();
            
            if (code.isEmpty() || code.length() != 6) {
                QMessageBox::warning(this, "Invalid Code",
                                   "Please enter a valid 6-digit code.");
                return;
            }

            // Verify and enable 2FA
            Auth::AuthResponse confirmResult = Auth::ConfirmTwoFactorSetup(
                g_currentUser,
                code.toStdString()
            );

            if (confirmResult.success()) {
                // Show backup codes
                Auth::BackupCodesResult backupCodes = Auth::GetBackupCodes(
                    g_currentUser,
                    password.toStdString()
                );

                QString backupMessage = "Two-factor authentication has been enabled!\n\n";
                if (backupCodes.success && !backupCodes.codes.empty()) {
                    backupMessage += "Save these backup codes in a secure location:\n\n";
                    for (const auto &code : backupCodes.codes) {
                        backupMessage += QString::fromStdString(code) + "\n";
                    }
                    backupMessage += "\nEach code can only be used once.";
                }

                QMessageBox::information(this, "2FA Enabled", backupMessage);
                update2FAStatus();
            } else {
                QMessageBox::warning(this, "Verification Failed",
                                   QString("Invalid code: %1").arg(QString::fromStdString(confirmResult.message)));
            }
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
        // Create dialog for password and TOTP code
        QDialog disableDialog(this);
        disableDialog.setWindowTitle("Disable Two-Factor Authentication");
        disableDialog.setModal(true);
        disableDialog.setMinimumWidth(350);

        QVBoxLayout *layout = new QVBoxLayout(&disableDialog);
        layout->setSpacing(12);
        layout->setContentsMargins(20, 20, 20, 20);

        QLabel *instructionLabel = new QLabel(
            "Enter your password and current authenticator code\nto disable 2FA:",
            &disableDialog
        );
        instructionLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(instructionLabel);

        // Password field
        QLabel *passwordLabel = new QLabel("Password:", &disableDialog);
        layout->addWidget(passwordLabel);
        QLineEdit *passwordEdit = new QLineEdit(&disableDialog);
        passwordEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(passwordEdit);

        // TOTP code field
        QLabel *codeLabel = new QLabel("Authenticator Code:", &disableDialog);
        layout->addWidget(codeLabel);
        QLineEdit *codeEdit = new QLineEdit(&disableDialog);
        codeEdit->setMaxLength(6);
        codeEdit->setPlaceholderText("000000");
        layout->addWidget(codeEdit);

        // Backup code option
        QLabel *backupLabel = new QLabel(
            "<small>Lost your authenticator? <a href='backup'>Use a backup code</a></small>",
            &disableDialog
        );
        backupLabel->setTextFormat(Qt::RichText);
        backupLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(backupLabel);

        // Buttons
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &disableDialog
        );
        buttonBox->button(QDialogButtonBox::Ok)->setText("Disable 2FA");
        layout->addWidget(buttonBox);

        bool useBackupCode = false;
        connect(backupLabel, &QLabel::linkActivated, [&useBackupCode, codeLabel, codeEdit]() {
            useBackupCode = true;
            codeLabel->setText("Backup Code:");
            codeEdit->setMaxLength(8);
            codeEdit->setPlaceholderText("xxxxxxxx");
            codeEdit->clear();
        });

        connect(buttonBox, &QDialogButtonBox::accepted, &disableDialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &disableDialog, &QDialog::reject);

        if (disableDialog.exec() == QDialog::Accepted) {
            QString password = passwordEdit->text();
            QString code = codeEdit->text().trimmed();

            if (password.isEmpty()) {
                QMessageBox::warning(this, "Error", "Please enter your password.");
                return;
            }

            if (code.isEmpty()) {
                QMessageBox::warning(this, "Error", "Please enter your authenticator code.");
                return;
            }

            Auth::AuthResponse response;
            if (useBackupCode) {
                response = Auth::UseBackupCode(g_currentUser, code.toStdString());
            } else {
                response = Auth::DisableTwoFactor(
                    g_currentUser,
                    password.toStdString(),
                    code.toStdString()
                );
            }

            if (response.success()) {
                QMessageBox::information(this, "2FA Disabled",
                                       "Two-factor authentication has been disabled successfully.");
                update2FAStatus();
            } else {
                QMessageBox::warning(this, "Error",
                                    QString("Failed to disable 2FA: %1").arg(QString::fromStdString(response.message)));
                update2FAStatus();
            }
        }
    }
}

