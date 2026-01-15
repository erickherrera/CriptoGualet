#include "include/QtTokenImportDialog.h"
#include "include/QtThemeManager.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <QFrame>

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
      m_cancelButton(nullptr)
{
    setupUI();
    connect(m_addressInput, &QLineEdit::textChanged, this, &QtTokenImportDialog::onAddressChanged);
    connect(m_importButton, &QPushButton::clicked, this, [this]() {
        if (m_addressInput->text().isEmpty()) return;
        m_importData = ImportData{m_addressInput->text().trimmed()};
        accept();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, &QtTokenImportDialog::reject);
    
    // This signal is used to get the result from the worker thread
    qRegisterMetaType<EthereumService::TokenInfo>("EthereumService::TokenInfo");
    connect(this, &QtTokenImportDialog::tokenInfoFetched, this, &QtTokenImportDialog::onTokenInfoFetched);
}

QtTokenImportDialog::~QtTokenImportDialog() {}

void QtTokenImportDialog::setupUI() {
    setWindowTitle("Import Token");
    setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // Title
    m_titleLabel = new QLabel("Import Custom Token", this);
    m_titleLabel->setObjectName("h2");
    mainLayout->addWidget(m_titleLabel);

    // Address input
    QVBoxLayout* addressLayout = new QVBoxLayout();
    addressLayout->setSpacing(5);
    m_addressLabel = new QLabel("Token Contract Address", this);
    m_addressLabel->setObjectName("inputLabel");
    m_addressInput = new QLineEdit(this);
    m_addressInput->setPlaceholderText("0x...");
    addressLayout->addWidget(m_addressLabel);
    addressLayout->addWidget(m_addressInput);
    mainLayout->addLayout(addressLayout);

    // Status label
    m_statusLabel = new QLabel("Enter a valid ERC20 contract address.", this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    // Token info display card
    m_infoFrame = new QFrame(this);
    m_infoFrame->setObjectName("infoCard");
    m_infoFrame->setContentsMargins(15, 15, 15, 15);
    QVBoxLayout* infoFrameLayout = new QVBoxLayout(m_infoFrame);
    QFormLayout* infoLayout = new QFormLayout();
    
    m_symbolLabel = new QLabel("Symbol:", m_infoFrame);
    m_symbolValueLabel = new QLabel("", m_infoFrame);
    infoLayout->addRow(m_symbolLabel, m_symbolValueLabel);

    m_nameLabel = new QLabel("Name:", m_infoFrame);
    m_nameValueLabel = new QLabel("", m_infoFrame);
    infoLayout->addRow(m_nameLabel, m_nameValueLabel);

    m_decimalsLabel = new QLabel("Decimals:", m_infoFrame);
    m_decimalsValueLabel = new QLabel("", m_infoFrame);
    infoLayout->addRow(m_decimalsLabel, m_decimalsValueLabel);
    
    infoFrameLayout->addLayout(infoLayout);
    mainLayout->addWidget(m_infoFrame);
    m_infoFrame->setVisible(false);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    m_cancelButton = new QPushButton("Cancel", this);
    m_importButton = new QPushButton("Import", this);
    m_importButton->setEnabled(false);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_importButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void QtTokenImportDialog::setEthereumWallet(WalletAPI::EthereumWallet* ethereumWallet) {
    m_ethereumWallet = ethereumWallet;
}

void QtTokenImportDialog::setThemeManager(QtThemeManager* themeManager) {
    m_themeManager = themeManager;
    if (m_themeManager) {
        connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtTokenImportDialog::applyTheme);
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
    m_statusLabel->setText("Enter a valid ERC20 contract address.");
    
    if (m_ethereumWallet && m_ethereumWallet->ValidateAddress(address.toStdString())) {
        m_statusLabel->setText("Validating address...");
        fetchTokenInfo(address);
    } else {
        m_statusLabel->setText("Invalid address format.");
    }
}

void QtTokenImportDialog::fetchTokenInfo(const QString& address) {
    if (address == m_lastQueriedAddress) return;
    m_lastQueriedAddress = address;

    m_statusLabel->setText("Fetching token information...");

    // Fetch token info in a background thread
    (void)QtConcurrent::run([this, address]() {
        if (m_ethereumWallet) {
            auto tokenInfoOpt = m_ethereumWallet->GetTokenInfo(address.toStdString());
            if (tokenInfoOpt.has_value()) {
                emit tokenInfoFetched(true, tokenInfoOpt.value());
            } else {
                emit tokenInfoFetched(false, {});
            }
        }
    });
}

void QtTokenImportDialog::onTokenInfoFetched(bool success, const EthereumService::TokenInfo& tokenInfo) {
    if (success) {
        m_nameValueLabel->setText(QString::fromStdString(tokenInfo.name));
        m_symbolValueLabel->setText(QString::fromStdString(tokenInfo.symbol));
        m_decimalsValueLabel->setText(QString::number(tokenInfo.decimals));
        
        m_infoFrame->setVisible(true);

        m_statusLabel->setText("Token information loaded.");
        m_importButton->setEnabled(true);
        m_addressInput->setStyleSheet(""); // Reset style
    } else {
        m_statusLabel->setText("Could not fetch token information. Check the contract address.");
        m_infoFrame->setVisible(false);
        m_importButton->setEnabled(false);
        if (m_themeManager) {
             m_addressInput->setStyleSheet(QString("border: 1px solid %1;").arg(m_themeManager->errorColor().name()));
        }
    }
}

void QtTokenImportDialog::applyTheme() {
    if (!m_themeManager) return;

    this->setStyleSheet(QString("QDialog { background-color: %1; }")
                        .arg(m_themeManager->backgroundColor().name()));
    
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;")
                                 .arg(m_themeManager->textColor().name()));
    }

    if (m_addressLabel) {
        m_addressLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(m_themeManager->textColor().name()));
    }
    
    if (m_addressInput) {
        m_addressInput->setStyleSheet(m_themeManager->getLineEditStyleSheet());
    }

    m_statusLabel->setStyleSheet(QString("color: %1;").arg(m_themeManager->dimmedTextColor().name()));

    if (m_infoFrame) {
        m_infoFrame->setStyleSheet(QString(
            "QFrame#infoCard {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 8px;"
            "}"
        ).arg(m_themeManager->surfaceColor().name()).arg(m_themeManager->secondaryColor().name()));
    }
    
    auto setLabelStyle = [this](QLabel* label) {
        if (label) label->setStyleSheet(QString("color: %1; font-weight: bold; background: transparent;")
                             .arg(m_themeManager->textColor().name()));
    };
    setLabelStyle(m_nameLabel);
    setLabelStyle(m_symbolLabel);
    setLabelStyle(m_decimalsLabel);

    auto setValueLabelStyle = [this](QLabel* label) {
        if (label) label->setStyleSheet(QString("color: %1; background: transparent;")
                            .arg(m_themeManager->textColor().name()));
    };
    setValueLabelStyle(m_nameValueLabel);
    setValueLabelStyle(m_symbolValueLabel);
    setValueLabelStyle(m_decimalsValueLabel);

    QString primaryButtonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:disabled {
            background-color: %4;
            color: %5;
        }
    )")
    .arg(m_themeManager->accentColor().name())
    .arg(m_themeManager->backgroundColor().name())
    .arg(m_themeManager->accentColor().darker(110).name())
    .arg(m_themeManager->secondaryColor().name())
    .arg(m_themeManager->disabledTextColor().name());
    m_importButton->setStyleSheet(primaryButtonStyle);

    QString secondaryButtonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %4;
            border-color: %4;
        }
    )")
    .arg(m_themeManager->surfaceColor().name())
    .arg(m_themeManager->textColor().name())
    .arg(m_themeManager->secondaryColor().name())
    .arg(m_themeManager->secondaryColor().name());
    m_cancelButton->setStyleSheet(secondaryButtonStyle);
}