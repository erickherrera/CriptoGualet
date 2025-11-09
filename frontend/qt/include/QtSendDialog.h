#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <optional>

// Forward declarations
class QtThemeManager;

/**
 * @brief Dialog for sending Bitcoin transactions
 *
 * Provides a user interface for:
 * - Entering recipient address
 * - Specifying amount to send
 * - Viewing fee estimates
 * - Confirming transaction details
 */
class QtSendDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Transaction data structure
     */
    struct TransactionData {
        QString recipientAddress;
        double amountBTC;
        uint64_t amountSatoshis;
        uint64_t estimatedFeeSatoshis;
        uint64_t totalSatoshis;
        QString password;  // User password for signing
    };

    /**
     * @brief Constructor
     * @param currentBalance Current wallet balance in BTC
     * @param btcPrice Current BTC price in USD
     * @param parent Parent widget
     */
    explicit QtSendDialog(double currentBalance, double btcPrice, QWidget* parent = nullptr);

    /**
     * @brief Get the transaction data if user confirmed
     * @return Transaction data if available
     */
    std::optional<TransactionData> getTransactionData() const;

private slots:
    void onRecipientAddressChanged();
    void onAmountChanged(double value);
    void onSendMaxClicked();
    void onConfirmClicked();
    void onCancelClicked();

private:
    void setupUI();
    void applyTheme();
    void updateEstimates();
    bool validateInputs();
    QString formatBTC(double btc) const;
    QString formatUSD(double usd) const;

private:
    QtThemeManager* m_themeManager;

    // Balance info
    double m_currentBalance;  // In BTC
    double m_btcPrice;        // USD per BTC

    // UI Components
    QVBoxLayout* m_mainLayout;

    // Recipient section
    QLabel* m_recipientLabel;
    QLineEdit* m_recipientInput;
    QLabel* m_recipientError;

    // Amount section
    QLabel* m_amountLabel;
    QDoubleSpinBox* m_amountInput;
    QPushButton* m_maxButton;
    QLabel* m_amountUSD;
    QLabel* m_amountError;

    // Fee section
    QLabel* m_feeLabel;
    QLabel* m_feeValue;

    // Total section
    QLabel* m_totalLabel;
    QLabel* m_totalValue;

    // Available balance section
    QLabel* m_availableLabel;
    QLabel* m_availableValue;

    // Summary section
    QTextEdit* m_summaryText;

    // Buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_confirmButton;
    QPushButton* m_cancelButton;

    // Transaction data
    std::optional<TransactionData> m_transactionData;

    // Fee estimation
    uint64_t m_estimatedFeeSatoshis;
    static constexpr uint64_t DEFAULT_FEE_SATOSHIS = 10000;  // 0.0001 BTC default fee
};
