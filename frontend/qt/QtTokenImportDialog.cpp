#include "QtTokenImportDialog.h"
#include "QtThemeManager.h"
#include <QRegularExpression>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>

QtTokenImportDialog::QtTokenImportDialog(QWidget* parent)
    : QDialog(parent)
    , m_themeManager(nullptr)
    , m_ethereumWallet(nullptr)
    , m_isValidAddress(false)
    , m_hasTokenInfo(false)
{
    setupUI();
    setupPopularTokens();
    applyTheme();
}

std::optional<TokenImportData> QtTokenImportDialog::getImportData() const {
    return m_importData;
}

void QtTokenImportDialog::setEthereumWallet(WalletAPI::EthereumWallet* wallet) {
    m_ethereumWallet = wallet;
}

void QtTokenImportDialog::setThemeManager(QtThemeManager* themeManager) {
    m_themeManager = themeManager;
    applyTheme();
}

void QtTokenImportDialog::setupUI() {
    setWindowTitle("Import ERC20 Token");
    setMinimumWidth(500);
    setMaximumWidth(600);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_contentWidget = new QWidget();
    m_mainLayout = new QVBoxLayout(m_contentWidget);
    m_mainLayout->setSpacing(16);
    m_mainLayout->setContentsMargins(24, 24, 24, 24);

    m_titleLabel = new QLabel("Import ERC20 Token");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);

    m_subtitleLabel = new QLabel("Enter the contract address to import a custom token to your wallet.");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setWordWrap(true);

    m_addressInput = new QLineEdit();
    m_addressInput->setPlaceholderText("0x...");
    m_addressInput->setMaxLength(42);

    m_addressError = new QLabel();
    m_addressError->setWordWrap(true);
    m_addressError->hide();

    m_fetchButton = new QPushButton("Fetch Token Info");
    m_fetchButton->setEnabled(false);

    m_loadingBar = new QProgressBar();
    m_loadingBar->setRange(0, 0);
    m_loadingBar->setTextVisible(false);
    m_loadingBar->hide();

    m_previewTitle = new QLabel("Token Preview");
    m_previewWidget = new QWidget();
    QVBoxLayout* previewLayout = new QVBoxLayout(m_previewWidget);
    previewLayout->setContentsMargins(16, 16, 16, 16);

    m_tokenNameLabel = new QLabel();
    m_tokenSymbolLabel = new QLabel();
    m_tokenDecimalsLabel = new QLabel();
    m_tokenAddressLabel = new QLabel();

    previewLayout->addWidget(m_tokenNameLabel);
    previewLayout->addWidget(m_tokenSymbolLabel);
    previewLayout->addWidget(m_tokenDecimalsLabel);
    previewLayout->addWidget(m_tokenAddressLabel);

    m_previewWidget->hide();

    m_popularTokensTitle = new QLabel("Popular Tokens");
    m_popularTokensWidget = new QWidget();
    m_popularTokensLayout = new QVBoxLayout(m_popularTokensWidget);
    m_popularTokensLayout->setSpacing(8);

    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);

    m_importButton = new QPushButton("Import Token");
    m_importButton->setEnabled(false);

    m_cancelButton = new QPushButton("Cancel");

    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_importButton);

    m_mainLayout->addWidget(m_titleLabel);
    m_mainLayout->addWidget(m_subtitleLabel);
    m_mainLayout->addWidget(m_addressInput);
    m_mainLayout->addWidget(m_addressError);
    m_mainLayout->addWidget(m_fetchButton);
    m_mainLayout->addWidget(m_loadingBar);
    m_mainLayout->addWidget(m_previewTitle);
    m_mainLayout->addWidget(m_previewWidget);
    m_mainLayout->addWidget(m_popularTokensTitle);
    m_mainLayout->addWidget(m_popularTokensWidget);
    m_mainLayout->addStretch();
    m_mainLayout->addLayout(m_buttonLayout);

    m_previewTitle->hide();
    m_popularTokensTitle->hide();

    m_scrollArea->setWidget(m_contentWidget);

    QVBoxLayout* dialogLayout = new QVBoxLayout(this);
    dialogLayout->addWidget(m_scrollArea);
    dialogLayout->setContentsMargins(0, 0, 0, 0);

    connect(m_addressInput, &QLineEdit::textChanged, this, &QtTokenImportDialog::onAddressChanged);
    connect(m_fetchButton, &QPushButton::clicked, this, &QtTokenImportDialog::onFetchTokenInfo);
    connect(m_importButton, &QPushButton::clicked, this, &QtTokenImportDialog::onImportClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QtTokenImportDialog::onCancelClicked);
}

void QtTokenImportDialog::setupPopularTokens() {
    struct PopularToken {
        QString name;
        QString symbol;
        QString address;
        QString decimals;
    };

    QList<PopularToken> popularTokens = {
        {"USDT", "USDT", "0xdAC17F958D2ee523a2206206994597C13D831ec7", "6"},
        {"USDC", "USDC", "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48", "6"},
        {"DAI", "DAI", "0x6B175474E89094C44Da98b954EedeAC495271d0F", "18"},
        {"UNI", "UNI", "0x1f9840a85d5aF5bf1D1762F925BDADdC4201F984", "18"},
        {"LINK", "LINK", "0x514910771AF9Ca656af840dff83E8264EcF986CA", "18"},
        {"WBTC", "WBTC", "0x2260FAC5E5542a773Aa44fBCfeDf7C193bc2C599", "8"}
    };

    for (const auto& token : popularTokens) {
        QPushButton* tokenButton = new QPushButton(QString("%1 - %2").arg(token.symbol, token.name));
        tokenButton->setProperty("tokenAddress", token.address);
        connect(tokenButton, &QPushButton::clicked, this, [this, token]() {
            onSuggestedTokenClicked(token.address);
        });
        m_popularTokensLayout->addWidget(tokenButton);
    }

    if (!popularTokens.isEmpty()) {
        m_popularTokensTitle->show();
        m_popularTokensWidget->show();
    }
}

void QtTokenImportDialog::applyTheme() {
    if (!m_themeManager) {
        return;
    }

    QString baseStyle = m_themeManager->getMainWindowStyleSheet();
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
    QString buttonStyle = m_themeManager->getButtonStyleSheet();

    m_titleLabel->setStyleSheet(labelStyle + "QLabel { font-size: 20px; font-weight: bold; }");
    m_subtitleLabel->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; }");
    m_previewTitle->setStyleSheet(labelStyle + "QLabel { font-weight: bold; }");
    m_popularTokensTitle->setStyleSheet(labelStyle + "QLabel { font-weight: bold; }");

    m_addressInput->setStyleSheet(lineEditStyle);
    m_addressError->setStyleSheet("QLabel { color: " + m_themeManager->errorColor().name() + "; }");

    m_fetchButton->setStyleSheet(buttonStyle);
    m_importButton->setStyleSheet(buttonStyle);
    m_cancelButton->setStyleSheet(buttonStyle);

    m_previewWidget->setStyleSheet("QWidget { background-color: " + m_themeManager->surfaceColor().name() + "; border-radius: " + QString::number(m_themeManager->borderRadiusMedium()) + "px; }");

    m_tokenNameLabel->setStyleSheet(labelStyle);
    m_tokenSymbolLabel->setStyleSheet(labelStyle);
    m_tokenDecimalsLabel->setStyleSheet(labelStyle);
    m_tokenAddressLabel->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; }");

    m_scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");
    m_contentWidget->setStyleSheet("QWidget { background-color: " + m_themeManager->backgroundColor().name() + "; }");

    setStyleSheet(baseStyle);
}

void QtTokenImportDialog::onAddressChanged(const QString& text) {
    validateAddress(text);
    clearPreview();
    m_importButton->setEnabled(false);
}

void QtTokenImportDialog::validateAddress(const QString& address) {
    m_addressError->hide();

    if (address.isEmpty()) {
        m_isValidAddress = false;
        m_fetchButton->setEnabled(false);
        return;
    }

    if (!isValidEthereumAddress(address)) {
        m_addressError->setText("Invalid Ethereum address format. Must be 42 characters starting with '0x'.");
        m_addressError->show();
        m_isValidAddress = false;
        m_fetchButton->setEnabled(false);
        return;
    }

    m_isValidAddress = true;
    m_fetchButton->setEnabled(true);
}

bool QtTokenImportDialog::isValidEthereumAddress(const QString& address) const {
    QRegularExpression regex("^0x[a-fA-F0-9]{40}$");
    return regex.match(address).hasMatch() && address.length() == 42;
}

void QtTokenImportDialog::onFetchTokenInfo() {
    if (!m_ethereumWallet || !m_isValidAddress) {
        showError("Ethereum wallet is not available. Please try again later.");
        return;
    }

    QString address = m_addressInput->text().trimmed();

    showLoading(true);
    clearPreview();
    m_fetchButton->setEnabled(false);
    m_importButton->setEnabled(false);

    QTimer::singleShot(100, this, [this, address]() {
        auto tokenInfoOpt = m_ethereumWallet->GetTokenInfo(address.toStdString());

        showLoading(false);
        m_fetchButton->setEnabled(true);

        if (tokenInfoOpt) {
            const auto& info = tokenInfoOpt.value();
            showPreview(info);
            m_hasTokenInfo = true;
            m_importButton->setEnabled(true);
            emit tokenInfoFetched(info);
        } else {
            showError("Failed to fetch token information. Please verify the contract address and try again.");
            m_importButton->setEnabled(false);
            m_hasTokenInfo = false;
        }
    });
}

void QtTokenImportDialog::showPreview(const EthereumService::TokenInfo& info) {
    m_previewWidget->show();
    m_previewTitle->show();

    m_tokenNameLabel->setText(QString("Name: %1").arg(QString::fromStdString(info.name)));
    m_tokenSymbolLabel->setText(QString("Symbol: %1").arg(QString::fromStdString(info.symbol)));
    m_tokenDecimalsLabel->setText(QString("Decimals: %1").arg(info.decimals));
    m_tokenAddressLabel->setText(QString("Address: %1").arg(QString::fromStdString(info.contract_address)));
}

void QtTokenImportDialog::showError(const QString& message) {
    m_addressError->setText(message);
    m_addressError->show();
}

void QtTokenImportDialog::showLoading(bool loading) {
    if (loading) {
        m_loadingBar->show();
    } else {
        m_loadingBar->hide();
    }
}

void QtTokenImportDialog::clearPreview() {
    m_previewWidget->hide();
    m_previewTitle->hide();
    m_hasTokenInfo = false;
}

void QtTokenImportDialog::onSuggestedTokenClicked(const QString& contractAddress) {
    m_addressInput->setText(contractAddress);
    onFetchTokenInfo();
}

void QtTokenImportDialog::onImportClicked() {
    if (!m_hasTokenInfo || !m_isValidAddress) {
        return;
    }

    QString address = m_addressInput->text().trimmed();
    QString name = m_tokenNameLabel->text().replace("Name: ", "");
    QString symbol = m_tokenSymbolLabel->text().replace("Symbol: ", "");
    int decimals = m_tokenDecimalsLabel->text().replace("Decimals: ", "").toInt();

    m_importData = TokenImportData{
        address,
        name,
        symbol,
        decimals
    };

    accept();
}

void QtTokenImportDialog::onCancelClicked() {
    m_importData.reset();
    reject();
}
