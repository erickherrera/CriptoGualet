#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <optional>

// Forward declarations
class QtThemeManager;

/**
 * @brief Dialog for sending Bitcoin and Ethereum transactions
 *
 * Provides a user interface for:
 * - Entering recipient address
 * - Specifying amount to send
 * - Viewing fee/gas estimates
 * - Confirming transaction details
 */
class QtSendDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Blockchain type enumeration
     */
    enum class ChainType {
        BITCOIN,
        LITECOIN,
        ETHEREUM
    };

    /**
     * @brief Transaction data structure
     */
    struct TransactionData {
        QString recipientAddress;

        // Bitcoin-specific fields
        double amountBTC;
        uint64_t amountSatoshis;
        uint64_t estimatedFeeSatoshis;
        uint64_t totalSatoshis;

        // Litecoin-specific fields (similar to Bitcoin)
        double amountLTC;
        uint64_t amountLitoshis;  // 1 LTC = 100,000,000 litoshis
        uint64_t estimatedFeeLitoshis;
        uint64_t totalLitoshis;

        // Ethereum-specific fields
        double amountETH;
        QString gasPriceGwei;
        uint64_t gasLimit;
        QString totalCostWei;
        double totalCostETH;

        QString password;  // User password for signing
    };

    /**
     * @brief Constructor
     * @param chainType Blockchain type (Bitcoin or Ethereum)
     * @param currentBalance Current wallet balance (BTC or ETH)
     * @param price Current cryptocurrency price in USD
     * @param parent Parent widget
     */
    explicit QtSendDialog(ChainType chainType, double currentBalance, double price, QWidget* parent = nullptr);

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
    void onGasPriceChanged(int index);
    void onGasLimitChanged(int value);

private:
    void setupUI();
    void applyTheme();
    void updateEstimates();
    bool validateInputs();
    QString formatCrypto(double amount) const;
    QString formatUSD(double usd) const;
    bool validateBitcoinAddress(const QString& address) const;
    bool validateLitecoinAddress(const QString& address) const;
    bool validateEthereumAddress(const QString& address) const;

private:
    QtThemeManager* m_themeManager;
    ChainType m_chainType;

    // Balance info
    double m_currentBalance;  // In BTC or ETH
    double m_cryptoPrice;     // USD per BTC or ETH

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

    // Fee/Gas section (shared)
    QLabel* m_feeLabel;
    QLabel* m_feeValue;

    // Ethereum-specific gas controls
    QLabel* m_gasPriceLabel;
    QComboBox* m_gasPriceCombo;
    QLabel* m_gasLimitLabel;
    QSpinBox* m_gasLimitInput;

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

    // Bitcoin fee estimation
    uint64_t m_estimatedFeeSatoshis;
    static constexpr uint64_t DEFAULT_FEE_SATOSHIS = 10000;  // 0.0001 BTC default fee

    // Ethereum gas data
    QString m_safeGasPrice;
    QString m_proposeGasPrice;
    QString m_fastGasPrice;
    uint64_t m_gasLimit;
    static constexpr uint64_t DEFAULT_GAS_LIMIT = 21000;  // Standard ETH transfer
};
