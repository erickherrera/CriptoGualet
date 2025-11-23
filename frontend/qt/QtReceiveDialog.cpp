// QtReceiveDialog.cpp
#include "QtReceiveDialog.h"
#include "QtThemeManager.h"
#include "QRGenerator.h"

#include <QGroupBox>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QTimer>

QtReceiveDialog::QtReceiveDialog(ChainType chainType, const QString& address, QWidget* parent)
    : QDialog(parent),
      m_themeManager(&QtThemeManager::instance()),
      m_chainType(chainType),
      m_address(address),
      m_includeAmount(false),
      m_requestAmount(0.0) {

    // Set window title based on chain type
    if (m_chainType == ChainType::BITCOIN) {
        setWindowTitle("Receive Bitcoin");
    } else {
        setWindowTitle("Receive Ethereum");
    }

    setModal(true);
    setMinimumWidth(500);
    setMinimumHeight(650);

    setupUI();
    applyTheme();
    generateQRCode();

    // Connect theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtReceiveDialog::applyTheme);
}

void QtReceiveDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(m_themeManager->standardSpacing());  // 16px
    m_mainLayout->setContentsMargins(
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin()   // 32px
    );

    const bool isBitcoin = (m_chainType == ChainType::BITCOIN);
    const QString coinSymbol = isBitcoin ? "BTC" : "ETH";
    const QString coinName = isBitcoin ? "Bitcoin" : "Ethereum";

    // === Title Section ===
    m_titleLabel = new QLabel(QString("Receive %1").arg(coinName));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_mainLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("Share this address to receive payments");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_themeManager->dimmedTextColor().name()));
    m_mainLayout->addWidget(m_subtitleLabel);

    m_mainLayout->addSpacing(m_themeManager->standardSpacing());

    // === QR Code Section ===
    QGroupBox* qrGroup = new QGroupBox("QR Code");
    QVBoxLayout* qrLayout = new QVBoxLayout(qrGroup);
    qrLayout->setAlignment(Qt::AlignCenter);

    m_qrCodeLabel = new QLabel();
    m_qrCodeLabel->setAlignment(Qt::AlignCenter);
    m_qrCodeLabel->setMinimumSize(QR_CODE_SIZE, QR_CODE_SIZE);
    m_qrCodeLabel->setMaximumSize(QR_CODE_SIZE, QR_CODE_SIZE);
    m_qrCodeLabel->setScaledContents(false);
    qrLayout->addWidget(m_qrCodeLabel);

    m_qrStatusLabel = new QLabel();
    m_qrStatusLabel->setAlignment(Qt::AlignCenter);
    m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->dimmedTextColor().name()));
    m_qrStatusLabel->hide();
    qrLayout->addWidget(m_qrStatusLabel);

    m_mainLayout->addWidget(qrGroup);

    // === Address Section ===
    QGroupBox* addressGroup = new QGroupBox("Address");
    QVBoxLayout* addressLayout = new QVBoxLayout(addressGroup);

    m_addressLabel = new QLabel(QString("%1 Address:").arg(coinName));
    addressLayout->addWidget(m_addressLabel);

    QHBoxLayout* addressInputLayout = new QHBoxLayout();
    m_addressInput = new QLineEdit(m_address);
    m_addressInput->setReadOnly(true);
    m_addressInput->setFont(m_themeManager->monoFont());
    addressInputLayout->addWidget(m_addressInput);

    m_copyButton = new QPushButton("Copy");
    m_copyButton->setFixedWidth(80);
    m_copyButton->setCursor(Qt::PointingHandCursor);
    m_copyButton->setToolTip("Copy address to clipboard");
    addressInputLayout->addWidget(m_copyButton);

    addressLayout->addLayout(addressInputLayout);

    m_mainLayout->addWidget(addressGroup);

    // === Optional Amount Section ===
    QGroupBox* amountGroup = new QGroupBox("Payment Request (Optional)");
    QVBoxLayout* amountLayout = new QVBoxLayout(amountGroup);

    m_includeAmountCheckbox = new QCheckBox("Include amount in QR code");
    m_includeAmountCheckbox->setChecked(false);
    amountLayout->addWidget(m_includeAmountCheckbox);

    m_amountLabel = new QLabel(QString("Request Amount (%1):").arg(coinSymbol));
    m_amountLabel->setEnabled(false);
    amountLayout->addWidget(m_amountLabel);

    m_amountInput = new QDoubleSpinBox();
    m_amountInput->setDecimals(isBitcoin ? 8 : 18);  // BTC: 8 decimals, ETH: 18 decimals
    m_amountInput->setMinimum(isBitcoin ? 0.00000001 : 0.000000000000000001);  // 1 satoshi or 1 wei
    m_amountInput->setMaximum(1000000.0);  // Reasonable maximum
    m_amountInput->setSingleStep(isBitcoin ? 0.001 : 0.01);
    m_amountInput->setValue(isBitcoin ? 0.001 : 0.01);
    m_amountInput->setEnabled(false);
    amountLayout->addWidget(m_amountInput);

    m_amountNote = new QLabel(
        "When you include an amount, the QR code will contain a payment request URI. "
        "Compatible wallets will automatically fill in the amount when scanning."
    );
    m_amountNote->setWordWrap(true);
    m_amountNote->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->dimmedTextColor().name()));
    m_amountNote->setEnabled(false);
    amountLayout->addWidget(m_amountNote);

    m_mainLayout->addWidget(amountGroup);

    // === Buttons ===
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();

    m_closeButton = new QPushButton("Close");
    m_closeButton->setFixedWidth(100);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setDefault(true);
    m_buttonLayout->addWidget(m_closeButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // Connect signals
    connect(m_copyButton, &QPushButton::clicked, this, &QtReceiveDialog::onCopyAddressClicked);
    connect(m_includeAmountCheckbox, &QCheckBox::toggled, this, &QtReceiveDialog::onIncludeAmountToggled);
    connect(m_amountInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QtReceiveDialog::onAmountChanged);
    connect(m_closeButton, &QPushButton::clicked, this, &QtReceiveDialog::onCloseClicked);
}

void QtReceiveDialog::applyTheme() {
    if (!m_themeManager) return;

    // Apply theme to dialog
    QString bgColor = m_themeManager->backgroundColor().name();
    QString textColor = m_themeManager->textColor().name();
    QString surfaceColor = m_themeManager->surfaceColor().name();
    QString accentColor = m_themeManager->accentColor().name();
    QString borderColor = m_themeManager->surfaceColor().lighter(120).name();

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
        QLineEdit, QDoubleSpinBox {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 8px;
        }
        QLineEdit:focus, QDoubleSpinBox:focus {
            border: 2px solid %5;
        }
        QLineEdit:read-only {
            background-color: %6;
            color: %2;
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
            background-color: %7;
        }
        QPushButton:pressed {
            background-color: %8;
        }
        QPushButton:disabled {
            background-color: %9;
            color: %10;
        }
        QCheckBox {
            color: %2;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 3px;
            border: 2px solid %4;
            background-color: %3;
        }
        QCheckBox::indicator:checked {
            background-color: %5;
            border-color: %5;
        }
        QCheckBox::indicator:hover {
            border-color: %5;
        }
    )").arg(bgColor)
       .arg(textColor)
       .arg(surfaceColor)
       .arg(borderColor)
       .arg(accentColor)
       .arg(surfaceColor.replace("surfaceColor", m_themeManager->surfaceColor().darker(105).name()))
       .arg(m_themeManager->accentColor().lighter(110).name())
       .arg(m_themeManager->accentColor().darker(110).name())
       .arg(m_themeManager->surfaceColor().darker(120).name())
       .arg(m_themeManager->dimmedTextColor().name()));

    // Regenerate QR code with theme-appropriate colors
    generateQRCode();
}

void QtReceiveDialog::onCopyAddressClicked() {
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_address);

    // Show confirmation
    m_qrStatusLabel->setText("Address copied to clipboard!");
    m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->successColor().name()));
    m_qrStatusLabel->show();

    // Optional: Change button text temporarily
    QString originalText = m_copyButton->text();
    m_copyButton->setText("Copied!");
    m_copyButton->setEnabled(false);

    // Reset after 2 seconds
    QTimer::singleShot(2000, this, [this, originalText]() {
        m_copyButton->setText(originalText);
        m_copyButton->setEnabled(true);
        m_qrStatusLabel->hide();
    });
}

void QtReceiveDialog::onAmountChanged(double value) {
    m_requestAmount = value;
    if (m_includeAmount) {
        updateQRCode();
    }
}

void QtReceiveDialog::onIncludeAmountToggled(bool checked) {
    m_includeAmount = checked;
    m_amountLabel->setEnabled(checked);
    m_amountInput->setEnabled(checked);
    m_amountNote->setEnabled(checked);

    // Update QR code when toggled
    updateQRCode();
}

void QtReceiveDialog::onCloseClicked() {
    accept();
}

void QtReceiveDialog::generateQRCode() {
    QString qrContent = getPaymentURI();

    // Use QRGenerator to create QR code
    QR::QRData qrData;
    bool success = QR::GenerateQRCode(qrContent.toStdString(), qrData);

    if (!success || qrData.width == 0 || qrData.height == 0) {
        m_qrStatusLabel->setText("Error: Could not generate QR code");
        m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->errorColor().name()));
        m_qrStatusLabel->show();
        return;
    }

    // Convert QRData to QImage
    QImage qrImage(qrData.width, qrData.height, QImage::Format_RGB32);

    // Get theme colors for QR code
    QColor bgColor = m_themeManager->backgroundColor();
    QColor fgColor = m_themeManager->textColor();

    for (int y = 0; y < qrData.height; ++y) {
        for (int x = 0; x < qrData.width; ++x) {
            int index = y * qrData.width + x;
            if (index < static_cast<int>(qrData.data.size())) {
                // QR data: 0 = white (background), 1 = black (foreground)
                QColor pixelColor = (qrData.data[index] & 1) ? fgColor : bgColor;
                qrImage.setPixel(x, y, pixelColor.rgb());
            }
        }
    }

    // Scale up the QR code for better visibility
    QImage scaledQrImage = qrImage.scaled(
        QR_CODE_SIZE,
        QR_CODE_SIZE,
        Qt::KeepAspectRatio,
        Qt::FastTransformation
    );

    // Convert to pixmap and add white border
    int borderSize = 20;
    QPixmap finalPixmap(QR_CODE_SIZE + 2 * borderSize, QR_CODE_SIZE + 2 * borderSize);
    finalPixmap.fill(bgColor);

    QPainter painter(&finalPixmap);
    painter.drawImage(borderSize, borderSize, scaledQrImage);
    painter.end();

    m_qrPixmap = finalPixmap;
    m_qrCodeLabel->setPixmap(m_qrPixmap);
    m_qrStatusLabel->hide();
}

void QtReceiveDialog::updateQRCode() {
    // Regenerate QR code when content changes
    generateQRCode();
}

QString QtReceiveDialog::getPaymentURI() const {
    if (!m_includeAmount || m_requestAmount <= 0.0) {
        // Just return the plain address
        return m_address;
    }

    // Generate payment URI with amount
    if (m_chainType == ChainType::BITCOIN) {
        // Bitcoin URI: bitcoin:address?amount=value
        return QString("bitcoin:%1?amount=%2")
            .arg(m_address)
            .arg(formatCrypto(m_requestAmount));
    } else {
        // Ethereum URI: ethereum:address?value=value_in_wei
        // Convert ETH to Wei (1 ETH = 10^18 Wei)
        // For simplicity, we'll use the plain address for Ethereum
        // A full implementation would convert to Wei and use proper URI format
        return QString("ethereum:%1").arg(m_address);
    }
}

QString QtReceiveDialog::formatCrypto(double amount) const {
    if (m_chainType == ChainType::BITCOIN) {
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
