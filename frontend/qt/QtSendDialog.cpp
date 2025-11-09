// QtSendDialog.cpp
#include "QtSendDialog.h"
#include "QtThemeManager.h"
#include "WalletAPI.h"

#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

QtSendDialog::QtSendDialog(double currentBalance, double btcPrice, QWidget* parent)
    : QDialog(parent),
      m_themeManager(&QtThemeManager::instance()),
      m_currentBalance(currentBalance),
      m_btcPrice(btcPrice),
      m_estimatedFeeSatoshis(DEFAULT_FEE_SATOSHIS) {

    setWindowTitle("Send Bitcoin");
    setModal(true);
    setMinimumWidth(500);

    setupUI();
    applyTheme();

    // Connect theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSendDialog::applyTheme);
}

void QtSendDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(30, 30, 30, 30);

    // === Recipient Section ===
    QGroupBox* recipientGroup = new QGroupBox("Recipient");
    QVBoxLayout* recipientLayout = new QVBoxLayout(recipientGroup);

    m_recipientLabel = new QLabel("Bitcoin Address:");
    recipientLayout->addWidget(m_recipientLabel);

    m_recipientInput = new QLineEdit();
    m_recipientInput->setPlaceholderText("Enter Bitcoin address (e.g., 1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa)");
    recipientLayout->addWidget(m_recipientInput);

    m_recipientError = new QLabel();
    m_recipientError->setStyleSheet("color: #ff4444;");
    m_recipientError->setWordWrap(true);
    m_recipientError->hide();
    recipientLayout->addWidget(m_recipientError);

    m_mainLayout->addWidget(recipientGroup);

    // === Amount Section ===
    QGroupBox* amountGroup = new QGroupBox("Amount");
    QVBoxLayout* amountLayout = new QVBoxLayout(amountGroup);

    m_amountLabel = new QLabel("Amount (BTC):");
    amountLayout->addWidget(m_amountLabel);

    QHBoxLayout* amountInputLayout = new QHBoxLayout();
    m_amountInput = new QDoubleSpinBox();
    m_amountInput->setDecimals(8);
    m_amountInput->setMinimum(0.00000001);  // 1 satoshi
    m_amountInput->setMaximum(m_currentBalance);
    m_amountInput->setSingleStep(0.001);
    m_amountInput->setValue(0.001);
    amountInputLayout->addWidget(m_amountInput);

    m_maxButton = new QPushButton("MAX");
    m_maxButton->setFixedWidth(60);
    m_maxButton->setCursor(Qt::PointingHandCursor);
    amountInputLayout->addWidget(m_maxButton);

    amountLayout->addLayout(amountInputLayout);

    m_amountUSD = new QLabel();
    m_amountUSD->setStyleSheet("color: #888888; font-size: 12px;");
    amountLayout->addWidget(m_amountUSD);

    m_amountError = new QLabel();
    m_amountError->setStyleSheet("color: #ff4444;");
    m_amountError->setWordWrap(true);
    m_amountError->hide();
    amountLayout->addWidget(m_amountError);

    m_mainLayout->addWidget(amountGroup);

    // === Fee Section ===
    QHBoxLayout* feeLayout = new QHBoxLayout();
    m_feeLabel = new QLabel("Network Fee:");
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
    m_availableValue = new QLabel(QString("%1 BTC").arg(m_currentBalance, 0, 'f', 8));
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
    m_amountError->hide();
    updateEstimates();
}

void QtSendDialog::onSendMaxClicked() {
    // Calculate max sendable amount (balance - fee)
    double feeBTC = m_estimatedFeeSatoshis / 100000000.0;
    double maxAmount = m_currentBalance - feeBTC;

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

    // Show confirmation summary
    double amountBTC = m_amountInput->value();
    double feeBTC = m_estimatedFeeSatoshis / 100000000.0;
    double totalBTC = amountBTC + feeBTC;

    QString summary = QString(
        "<b>Transaction Summary</b><br><br>"
        "Recipient: %1<br>"
        "Amount: %2 BTC (%3)<br>"
        "Fee: %4 BTC (%5)<br>"
        "<b>Total: %6 BTC (%7)</b><br><br>"
        "Are you sure you want to send this transaction?"
    ).arg(m_recipientInput->text())
     .arg(formatBTC(amountBTC))
     .arg(formatUSD(amountBTC * m_btcPrice))
     .arg(formatBTC(feeBTC))
     .arg(formatUSD(feeBTC * m_btcPrice))
     .arg(formatBTC(totalBTC))
     .arg(formatUSD(totalBTC * m_btcPrice));

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
    txData.amountBTC = amountBTC;
    txData.amountSatoshis = static_cast<uint64_t>(amountBTC * 100000000);
    txData.estimatedFeeSatoshis = m_estimatedFeeSatoshis;
    txData.totalSatoshis = txData.amountSatoshis + m_estimatedFeeSatoshis;
    txData.password = password;

    m_transactionData = txData;

    accept();
}

void QtSendDialog::onCancelClicked() {
    reject();
}

void QtSendDialog::updateEstimates() {
    // Update USD value
    double amountBTC = m_amountInput->value();
    double amountUSD = amountBTC * m_btcPrice;
    m_amountUSD->setText(QString("≈ %1").arg(formatUSD(amountUSD)));

    // Update fee
    double feeBTC = m_estimatedFeeSatoshis / 100000000.0;
    double feeUSD = feeBTC * m_btcPrice;
    m_feeValue->setText(QString("%1 BTC (%2)").arg(formatBTC(feeBTC)).arg(formatUSD(feeUSD)));

    // Update total
    double totalBTC = amountBTC + feeBTC;
    double totalUSD = totalBTC * m_btcPrice;
    m_totalValue->setText(QString("%1 BTC (%2)").arg(formatBTC(totalBTC)).arg(formatUSD(totalUSD)));
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
        // Basic address validation (could be more thorough)
        if (recipientAddr.length() < 26 || recipientAddr.length() > 35) {
            m_recipientError->setText("Invalid Bitcoin address format");
            m_recipientError->show();
            valid = false;
        }
    }

    // Validate amount
    double amountBTC = m_amountInput->value();
    if (amountBTC <= 0) {
        m_amountError->setText("Amount must be greater than 0");
        m_amountError->show();
        valid = false;
    } else {
        double feeBTC = m_estimatedFeeSatoshis / 100000000.0;
        double totalBTC = amountBTC + feeBTC;
        if (totalBTC > m_currentBalance) {
            m_amountError->setText(QString("Insufficient balance. You need %1 BTC (including fee)")
                .arg(formatBTC(totalBTC)));
            m_amountError->show();
            valid = false;
        }
    }

    return valid;
}

std::optional<QtSendDialog::TransactionData> QtSendDialog::getTransactionData() const {
    return m_transactionData;
}

QString QtSendDialog::formatBTC(double btc) const {
    return QString::number(btc, 'f', 8);
}

QString QtSendDialog::formatUSD(double usd) const {
    return QString("$%L1").arg(usd, 0, 'f', 2);
}
