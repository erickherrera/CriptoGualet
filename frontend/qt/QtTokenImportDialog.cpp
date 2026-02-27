#include "include/QtTokenImportDialog.h"
#include "include/QtThemeManager.h"

#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

QtTokenImportDialog::QtTokenImportDialog(QWidget* parent)
    : QDialog(parent),
      m_ethereumWallet(nullptr),
      m_themeManager(nullptr),
      m_addressInput(nullptr),
      m_nameLabel(nullptr),
      m_symbolLabel(nullptr),
      m_decimalsLabel(nullptr),
      m_nameValueLabel(nullptr),
      m_symbolValueLabel(nullptr),
      m_decimalsValueLabel(nullptr),
      m_statusLabel(nullptr),
      m_importButton(nullptr),
      m_cancelButton(nullptr) {
    setModal(true);

    // Resize to the entire screen for a fullscreen overlay effect
    if (auto* screen = QGuiApplication::primaryScreen()) {
        this->resize(screen->size());
    } else if (parent) {
        this->resize(parent->size());  // Fallback to parent size
    }

    // Remove default window decorations and enable translucency for a modern, floating card look
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUI();

    connect(m_addressInput, &QLineEdit::textChanged, this, &QtTokenImportDialog::onAddressChanged);
    connect(m_importButton, &QPushButton::clicked, this, [this]() {
        if (m_addressInput->text().isEmpty())
            return;
        m_importData = ImportData{m_addressInput->text().trimmed()};
        accept();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, &QtTokenImportDialog::reject);

    // This signal is used to get the result from the worker thread
    qRegisterMetaType<EthereumService::TokenInfo>("EthereumService::TokenInfo");
    connect(this, &QtTokenImportDialog::tokenInfoFetched, this,
            &QtTokenImportDialog::onTokenInfoFetched);

    // Initialize UI state
    m_statusLabel->setText("Enter a valid ERC20 contract address.");
    m_statusLabel->setProperty("statusType", "neutral");
}

QtTokenImportDialog::~QtTokenImportDialog() {
}

void QtTokenImportDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Fill the background with a semi-transparent color to create a "dimmed" effect
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
}

void QtTokenImportDialog::setupUI() {
    setWindowTitle("Import Token");
    setSizeGripEnabled(false);

    // Main container for the dialog content, styled as a floating card
    QFrame* card = new QFrame(this);
    card->setObjectName("card");

    // Make the card responsive
    card->setMinimumWidth(500);
    if (auto* screen = QGuiApplication::primaryScreen()) {
        int screenWidth = screen->size().width();
        card->setMaximumWidth(std::min(800, screenWidth / 2));
    } else {
        card->setMaximumWidth(600);
    }

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(16);

    // Header section with title and description
    m_titleLabel = new QLabel("Import ERC20 Token", this);
    m_titleLabel->setObjectName("title");
    cardLayout->addWidget(m_titleLabel);

    QLabel* descriptionLabel = new QLabel(
        "Enter a token contract address to add a custom ERC20 token to your wallet.", this);
    descriptionLabel->setObjectName("description");
    descriptionLabel->setWordWrap(true);
    cardLayout->addWidget(descriptionLabel);

    // Address input section with better visual grouping
    QFrame* addressSection = new QFrame(this);
    addressSection->setObjectName("inputSection");
    QVBoxLayout* addressSectionLayout = new QVBoxLayout(addressSection);
    addressSectionLayout->setContentsMargins(0, 0, 0, 0);
    addressSectionLayout->setSpacing(8);

    m_addressLabel = new QLabel("Contract Address", this);
    m_addressLabel->setObjectName("fieldLabel");
    addressSectionLayout->addWidget(m_addressLabel);

    m_addressInput = new QLineEdit(this);
    m_addressInput->setPlaceholderText("0x...");
    m_addressInput->setObjectName("addressInput");
    m_addressInput->setMaxLength(42);
    addressSectionLayout->addWidget(m_addressInput);

    cardLayout->addWidget(addressSection);

    // Help hint
    QLabel* hintLabel =
        new QLabel("💡 You can find contract addresses on Etherscan or token websites.", this);
    hintLabel->setObjectName("hintLabel");
    hintLabel->setWordWrap(true);
    cardLayout->addWidget(hintLabel);

    // Status label with better visual feedback
    m_statusLabel = new QLabel("", this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft);
    cardLayout->addWidget(m_statusLabel);

    // Token info display card - enhanced design
    m_infoFrame = new QFrame(this);
    m_infoFrame->setObjectName("infoCard");
    m_infoFrame->setContentsMargins(20, 16, 20, 20);
    QVBoxLayout* infoFrameLayout = new QVBoxLayout(m_infoFrame);
    infoFrameLayout->setSpacing(12);

    // Card header
    QLabel* infoHeaderLabel = new QLabel("Token Information", this);
    infoHeaderLabel->setObjectName("infoCardHeader");
    infoFrameLayout->addWidget(infoHeaderLabel);

    // Separator line
    QFrame* separator = new QFrame(this);
    separator->setObjectName("cardSeparator");
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    infoFrameLayout->addWidget(separator);

    QFormLayout* infoLayout = new QFormLayout();
    infoLayout->setSpacing(8);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_symbolLabel = new QLabel("Symbol", m_infoFrame);
    m_symbolLabel->setObjectName("infoLabel");
    m_symbolValueLabel = new QLabel("", m_infoFrame);
    m_symbolValueLabel->setObjectName("infoValue");
    infoLayout->addRow(m_symbolLabel, m_symbolValueLabel);

    m_nameLabel = new QLabel("Token Name", m_infoFrame);
    m_nameLabel->setObjectName("infoLabel");
    m_nameValueLabel = new QLabel("", m_infoFrame);
    m_nameValueLabel->setObjectName("infoValue");
    infoLayout->addRow(m_nameLabel, m_nameValueLabel);

    m_decimalsLabel = new QLabel("Decimals", m_infoFrame);
    m_decimalsLabel->setObjectName("infoLabel");
    m_decimalsValueLabel = new QLabel("", m_infoFrame);
    m_decimalsValueLabel->setObjectName("infoValue");
    infoLayout->addRow(m_decimalsLabel, m_decimalsValueLabel);

    infoFrameLayout->addLayout(infoLayout);
    cardLayout->addWidget(m_infoFrame);
    m_infoFrame->setVisible(false);

    cardLayout->addStretch();

    // Buttons with better spacing and hierarchy
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setMinimumWidth(100);

    m_importButton = new QPushButton("Import Token", this);
    m_importButton->setObjectName("importButton");
    m_importButton->setEnabled(false);
    m_importButton->setMinimumWidth(120);
    m_importButton->setDefault(true);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_importButton);
    cardLayout->addLayout(buttonLayout);

    // The main layout of the dialog centers the card
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(card, 0, Qt::AlignCenter);
    setLayout(mainLayout);

    // Add drop shadow for a floating effect
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(30);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    shadowEffect->setOffset(0, 0);
    card->setGraphicsEffect(shadowEffect);
}

void QtTokenImportDialog::setEthereumWallet(WalletAPI::EthereumWallet* ethereumWallet) {
    m_ethereumWallet = ethereumWallet;
}

void QtTokenImportDialog::setThemeManager(QtThemeManager* themeManager) {
    m_themeManager = themeManager;
    if (m_themeManager) {
        connect(m_themeManager, &QtThemeManager::themeChanged, this,
                &QtTokenImportDialog::applyTheme);
        applyTheme();
    }
}

std::optional<QtTokenImportDialog::ImportData> QtTokenImportDialog::getImportData() const {
    return m_importData;
}

void QtTokenImportDialog::onAddressChanged(const QString& address) {
    m_importButton->setEnabled(false);
    m_infoFrame->setVisible(false);
    m_nameValueLabel->setText("");
    m_symbolValueLabel->setText("");
    m_decimalsValueLabel->setText("");

    if (address.isEmpty()) {
        m_statusLabel->setText("Enter a valid ERC20 contract address.");
        m_statusLabel->setProperty("statusType", "neutral");
        m_addressInput->setProperty("errorState", false);
        applyTheme();
        return;
    }

    bool isValidFormat =
        m_ethereumWallet && m_ethereumWallet->ValidateAddress(address.toStdString());

    if (!isValidFormat) {
        m_statusLabel->setText("Invalid address format. Must be 42 characters starting with '0x'.");
        m_statusLabel->setProperty("statusType", "error");
        m_addressInput->setProperty("errorState", true);
        applyTheme();
        return;
    }

    m_addressInput->setProperty("errorState", false);
    m_statusLabel->setText("Validating address...");
    m_statusLabel->setProperty("statusType", "loading");
    applyTheme();

    fetchTokenInfo(address);
}

void QtTokenImportDialog::fetchTokenInfo(const QString& address) {
    if (address == m_lastQueriedAddress)
        return;
    m_lastQueriedAddress = address;

    m_statusLabel->setText("⏳ Fetching token information...");
    m_statusLabel->setProperty("statusType", "loading");
    applyTheme();

    // Disable input during fetch to prevent multiple requests
    m_addressInput->setEnabled(false);
    m_importButton->setEnabled(false);

    // Fetch token info in a background thread with slight delay for visual feedback
    (void)QtConcurrent::run([this, address]() {
        try {
            if (m_ethereumWallet) {
                auto tokenInfoOpt = m_ethereumWallet->GetTokenInfo(address.toStdString());
                emit tokenInfoFetched(tokenInfoOpt.has_value(), tokenInfoOpt.has_value()
                                                                    ? tokenInfoOpt.value()
                                                                    : EthereumService::TokenInfo{});
            } else {
                emit tokenInfoFetched(false, {});
            }
        } catch (const std::exception& e) {
            qDebug() << "Exception in fetchTokenInfo:" << e.what();
            emit tokenInfoFetched(false, {});
        } catch (...) {
            qDebug() << "Unknown exception in fetchTokenInfo";
            emit tokenInfoFetched(false, {});
        }
    });
}

void QtTokenImportDialog::onTokenInfoFetched(bool success,
                                             const EthereumService::TokenInfo& tokenInfo) {
    m_addressInput->setEnabled(true);

    if (success) {
        m_nameValueLabel->setText(QString::fromStdString(tokenInfo.name));
        m_symbolValueLabel->setText(QString::fromStdString(tokenInfo.symbol));
        m_decimalsValueLabel->setText(QString::number(tokenInfo.decimals));

        m_infoFrame->setVisible(true);
        m_statusLabel->setText("✓ Token information loaded successfully.");
        m_statusLabel->setProperty("statusType", "success");
        m_importButton->setEnabled(true);
        m_addressInput->setProperty("errorState", false);
    } else {
        m_statusLabel->setText(
            "✕ Could not fetch token information. Please verify the contract address.");
        m_statusLabel->setProperty("statusType", "error");
        m_infoFrame->setVisible(false);
        m_importButton->setEnabled(false);
        m_addressInput->setProperty("errorState", true);
    }

    applyTheme();
}

void QtTokenImportDialog::applyTheme() {
    if (!m_themeManager)
        return;

    const auto& tm = *m_themeManager;
    QString bg = tm.backgroundColor().name();
    QString surface = tm.surfaceColor().name();
    QString text = tm.textColor().name();
    QString accent = tm.accentColor().name();
    QString secondary = tm.secondaryColor().name();
    QString dimmed = tm.dimmedTextColor().name();
    QString error = tm.errorColor().name();
    QString success = tm.successColor().name();
    QString focusBorder = tm.focusBorderColor().name();
    QString subtitle = tm.subtitleColor().name();
    QString disabledText = tm.disabledTextColor().name();
    QString lightError = tm.lightError().name();
    QString lightPositive = tm.lightPositive().name();

    QColor accentColor = tm.accentColor();
    QColor secondaryColor = tm.secondaryColor();
    QColor surfaceColor = tm.surfaceColor();

    // Dialog is transparent, card gets the background color
    this->setStyleSheet("QDialog { background-color: transparent; }");

    // Card styling
    if (auto* card = findChild<QFrame*>("card")) {
        card->setStyleSheet(QString("QFrame#card {"
                                    "   background-color: %1;"
                                    "   border-radius: 16px;"
                                    "}")
                                .arg(bg));
    }

    // Title styling - prominent and bold
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("QLabel#title {"
                                            "   font-size: 24px;"
                                            "   font-weight: 700;"
                                            "   color: %1;"
                                            "   background-color: transparent;"
                                            "   padding-bottom: 4px;"
                                            "}")
                                        .arg(text));
    }

    // Description label styling
    for (auto* widget : findChildren<QLabel*>()) {
        if (widget->objectName() == "description") {
            widget->setStyleSheet(QString("QLabel#description {"
                                          "   font-size: 13px;"
                                          "   color: %1;"
                                          "   background-color: transparent;"
                                          "   padding-bottom: 8px;"
                                          "}")
                                      .arg(subtitle));
        }
    }

    // Field label styling
    if (m_addressLabel) {
        m_addressLabel->setStyleSheet(QString("QLabel#fieldLabel {"
                                              "   font-size: 13px;"
                                              "   font-weight: 600;"
                                              "   color: %1;"
                                              "   background-color: transparent;"
                                              "}")
                                          .arg(text));
    }

    // Input field styling with enhanced focus and error states
    if (m_addressInput) {
        QString currentStyle = m_addressInput->property("errorState").toBool()
                                   ? QString("border: 2px solid %1;").arg(error)
                                   : QString("border: 2px solid %1;").arg(secondary);

        m_addressInput->setStyleSheet(QString(R"(
            QLineEdit#addressInput {
                background-color: %1;
                color: %2;
                border: 2px solid %3;
                border-radius: 8px;
                padding: 12px 14px;
                font-size: 14px;
                font-family: %4;
                selection-background-color: %5;
            }
            QLineEdit#addressInput:focus {
                border-color: %6;
                background-color: %1;
            }
            QLineEdit#addressInput:disabled {
                background-color: %7;
                color: %8;
                border-color: %9;
            }
            QLineEdit#addressInput:disabled:hover {
                border-color: %9;
            }
        )")
                                          .arg(surface)
                                          .arg(text)
                                          .arg(currentStyle)
                                          .arg(tm.textFont().family())
                                          .arg(accent)
                                          .arg(focusBorder)
                                          .arg(surfaceColor.darker(120).name())
                                          .arg(disabledText)
                                          .arg(secondary));
    }

    // Hint label styling
    for (auto* widget : findChildren<QLabel*>()) {
        if (widget->objectName() == "hintLabel") {
            widget->setStyleSheet(QString("QLabel#hintLabel {"
                                          "   font-size: 12px;"
                                          "   color: %1;"
                                          "   background-color: transparent;"
                                          "   padding: 4px 0;"
                                          "}")
                                      .arg(subtitle));
        }
    }

    // Status label styling
    if (m_statusLabel) {
        QString statusColor = dimmed;
        QString statusBg = "transparent";
        if (m_statusLabel->property("statusType").toString() == "error") {
            statusColor = error;
            statusBg = lightError;
        } else if (m_statusLabel->property("statusType").toString() == "success") {
            statusColor = success;
            statusBg = lightPositive;
        } else if (m_statusLabel->property("statusType").toString() == "loading") {
            statusColor = accent;
        }

        m_statusLabel->setStyleSheet(QString("QLabel#statusLabel {"
                                             "   font-size: 13px;"
                                             "   color: %1;"
                                             "   background-color: %2;"
                                             "   padding: 8px 12px;"
                                             "   border-radius: 6px;"
                                             "}")
                                         .arg(statusColor)
                                         .arg(statusBg));
    }

    // Info card styling - enhanced with shadow and better borders
    if (m_infoFrame) {
        m_infoFrame->setStyleSheet(QString(R"(
            QFrame#infoCard {
                background-color: %1;
                border: 1px solid %2;
                border-radius: 12px;
            }
        )")
                                       .arg(surface)
                                       .arg(secondary));
    }

    // Info card header styling
    for (auto* widget : findChildren<QLabel*>()) {
        if (widget->objectName() == "infoCardHeader") {
            widget->setStyleSheet(QString("QLabel#infoCardHeader {"
                                          "   font-size: 15px;"
                                          "   font-weight: 600;"
                                          "   color: %1;"
                                          "   background-color: transparent;"
                                          "}")
                                      .arg(text));
        }
    }

    // Separator styling
    for (auto* widget : findChildren<QFrame*>()) {
        if (widget->objectName() == "cardSeparator") {
            widget->setStyleSheet(QString("QFrame#cardSeparator {"
                                          "   background-color: %1;"
                                          "   max-height: 1px;"
                                          "   border: none;"
                                          "}")
                                      .arg(secondary));
        }
    }

    // Info labels styling
    auto setInfoLabelStyle = [text](QLabel* label) {
        if (label) {
            label->setStyleSheet(QString("QLabel#infoLabel {"
                                         "   font-size: 13px;"
                                         "   font-weight: 600;"
                                         "   color: %1;"
                                         "   background-color: transparent;"
                                         "}")
                                     .arg(text));
        }
    };
    setInfoLabelStyle(m_nameLabel);
    setInfoLabelStyle(m_symbolLabel);
    setInfoLabelStyle(m_decimalsLabel);

    auto setInfoValueStyle = [text](QLabel* label) {
        if (label) {
            label->setStyleSheet(QString("QLabel#infoValue {"
                                         "   font-size: 13px;"
                                         "   font-weight: 400;"
                                         "   color: %1;"
                                         "   background-color: transparent;"
                                         "}")
                                     .arg(text));
        }
    };
    setInfoValueStyle(m_nameValueLabel);
    setInfoValueStyle(m_symbolValueLabel);
    setInfoValueStyle(m_decimalsValueLabel);

    // Determine contrasting text color for accent background
    int luminance =
        (0.299 * accentColor.red() + 0.587 * accentColor.green() + 0.114 * accentColor.blue());
    QString textOnAccent = (luminance > 128) ? "#000000" : "#FFFFFF";

    // Determine contrasting text color for disabled background
    int disabledLuminance = (0.299 * secondaryColor.red() + 0.587 * secondaryColor.green() +
                             0.114 * secondaryColor.blue());
    QString textOnDisabled = (disabledLuminance > 128) ? "#000000" : "#FFFFFF";

    // Primary button (Import) styling
    QString primaryButtonStyle = QString(R"(
        QPushButton#importButton {
            background-color: %1;
            color: %2;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            font-family: %3;
        }
        QPushButton#importButton:hover {
            background-color: %4;
        }
        QPushButton#importButton:pressed {
            background-color: %5;
        }
        QPushButton#importButton:disabled {
            background-color: %6;
            color: %7;
            border: none;
        }
    )")
                                     .arg(accent)
                                     .arg(textOnAccent)
                                     .arg(tm.buttonFont().family())
                                     .arg(accentColor.lighter(120).name())
                                     .arg(accentColor.darker(120).name())
                                     .arg(secondary)
                                     .arg(textOnDisabled);
    m_importButton->setStyleSheet(primaryButtonStyle);

    // Secondary button (Cancel) styling
    QString secondaryButtonStyle = QString(R"(
        QPushButton#cancelButton {
            background-color: transparent;
            color: %1;
            border: 2px solid %2;
            padding: 12px 24px;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            font-family: %3;
        }
        QPushButton#cancelButton:hover {
            background-color: %4;
            border-color: %5;
        }
        QPushButton#cancelButton:pressed {
            background-color: %6;
            border-color: %6;
        }
    )")
                                       .arg(text)
                                       .arg(secondary)
                                       .arg(tm.buttonFont().family())
                                       .arg(secondaryColor.lighter(110).name())
                                       .arg(secondaryColor.lighter(130).name())
                                       .arg(secondary);
    m_cancelButton->setStyleSheet(secondaryButtonStyle);
}