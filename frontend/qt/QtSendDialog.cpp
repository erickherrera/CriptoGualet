// QtSendDialog.cpp
#include "QtSendDialog.h"
#include "QtThemeManager.h"
#include "../../backend/core/include/WalletAPI.h"

#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

QtSendDialog::QtSendDialog(ChainType chainType, double currentBalance, double price, QWidget* parent)
    : QDialog(parent),
      m_themeManager(&QtThemeManager::instance()),
      m_chainType(chainType),
      m_currentBalance(currentBalance),
      m_cryptoPrice(price),
      m_estimatedFeeSatoshis(DEFAULT_FEE_SATOSHIS),
      m_gasLimit(DEFAULT_GAS_LIMIT) {

    // Set window title based on chain type
    if (m_chainType == ChainType::BITCOIN) {
        setWindowTitle("Send Bitcoin");
    } else if (m_chainType == ChainType::LITECOIN) {
        setWindowTitle("Send Litecoin");
    } else {
        setWindowTitle("Send Ethereum");
    }

    setModal(true);
    setMinimumWidth(500);

    setupUI();
    applyTheme();

    // Connect theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSendDialog::applyTheme);
}

void QtSendDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(m_themeManager->standardSpacing());  // 16px (was 20)
    m_mainLayout->setContentsMargins(
        m_themeManager->generousMargin(),  // 32px (was 30)
        m_themeManager->generousMargin(),  // 32px (was 30)
        m_themeManager->generousMargin(),  // 32px (was 30)
        m_themeManager->generousMargin()   // 32px (was 30)
    );

    const bool isBitcoin = (m_chainType == ChainType::BITCOIN);
    const bool isLitecoin = (m_chainType == ChainType::LITECOIN);
    const bool isBitcoinLike = isBitcoin || isLitecoin;
    const QString coinSymbol = isBitcoin ? "BTC" : (isLitecoin ? "LTC" : "ETH");

    // === Recipient Section ===
    QGroupBox* recipientGroup = new QGroupBox("Recipient");
    QVBoxLayout* recipientLayout = new QVBoxLayout(recipientGroup);

    QString addrLabel = isBitcoin ? "Bitcoin Address:" : (isLitecoin ? "Litecoin Address:" : "Ethereum Address:");
    m_recipientLabel = new QLabel(addrLabel);
    recipientLayout->addWidget(m_recipientLabel);

    m_recipientInput = new QLineEdit();
    if (isBitcoin) {
        m_recipientInput->setPlaceholderText("Enter Bitcoin address (e.g., 1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa)");
    } else if (isLitecoin) {
        m_recipientInput->setPlaceholderText("Enter Litecoin address (e.g., LZKqVn1CnZUjz5pN1jLjE8Q6NR3q5jQX9e)");
    } else {
        m_recipientInput->setPlaceholderText("Enter Ethereum address (e.g., 0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb)");
    }
    recipientLayout->addWidget(m_recipientInput);

    m_recipientError = new QLabel();
    m_recipientError->setStyleSheet(QString("color: %1;").arg(m_themeManager->errorColor().name()));
    m_recipientError->setWordWrap(true);
    m_recipientError->hide();
    recipientLayout->addWidget(m_recipientError);

    m_mainLayout->addWidget(recipientGroup);

    // === Amount Section ===
    QGroupBox* amountGroup = new QGroupBox("Amount");
    QVBoxLayout* amountLayout = new QVBoxLayout(amountGroup);

    m_amountLabel = new QLabel(QString("Amount (%1):").arg(coinSymbol));
    amountLayout->addWidget(m_amountLabel);

    QHBoxLayout* amountInputLayout = new QHBoxLayout();
    m_amountInput = new QDoubleSpinBox();
    m_amountInput->setDecimals(isBitcoinLike ? 8 : 18);  // BTC/LTC: 8 decimals, ETH: 18 decimals
    m_amountInput->setMinimum(isBitcoinLike ? 0.00000001 : 0.000000000000000001);  // 1 satoshi/litoshi or 1 wei
    m_amountInput->setMaximum(m_currentBalance);
    m_amountInput->setSingleStep(isBitcoinLike ? 0.001 : 0.01);
    m_amountInput->setValue(isBitcoinLike ? 0.001 : 0.01);
    amountInputLayout->addWidget(m_amountInput);

    m_maxButton = new QPushButton("MAX");
    m_maxButton->setFixedWidth(60);
    m_maxButton->setCursor(Qt::PointingHandCursor);
    amountInputLayout->addWidget(m_maxButton);

    amountLayout->addLayout(amountInputLayout);

    m_amountUSD = new QLabel();
    m_amountUSD->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_themeManager->dimmedTextColor().name()));
    amountLayout->addWidget(m_amountUSD);

    m_amountError = new QLabel();
    m_amountError->setStyleSheet(QString("color: %1;").arg(m_themeManager->errorColor().name()));
    m_amountError->setWordWrap(true);
    m_amountError->hide();
    amountLayout->addWidget(m_amountError);

    m_mainLayout->addWidget(amountGroup);

    // === Ethereum Gas Controls ===
    if (!isBitcoinLike) {
        QGroupBox* gasGroup = new QGroupBox("Gas Settings");
        QVBoxLayout* gasLayout = new QVBoxLayout(gasGroup);

        // Gas Price Selection
        m_gasPriceLabel = new QLabel("Gas Price:");
        gasLayout->addWidget(m_gasPriceLabel);

        m_gasPriceCombo = new QComboBox();
        m_gasPriceCombo->addItem("Safe (Slower)", "safe");
        m_gasPriceCombo->addItem("Propose (Average)", "propose");
        m_gasPriceCombo->addItem("Fast", "fast");
        m_gasPriceCombo->setCurrentIndex(1);  // Default to "Propose"
        gasLayout->addWidget(m_gasPriceCombo);

        // Gas Limit
        m_gasLimitLabel = new QLabel("Gas Limit:");
        gasLayout->addWidget(m_gasLimitLabel);

        m_gasLimitInput = new QSpinBox();
        m_gasLimitInput->setMinimum(21000);  // Minimum for ETH transfer
        m_gasLimitInput->setMaximum(1000000);  // Reasonable maximum
        m_gasLimitInput->setValue(DEFAULT_GAS_LIMIT);
        m_gasLimitInput->setSingleStep(1000);
        gasLayout->addWidget(m_gasLimitInput);

        m_mainLayout->addWidget(gasGroup);

        // Connect gas controls
        connect(m_gasPriceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSendDialog::onGasPriceChanged);
        connect(m_gasLimitInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &QtSendDialog::onGasLimitChanged);
    } else {
        // Initialize to nullptr for Bitcoin
        m_gasPriceLabel = nullptr;
        m_gasPriceCombo = nullptr;
        m_gasLimitLabel = nullptr;
        m_gasLimitInput = nullptr;
    }

    // === Fee Section ===
    QHBoxLayout* feeLayout = new QHBoxLayout();
    m_feeLabel = new QLabel(isBitcoinLike ? "Network Fee:" : "Gas Fee:");
    m_feeValue = new QLabel();
    feeLayout->addWidget(m_feeLabel);
    feeLayout->addStretch();
    feeLayout->addWidget(m_feeValue);
    m_mainLayout->addLayout(feeLayout);

    // === Total Section ===
    QHBoxLayout* totalLayout = new QHBoxLayout();
    m_totalLabel = new QLabel("Total:");
    m_totalLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_totalValue = new QLabel();
    m_totalValue->setStyleSheet("font-weight: bold; font-size: 14px;");
    totalLayout->addWidget(m_totalLabel);
    totalLayout->addStretch();
    totalLayout->addWidget(m_totalValue);
    m_mainLayout->addLayout(totalLayout);

    // === Available Balance Section ===
    QHBoxLayout* availableLayout = new QHBoxLayout();
    m_availableLabel = new QLabel("Available Balance:");
    m_availableValue = new QLabel(QString("%1 %2").arg(formatCrypto(m_currentBalance)).arg(coinSymbol));
    availableLayout->addWidget(m_availableLabel);
    availableLayout->addStretch();
    availableLayout->addWidget(m_availableValue);
    m_mainLayout->addLayout(availableLayout);

    // === Summary Section ===
    m_summaryText = new QTextEdit();
    m_summaryText->setReadOnly(true);
    m_summaryText->setMaximumHeight(100);
    m_summaryText->hide();
    m_mainLayout->addWidget(m_summaryText);

    // === Buttons ===
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();

    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setFixedWidth(100);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_buttonLayout->addWidget(m_cancelButton);

    m_confirmButton = new QPushButton("Send");
    m_confirmButton->setFixedWidth(100);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setDefault(true);
    m_buttonLayout->addWidget(m_confirmButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // Connect signals
    connect(m_recipientInput, &QLineEdit::textChanged, this, &QtSendDialog::onRecipientAddressChanged);
    connect(m_amountInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QtSendDialog::onAmountChanged);
    connect(m_maxButton, &QPushButton::clicked, this, &QtSendDialog::onSendMaxClicked);
    connect(m_confirmButton, &QPushButton::clicked, this, &QtSendDialog::onConfirmClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QtSendDialog::onCancelClicked);

    // Initial update
    updateEstimates();
}

void QtSendDialog::applyTheme() {
    if (!m_themeManager) return;

    // Apply theme to dialog
    QString bgColor = m_themeManager->backgroundColor().name();
    QString textColor = m_themeManager->textColor().name();
    QString surfaceColor = m_themeManager->surfaceColor().name();
    QString accentColor = m_themeManager->accentColor().name();

    setStyleSheet(QString(R"(
        QDialog {
            background-color: %1;
            color: %2;
        }
        QGroupBox {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 8px;
            margin-top: 10px;
            padding: 15px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QLabel {
            color: %2;
        }
        QLineEdit, QDoubleSpinBox, QTextEdit {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 8px;
        }
        QLineEdit:focus, QDoubleSpinBox:focus {
            border: 2px solid %5;
        }
        QPushButton {
            background-color: %5;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %6;
        }
        QPushButton:pressed {
            background-color: %7;
        }
    )").arg(bgColor)
       .arg(textColor)
       .arg(surfaceColor)
       .arg(m_themeManager->surfaceColor().lighter(120).name())
       .arg(accentColor)
       .arg(m_themeManager->accentColor().lighter(110).name())
       .arg(m_themeManager->accentColor().darker(110).name()));
}

void QtSendDialog::onRecipientAddressChanged() {
    m_recipientError->hide();
    updateEstimates();
}

void QtSendDialog::onAmountChanged(double value) {
    Q_UNUSED(value);
    m_amountError->hide();
    updateEstimates();
}

void QtSendDialog::onSendMaxClicked() {
    double maxAmount = 0.0;

    if (m_chainType == ChainType::BITCOIN || m_chainType == ChainType::LITECOIN) {
        // Calculate max sendable amount (balance - fee)
        double fee = m_estimatedFeeSatoshis / 100000000.0;
        maxAmount = m_currentBalance - fee;
    } else {
        // For Ethereum, calculate gas cost and subtract from balance
        QString gasPrice = m_proposeGasPrice;  // Use currently selected gas price
        if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "safe") {
            gasPrice = m_safeGasPrice;
        } else if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "fast") {
            gasPrice = m_fastGasPrice;
        }

        if (!gasPrice.isEmpty()) {
            double gasPriceGwei = gasPrice.toDouble();
            double gasPriceEth = gasPriceGwei / 1000000000.0;  // Gwei to ETH
            double gasCostEth = gasPriceEth * m_gasLimit;
            maxAmount = m_currentBalance - gasCostEth;
        } else {
            // Fallback: estimate 0.001 ETH for gas
            maxAmount = m_currentBalance - 0.001;
        }
    }

    if (maxAmount <= 0) {
        m_amountError->setText("Insufficient balance to cover network fee");
        m_amountError->show();
        return;
    }

    m_amountInput->setValue(maxAmount);
}

void QtSendDialog::onConfirmClicked() {
    if (!validateInputs()) {
        return;
    }

    QString summary;
    double amount = m_amountInput->value();

    if (m_chainType == ChainType::BITCOIN || m_chainType == ChainType::LITECOIN) {
        // Bitcoin/Litecoin confirmation summary
        QString coinName = (m_chainType == ChainType::BITCOIN) ? "Bitcoin" : "Litecoin";
        QString coinSymbol = (m_chainType == ChainType::BITCOIN) ? "BTC" : "LTC";
        double fee = m_estimatedFeeSatoshis / 100000000.0;
        double totalAmount = amount + fee;

        summary = QString(
            "<b>%1 Transaction Summary</b><br><br>"
            "Recipient: %2<br>"
            "Amount: %3 %4 (%5)<br>"
            "Fee: %6 %7 (%8)<br>"
            "<b>Total: %9 %10 (%11)</b><br><br>"
            "Are you sure you want to send this transaction?"
        ).arg(coinName)
         .arg(m_recipientInput->text())
         .arg(formatCrypto(amount))
         .arg(coinSymbol)
         .arg(formatUSD(amount * m_cryptoPrice))
         .arg(formatCrypto(fee))
         .arg(coinSymbol)
         .arg(formatUSD(fee * m_cryptoPrice))
         .arg(formatCrypto(totalAmount))
         .arg(coinSymbol)
         .arg(formatUSD(totalAmount * m_cryptoPrice));
    } else {
        // Ethereum confirmation summary
        QString gasPrice = m_proposeGasPrice;
        if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "safe") {
            gasPrice = m_safeGasPrice;
        } else if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "fast") {
            gasPrice = m_fastGasPrice;
        }

        double gasPriceGwei = gasPrice.toDouble();
        double gasPriceEth = gasPriceGwei / 1000000000.0;  // Gwei to ETH
        double gasCostEth = gasPriceEth * m_gasLimit;
        double totalETH = amount + gasCostEth;

        summary = QString(
            "<b>Ethereum Transaction Summary</b><br><br>"
            "Recipient: %1<br>"
            "Amount: %2 ETH (%3)<br>"
            "Gas Price: %4 Gwei<br>"
            "Gas Limit: %5<br>"
            "Gas Fee: %6 ETH (%7)<br>"
            "<b>Total: %8 ETH (%9)</b><br><br>"
            "Are you sure you want to send this transaction?"
        ).arg(m_recipientInput->text())
         .arg(formatCrypto(amount))
         .arg(formatUSD(amount * m_cryptoPrice))
         .arg(gasPrice)
         .arg(m_gasLimit)
         .arg(formatCrypto(gasCostEth))
         .arg(formatUSD(gasCostEth * m_cryptoPrice))
         .arg(formatCrypto(totalETH))
         .arg(formatUSD(totalETH * m_cryptoPrice));
    }

    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("Confirm Transaction");
    confirmBox.setTextFormat(Qt::RichText);
    confirmBox.setText(summary);
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmBox.setDefaultButton(QMessageBox::No);
    confirmBox.setIcon(QMessageBox::Question);

    if (confirmBox.exec() != QMessageBox::Yes) {
        return;
    }

    // Request password for signing
    bool ok;
    QString password = QInputDialog::getText(
        this,
        "Authentication Required",
        "Enter your password to sign the transaction:",
        QLineEdit::Password,
        "",
        &ok
    );

    if (!ok || password.isEmpty()) {
        return;
    }

    // Create transaction data
    TransactionData txData;
    txData.recipientAddress = m_recipientInput->text();
    txData.password = password;

    if (m_chainType == ChainType::BITCOIN) {
        txData.amountBTC = amount;
        txData.amountSatoshis = static_cast<uint64_t>(amount * 100000000);
        txData.estimatedFeeSatoshis = m_estimatedFeeSatoshis;
        txData.totalSatoshis = txData.amountSatoshis + m_estimatedFeeSatoshis;

        // Initialize Litecoin and Ethereum fields to defaults
        txData.amountLTC = 0.0;
        txData.amountLitoshis = 0;
        txData.estimatedFeeLitoshis = 0;
        txData.totalLitoshis = 0;
        txData.amountETH = 0.0;
        txData.gasPriceGwei = "";
        txData.gasLimit = 0;
        txData.totalCostWei = "";
        txData.totalCostETH = 0.0;
    } else if (m_chainType == ChainType::LITECOIN) {
        txData.amountLTC = amount;
        txData.amountLitoshis = static_cast<uint64_t>(amount * 100000000);
        txData.estimatedFeeLitoshis = m_estimatedFeeSatoshis;  // Use same fee field
        txData.totalLitoshis = txData.amountLitoshis + m_estimatedFeeSatoshis;

        // Initialize Bitcoin and Ethereum fields to defaults
        txData.amountBTC = 0.0;
        txData.amountSatoshis = 0;
        txData.estimatedFeeSatoshis = 0;
        txData.totalSatoshis = 0;
        txData.amountETH = 0.0;
        txData.gasPriceGwei = "";
        txData.gasLimit = 0;
        txData.totalCostWei = "";
        txData.totalCostETH = 0.0;
    } else {
        // Ethereum transaction
        QString gasPrice = m_proposeGasPrice;
        if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "safe") {
            gasPrice = m_safeGasPrice;
        } else if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "fast") {
            gasPrice = m_fastGasPrice;
        }

        double gasPriceGwei = gasPrice.toDouble();
        double gasPriceEth = gasPriceGwei / 1000000000.0;
        double gasCostEth = gasPriceEth * m_gasLimit;
        double totalETH = amount + gasCostEth;

        txData.amountETH = amount;
        txData.gasPriceGwei = gasPrice;
        txData.gasLimit = m_gasLimit;
        txData.totalCostETH = totalETH;
        // Convert total cost to Wei (approximate - for display purposes)
        txData.totalCostWei = QString::number(static_cast<uint64_t>(totalETH * 1e18));

        // Initialize Bitcoin and Litecoin fields to defaults
        txData.amountBTC = 0.0;
        txData.amountSatoshis = 0;
        txData.estimatedFeeSatoshis = 0;
        txData.totalSatoshis = 0;
        txData.amountLTC = 0.0;
        txData.amountLitoshis = 0;
        txData.estimatedFeeLitoshis = 0;
        txData.totalLitoshis = 0;
    }

    m_transactionData = txData;

    accept();
}

void QtSendDialog::onCancelClicked() {
    reject();
}

void QtSendDialog::onGasPriceChanged(int index) {
    Q_UNUSED(index);
    updateEstimates();
}

void QtSendDialog::onGasLimitChanged(int value) {
    m_gasLimit = value;
    updateEstimates();
}

void QtSendDialog::updateEstimates() {
    double amount = m_amountInput->value();
    double amountUSD = amount * m_cryptoPrice;
    m_amountUSD->setText(QString("≈ %1").arg(formatUSD(amountUSD)));

    if (m_chainType == ChainType::BITCOIN || m_chainType == ChainType::LITECOIN) {
        // Bitcoin/Litecoin fee estimation
        QString coinSymbol = (m_chainType == ChainType::BITCOIN) ? "BTC" : "LTC";
        double fee = m_estimatedFeeSatoshis / 100000000.0;
        double feeUSD = fee * m_cryptoPrice;
        m_feeValue->setText(QString("%1 %2 (%3)").arg(formatCrypto(fee)).arg(coinSymbol).arg(formatUSD(feeUSD)));

        // Update total
        double total = amount + fee;
        double totalUSD = total * m_cryptoPrice;
        m_totalValue->setText(QString("%1 %2 (%3)").arg(formatCrypto(total)).arg(coinSymbol).arg(formatUSD(totalUSD)));
    } else {
        // Ethereum gas fee estimation
        QString gasPrice = m_proposeGasPrice;
        if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "safe") {
            gasPrice = m_safeGasPrice;
        } else if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "fast") {
            gasPrice = m_fastGasPrice;
        }

        if (!gasPrice.isEmpty()) {
            double gasPriceGwei = gasPrice.toDouble();
            double gasPriceEth = gasPriceGwei / 1000000000.0;  // Gwei to ETH
            double gasCostEth = gasPriceEth * m_gasLimit;
            double gasCostUSD = gasCostEth * m_cryptoPrice;

            m_feeValue->setText(QString("%1 Gwei × %2 = %3 ETH (%4)")
                .arg(gasPrice)
                .arg(m_gasLimit)
                .arg(formatCrypto(gasCostEth))
                .arg(formatUSD(gasCostUSD)));

            // Update total
            double totalETH = amount + gasCostEth;
            double totalUSD = totalETH * m_cryptoPrice;
            m_totalValue->setText(QString("%1 ETH (%2)").arg(formatCrypto(totalETH)).arg(formatUSD(totalUSD)));
        } else {
            m_feeValue->setText("Fetching gas prices...");
            m_totalValue->setText(QString("%1 ETH + fees").arg(formatCrypto(amount)));
        }
    }
}

bool QtSendDialog::validateInputs() {
    bool valid = true;

    // Validate recipient address
    QString recipientAddr = m_recipientInput->text().trimmed();
    if (recipientAddr.isEmpty()) {
        m_recipientError->setText("Please enter a recipient address");
        m_recipientError->show();
        valid = false;
    } else {
        if (m_chainType == ChainType::BITCOIN) {
            if (!validateBitcoinAddress(recipientAddr)) {
                m_recipientError->setText("Invalid Bitcoin address format");
                m_recipientError->show();
                valid = false;
            }
        } else if (m_chainType == ChainType::LITECOIN) {
            if (!validateLitecoinAddress(recipientAddr)) {
                m_recipientError->setText("Invalid Litecoin address format");
                m_recipientError->show();
                valid = false;
            }
        } else {
            if (!validateEthereumAddress(recipientAddr)) {
                m_recipientError->setText("Invalid Ethereum address format (must start with 0x and contain 40 hex characters)");
                m_recipientError->show();
                valid = false;
            }
        }
    }

    // Validate amount
    double amount = m_amountInput->value();
    if (amount <= 0) {
        m_amountError->setText("Amount must be greater than 0");
        m_amountError->show();
        valid = false;
    } else {
        if (m_chainType == ChainType::BITCOIN || m_chainType == ChainType::LITECOIN) {
            QString coinSymbol = (m_chainType == ChainType::BITCOIN) ? "BTC" : "LTC";
            double fee = m_estimatedFeeSatoshis / 100000000.0;
            double total = amount + fee;
            if (total > m_currentBalance) {
                m_amountError->setText(QString("Insufficient balance. You need %1 %2 (including fee)")
                    .arg(formatCrypto(total))
                    .arg(coinSymbol));
                m_amountError->show();
                valid = false;
            }
        } else {
            // Ethereum validation
            QString gasPrice = m_proposeGasPrice;
            if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "safe") {
                gasPrice = m_safeGasPrice;
            } else if (m_gasPriceCombo && m_gasPriceCombo->currentData().toString() == "fast") {
                gasPrice = m_fastGasPrice;
            }

            if (!gasPrice.isEmpty()) {
                double gasPriceGwei = gasPrice.toDouble();
                double gasPriceEth = gasPriceGwei / 1000000000.0;
                double gasCostEth = gasPriceEth * m_gasLimit;
                double totalETH = amount + gasCostEth;

                if (totalETH > m_currentBalance) {
                    m_amountError->setText(QString("Insufficient balance. You need %1 ETH (including gas)")
                        .arg(formatCrypto(totalETH)));
                    m_amountError->show();
                    valid = false;
                }
            }
        }
    }

    return valid;
}

std::optional<QtSendDialog::TransactionData> QtSendDialog::getTransactionData() const {
    return m_transactionData;
}

QString QtSendDialog::formatCrypto(double amount) const {
    if (m_chainType == ChainType::BITCOIN || m_chainType == ChainType::LITECOIN) {
        return QString::number(amount, 'f', 8);
    } else {
        // For Ethereum, use up to 18 decimals but remove trailing zeros
        QString formatted = QString::number(amount, 'f', 18);
        // Remove trailing zeros
        while (formatted.contains('.') && (formatted.endsWith('0') || formatted.endsWith('.'))) {
            if (formatted.endsWith('.')) {
                formatted.chop(1);
                break;
            }
            formatted.chop(1);
        }
        return formatted;
    }
}

QString QtSendDialog::formatUSD(double usd) const {
    return QString("$%L1").arg(usd, 0, 'f', 2);
}

bool QtSendDialog::validateBitcoinAddress(const QString& address) const {
    // Basic Bitcoin address validation
    // P2PKH addresses start with 1 and are 26-35 characters
    // P2SH addresses start with 3 and are 26-35 characters
    // Bech32 addresses start with bc1 and can be longer
    if (address.length() < 26 || address.length() > 90) {
        return false;
    }

    // Check if starts with valid prefix
    if (!address.startsWith('1') && !address.startsWith('3') && !address.startsWith("bc1")) {
        return false;
    }

    // Check for valid characters (Base58 for legacy, bech32 for segwit)
    if (address.startsWith("bc1")) {
        // Bech32 validation (simplified)
        QRegularExpression bech32Regex("^bc1[a-z0-9]{39,87}$", QRegularExpression::CaseInsensitiveOption);
        return bech32Regex.match(address).hasMatch();
    } else {
        // Base58 validation (no 0, O, I, l)
        QRegularExpression base58Regex("^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$");
        return base58Regex.match(address).hasMatch();
    }
}

bool QtSendDialog::validateLitecoinAddress(const QString& address) const {
    // Basic Litecoin address validation
    // Legacy addresses start with L or M and are 26-35 characters
    // P2SH addresses start with 3 and are 26-35 characters (same as Bitcoin)
    // Bech32 addresses start with ltc1
    if (address.length() < 26 || address.length() > 90) {
        return false;
    }

    // Check if starts with valid Litecoin prefix
    if (!address.startsWith('L') && !address.startsWith('M') && 
        !address.startsWith('3') && !address.startsWith("ltc1")) {
        return false;
    }

    // Check for valid characters
    if (address.startsWith("ltc1")) {
        // Bech32 validation (simplified)
        QRegularExpression bech32Regex("^ltc1[a-z0-9]{39,87}$", QRegularExpression::CaseInsensitiveOption);
        return bech32Regex.match(address).hasMatch();
    } else {
        // Base58 validation (no 0, O, I, l)
        QRegularExpression base58Regex("^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$");
        return base58Regex.match(address).hasMatch();
    }
}

bool QtSendDialog::validateEthereumAddress(const QString& address) const {
    // Ethereum address must:
    // 1. Start with 0x
    // 2. Be exactly 42 characters long (0x + 40 hex chars)
    // 3. Contain only hexadecimal characters after 0x

    if (!address.startsWith("0x")) {
        return false;
    }

    if (address.length() != 42) {
        return false;
    }

    // Check if remaining characters are valid hex
    QRegularExpression hexRegex("^0x[0-9a-fA-F]{40}$");
    return hexRegex.match(address).hasMatch();
}
