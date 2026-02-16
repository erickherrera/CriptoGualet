// QtReceiveDialog.cpp
#include "QtReceiveDialog.h"
#include "QtThemeManager.h"
#include "../../backend/utils/include/QRGenerator.h"

#include <QGroupBox>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QScreen>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QTimer>

QtReceiveDialog::QtReceiveDialog(ChainType chainType, const QString& address, QWidget* parent)
    : QDialog(parent),
      m_themeManager(&QtThemeManager::instance()),
      m_chainType(chainType),
      m_address(address),
      m_scrollArea(nullptr),
      m_scrollContent(nullptr),
      m_contentLayout(nullptr),
      m_includeAmount(false),
      m_requestAmount(0.0) {

    // Set window title based on chain type
    if (m_chainType == ChainType::BITCOIN) {
        setWindowTitle("Receive Bitcoin");
    } else if (m_chainType == ChainType::LITECOIN) {
        setWindowTitle("Receive Litecoin");
    } else {
        setWindowTitle("Receive Ethereum");
    }

    setModal(true);
    
    // Set responsive initial size
    int screenWidth = QApplication::primaryScreen()->geometry().width();
    int screenHeight = QApplication::primaryScreen()->geometry().height();
    
    int targetWidth = qMin(550, screenWidth - 40);
    int targetHeight = qMin(750, screenHeight - 80);
    
    resize(targetWidth, targetHeight);
    setMinimumWidth(qMin(320, screenWidth));
    setMinimumHeight(qMin(400, screenHeight));

    setupUI();
    applyTheme();
    generateQRCode();

    // Connect theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtReceiveDialog::applyTheme);
}

void QtReceiveDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create Scroll Area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setSpacing(m_themeManager->standardSpacing());  // 16px
    m_contentLayout->setContentsMargins(
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin(),  // 32px
        m_themeManager->generousMargin()   // 32px
    );

    const bool isBitcoin = (m_chainType == ChainType::BITCOIN);
    const bool isLitecoin = (m_chainType == ChainType::LITECOIN);
    const bool isBitcoinLike = isBitcoin || isLitecoin;
    const QString coinSymbol = isBitcoin ? "BTC" : (isLitecoin ? "LTC" : "ETH");
    const QString coinName = isBitcoin ? "Bitcoin" : (isLitecoin ? "Litecoin" : "Ethereum");

    // === Title Section ===
    m_titleLabel = new QLabel(QString("Receive %1").arg(coinName));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_contentLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("Share this address to receive payments");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_themeManager->dimmedTextColor().name()));
    m_contentLayout->addWidget(m_subtitleLabel);

    m_contentLayout->addSpacing(m_themeManager->standardSpacing());

    // === QR Code Section ===
    QGroupBox* qrGroup = new QGroupBox("QR Code");
    QVBoxLayout* qrLayout = new QVBoxLayout(qrGroup);
    qrLayout->setAlignment(Qt::AlignCenter);

    m_qrCodeLabel = new QLabel();
    m_qrCodeLabel->setAlignment(Qt::AlignCenter);
    m_qrCodeLabel->setMinimumSize(100, 100);
    m_qrCodeLabel->setScaledContents(true);
    qrLayout->addWidget(m_qrCodeLabel);

    m_qrStatusLabel = new QLabel();
    m_qrStatusLabel->setAlignment(Qt::AlignCenter);
    m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->dimmedTextColor().name()));
    m_qrStatusLabel->hide();
    qrLayout->addWidget(m_qrStatusLabel);

    m_contentLayout->addWidget(qrGroup);

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

    m_contentLayout->addWidget(addressGroup);

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
    m_amountInput->setDecimals(isBitcoinLike ? 8 : 18);  // BTC/LTC: 8 decimals, ETH: 18 decimals
    m_amountInput->setMinimum(isBitcoinLike ? 0.00000001 : 0.000000000000000001);  // 1 satoshi/litoshi or 1 wei
    m_amountInput->setMaximum(1000000.0);  // Reasonable maximum
    m_amountInput->setSingleStep(isBitcoinLike ? 0.001 : 0.01);
    m_amountInput->setValue(isBitcoinLike ? 0.001 : 0.01);
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

    m_contentLayout->addWidget(amountGroup);

    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);

    // === Buttons ===
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setContentsMargins(
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin()
    );
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
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: %1;
            width: 10px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: %4;
            border-radius: 5px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
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

    // Style the scroll content background
    if (m_scrollContent) {
        m_scrollContent->setStyleSheet(QString("QWidget { background-color: %1; }").arg(bgColor));
    }

    // Regenerate QR code with theme-appropriate colors
    generateQRCode();
}

void QtReceiveDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    updateResponsiveLayout();
}

void QtReceiveDialog::updateResponsiveLayout() {
    int dialogWidth = width();
    
    // Adjust content margins based on width
    if (m_contentLayout) {
        int margin = dialogWidth < 450 ? 15 : 32;
        m_contentLayout->setContentsMargins(margin, margin, margin, margin);
    }
    
    // Adjust QR code size if necessary
    if (m_qrCodeLabel) {
        int margins = m_contentLayout ? m_contentLayout->contentsMargins().left() + m_contentLayout->contentsMargins().right() : 64;
        int availableWidth = dialogWidth - margins - 40; // 40 for groupbox padding etc
        int targetSize = qMin(QR_CODE_SIZE + 40, availableWidth);
        
        if (targetSize > 100) {
            m_qrCodeLabel->setFixedSize(targetSize, targetSize);
        }
    }
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

    // If we have no data at all, then it's a real failure
    if (qrData.width == 0 || qrData.height == 0 || qrData.data.empty()) {
        m_qrStatusLabel->setText("Error: Could not generate QR code");
        m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->errorColor().name()));
        m_qrStatusLabel->show();
        return;
    }

    // Convert QRData to QImage
    QImage qrImage(qrData.width, qrData.height, QImage::Format_RGB32);

    // Get theme colors for QR code
    // For QR codes, we usually want high contrast
    QColor bgColor = Qt::white;
    QColor fgColor = Qt::black;

    for (int y = 0; y < qrData.height; ++y) {
        for (int x = 0; x < qrData.width; ++x) {
            int index = y * qrData.width + x;
            if (index < static_cast<int>(qrData.data.size())) {
                // QR data: 0 = black, 255 = white (as per QRGenerator.cpp)
                uint8_t pixelValue = qrData.data[index];
                QColor pixelColor = (pixelValue < 128) ? fgColor : bgColor;
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
    finalPixmap.fill(Qt::white); // Always white background for QR codes for better scanning

    QPainter painter(&finalPixmap);
    painter.drawImage(borderSize, borderSize, scaledQrImage);
    painter.end();

    m_qrPixmap = finalPixmap;
    m_qrCodeLabel->setPixmap(m_qrPixmap);
    
    if (!success) {
        m_qrStatusLabel->setText("Using fallback pattern (libqrencode not available)");
        m_qrStatusLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(m_themeManager->warningColor().name()));
        m_qrStatusLabel->show();
    } else {
        m_qrStatusLabel->hide();
    }
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
    } else if (m_chainType == ChainType::LITECOIN) {
        // Litecoin URI: litecoin:address?amount=value
        return QString("litecoin:%1?amount=%2")
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
