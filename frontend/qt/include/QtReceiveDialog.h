#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPixmap>
#include <QScrollArea>

// Forward declarations
class QtThemeManager;
class QResizeEvent;

/**
 * @brief Dialog for receiving Bitcoin and Ethereum payments
 *
 * Provides a user interface for:
 * - Displaying receiving address with QR code
 * - Copying address to clipboard
 * - Optionally including payment amount in QR code
 * - Supporting both Bitcoin and Ethereum address formats
 */
class QtReceiveDialog : public QDialog {
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
     * @brief Constructor
     * @param chainType Blockchain type (Bitcoin or Ethereum)
     * @param address Receiving address to display
     * @param parent Parent widget
     */
    explicit QtReceiveDialog(ChainType chainType, const QString& address, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onCopyAddressClicked();
    void onAmountChanged(double value);
    void onIncludeAmountToggled(bool checked);
    void onCloseClicked();

private:
    void setupUI();
    void applyTheme();
    void generateQRCode();
    void updateQRCode();
    void updateResponsiveLayout();
    QString getPaymentURI() const;
    QString formatCrypto(double amount) const;

private:
    QtThemeManager* m_themeManager;
    ChainType m_chainType;
    QString m_address;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;

    // Title section
    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;

    // QR Code section
    QLabel* m_qrCodeLabel;
    QLabel* m_qrStatusLabel;

    // Address section
    QLabel* m_addressLabel;
    QLineEdit* m_addressInput;
    QPushButton* m_copyButton;

    // Optional amount section
    QCheckBox* m_includeAmountCheckbox;
    QLabel* m_amountLabel;
    QDoubleSpinBox* m_amountInput;
    QLabel* m_amountNote;

    // Buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_closeButton;

    // QR Code data
    QPixmap m_qrPixmap;
    bool m_includeAmount;
    double m_requestAmount;

    // Constants
    static constexpr int QR_CODE_SIZE = 300;  // Display size in pixels
    static constexpr int QR_SCALE_FACTOR = 8;  // Scale factor for QR generation
};
