#include "../include/QtSeedDisplayDialog.h"
#include "../include/QRGenerator.h"
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QTimer>
#include <sstream>
#include <iomanip>

QtSeedDisplayDialog::QtSeedDisplayDialog(const std::vector<std::string>& seedWords, QWidget* parent)
    : QDialog(parent), m_seedWords(seedWords) {
    setWindowTitle("Backup Your Seed Phrase");
    setModal(true);
    setMinimumSize(500, 600);  // Optimized for laptop screens - taller than wide
    resize(500, 700);  // Default size for laptops

    setupUI();
    setupSeedDisplay();
    generateQRCode();  // Generate QR code immediately
}

void QtSeedDisplayDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(5);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    // Title - compact
    QLabel* titleLabel = new QLabel("🔐 BACKUP YOUR SEED PHRASE");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);  // Smaller for laptop screens
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #d32f2f; padding: 5px;");
    m_mainLayout->addWidget(titleLabel);

    // QR Code Section - Main Component
    QGroupBox* qrGroup = new QGroupBox("📱 QR Code - Scan to backup on mobile");
    qrGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #007bff; }");
    QVBoxLayout* qrLayout = new QVBoxLayout(qrGroup);
    qrLayout->setContentsMargins(10, 15, 10, 10);

    m_qrLabel = new QLabel();
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(150, 150);  // Half size QR code
    m_qrLabel->setMaximumSize(200, 200);  // Half size QR code
    m_qrLabel->setStyleSheet("border: 2px solid #007bff; background-color: white; border-radius: 8px;");
    m_qrLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Center the QR code in the layout
    qrLayout->addWidget(m_qrLabel, 0, Qt::AlignHCenter);

    m_mainLayout->addWidget(qrGroup);

    // Create scroll area for the word list
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setMaximumHeight(200);  // Limit height to force scrolling
    m_scrollArea->setStyleSheet("QScrollArea { border: none; }");

    m_scrollContent = new QWidget();
    m_scrollLayout = new QVBoxLayout(m_scrollContent);
    m_scrollLayout->setContentsMargins(5, 5, 5, 5);

    // Warning - compact
    QLabel* warningLabel = new QLabel("⚠️ Write these words on paper and store safely!");
    warningLabel->setWordWrap(true);
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setStyleSheet("background-color: #fff3cd; color: #856404; padding: 8px; border: 1px solid #ffeaa7; border-radius: 4px; font-size: 11px;");
    m_scrollLayout->addWidget(warningLabel);

    // Word Grid Section
    m_wordGroup = new QGroupBox("12-Word Seed Phrase (Copy-Paste Ready):");
    m_wordGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* wordLayout = new QVBoxLayout(m_wordGroup);

    createWordGrid();
    wordLayout->addLayout(m_wordGrid);

    // Full text version for easy copying
    m_seedTextEdit = new QTextEdit();
    m_seedTextEdit->setReadOnly(true);
    m_seedTextEdit->setMaximumHeight(60);  // Compact
    QFont monoFont("Consolas", 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_seedTextEdit->setFont(monoFont);
    m_seedTextEdit->setStyleSheet("background-color: #1e1e1e; color: white; border: 1px solid #555555; padding: 5px; border-radius: 4px;");
    wordLayout->addWidget(m_seedTextEdit);

    // Copy button for easy clipboard access
    m_copyButton = new QPushButton("📋 Copy All Words");
    m_copyButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; padding: 8px 12px; border: none; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #138496; }");
    connect(m_copyButton, &QPushButton::clicked, this, &QtSeedDisplayDialog::onCopyToClipboard);
    wordLayout->addWidget(m_copyButton);

    m_scrollLayout->addWidget(m_wordGroup);

    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);

    // Confirmation section - always visible at bottom
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line);

    m_confirmCheckbox = new QCheckBox("✅ I have safely stored my seed phrase");
    m_confirmCheckbox->setStyleSheet("QCheckBox { font-weight: bold; padding: 5px; }");
    m_mainLayout->addWidget(m_confirmCheckbox);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_confirmButton = new QPushButton("✓ Continue");
    m_confirmButton->setEnabled(false);
    m_confirmButton->setStyleSheet("QPushButton { background-color: #007bff; color: white; padding: 10px 16px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover:enabled { background-color: #0056b3; } QPushButton:disabled { background-color: #cccccc; }");
    connect(m_confirmButton, &QPushButton::clicked, this, &QtSeedDisplayDialog::onConfirmBackup);
    connect(m_confirmCheckbox, &QCheckBox::toggled, m_confirmButton, &QPushButton::setEnabled);

    QPushButton* cancelButton = new QPushButton("Cancel");
    cancelButton->setStyleSheet("QPushButton { background-color: #6c757d; color: white; padding: 10px 16px; border: none; border-radius: 5px; } QPushButton:hover { background-color: #545b62; }");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(m_confirmButton);

    m_mainLayout->addLayout(buttonLayout);
}

void QtSeedDisplayDialog::createWordGrid() {
    m_wordGrid = new QGridLayout();
    m_wordGrid->setSpacing(8);

    // Create a nice grid layout with numbered words
    int cols = 3;  // 3 columns for compact layout
    for (size_t i = 0; i < m_seedWords.size(); ++i) {
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;

        QLabel* wordLabel = new QLabel(QString("%1. %2")
            .arg(i + 1, 2, 10, QChar('0'))  // Zero-padded number
            .arg(QString::fromStdString(m_seedWords[i])));

        wordLabel->setStyleSheet(
            "QLabel { "
            "background-color: #2c2c2c; "
            "color: white; "
            "border: 1px solid #555555; "
            "border-radius: 4px; "
            "padding: 8px; "
            "font-family: 'Consolas', monospace; "
            "font-size: 11px; "
            "font-weight: bold; "
            "}"
        );
        wordLabel->setAlignment(Qt::AlignCenter);
        wordLabel->setMinimumHeight(30);

        m_wordGrid->addWidget(wordLabel, row, col);
    }
}

void QtSeedDisplayDialog::setupSeedDisplay() {
    // Create single-line copy-paste friendly format
    std::ostringstream oss;
    for (size_t i = 0; i < m_seedWords.size(); ++i) {
        if (i > 0) oss << " ";
        oss << m_seedWords[i];
    }

    m_seedTextEdit->setPlainText(QString::fromStdString(oss.str()));
}

void QtSeedDisplayDialog::onShowQRCode() {
    // This method is no longer needed since QR code is shown immediately
    // But keeping it for compatibility if referenced elsewhere
}

void QtSeedDisplayDialog::generateQRCode() {
    std::ostringstream seedText;
    for (size_t i = 0; i < m_seedWords.size(); ++i) {
        if (i > 0) seedText << " ";
        seedText << m_seedWords[i];
    }

    QR::QRData qrData;
    bool qrSuccess = QR::GenerateQRCode(seedText.str(), qrData);

    if (qrData.width > 0 && qrData.height > 0 && !qrData.data.empty()) {
        QImage qrImage(qrData.width, qrData.height, QImage::Format_RGB888);

        for (int y = 0; y < qrData.height; ++y) {
            for (int x = 0; x < qrData.width; ++x) {
                uint8_t pixel = qrData.data[y * qrData.width + x];
                QColor color(pixel, pixel, pixel);
                qrImage.setPixelColor(x, y, color);
            }
        }

        // Scale to fit the available space (laptop friendly)
        // Use a multiple of the original QR module size for crisp rendering
        int moduleSize = 4;  // 4 pixels per QR module for crisp edges
        int scaledSize = qrData.width * moduleSize;

        // Ensure it fits within our target size
        int targetSize = 150;
        if (scaledSize > targetSize) {
            // Find the largest multiple that fits
            moduleSize = targetSize / qrData.width;
            if (moduleSize < 1) moduleSize = 1;
            scaledSize = qrData.width * moduleSize;
        }

        QImage scaledImage = qrImage.scaled(
            scaledSize, scaledSize,
            Qt::KeepAspectRatio,
            Qt::FastTransformation  // Use fast transformation for pixel-perfect scaling
        );

        // Add white padding around the QR code for better scanning
        int padding = 8;
        int finalSize = scaledSize + (padding * 2);
        QImage paddedImage(finalSize, finalSize, QImage::Format_RGB888);
        paddedImage.fill(Qt::white);

        // Draw the QR code centered in the padded image
        for (int y = 0; y < scaledSize; ++y) {
            for (int x = 0; x < scaledSize; ++x) {
                paddedImage.setPixelColor(x + padding, y + padding, scaledImage.pixelColor(x, y));
            }
        }

        m_qrPixmap = QPixmap::fromImage(paddedImage);
        m_qrGenerated = true;

        // Display the QR code immediately
        m_qrLabel->setPixmap(m_qrPixmap);

        // Add helpful text overlay if it's a fallback pattern
        if (!qrSuccess) {
            m_qrLabel->setText("Fallback Pattern\n(Not a real QR code)\nCopy text below instead");
            m_qrLabel->setStyleSheet(m_qrLabel->styleSheet() + "color: #856404; font-weight: bold;");
        }
        return;
    }

    // Complete failure - show helpful message
    m_qrGenerated = false;
    m_qrLabel->setText("QR Code Generation Failed\n\n📝 Please copy the words\nbelow manually");
    m_qrLabel->setStyleSheet(m_qrLabel->styleSheet() + "color: #d32f2f; font-weight: bold; font-size: 14px;");
}

void QtSeedDisplayDialog::onCopyToClipboard() {
    // Copy the seed phrase to clipboard
    std::ostringstream oss;
    for (size_t i = 0; i < m_seedWords.size(); ++i) {
        if (i > 0) oss << " ";
        oss << m_seedWords[i];
    }

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(QString::fromStdString(oss.str()));

    // Temporarily change button text to show feedback
    m_copyButton->setText("✅ Copied!");
    m_copyButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; padding: 8px 12px; border: none; border-radius: 4px; font-weight: bold; }");

    // Reset button after 2 seconds
    QTimer::singleShot(2000, [this]() {
        m_copyButton->setText("📋 Copy All Words");
        m_copyButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; padding: 8px 12px; border: none; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #138496; }");
    });
}

void QtSeedDisplayDialog::onConfirmBackup() {
    if (m_confirmCheckbox->isChecked()) {
        m_userConfirmed = true;
        accept();
    }
}